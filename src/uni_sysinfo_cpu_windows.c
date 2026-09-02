#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winreg.h>

#include "uni_sysinfo_cpu_internal.h"

#include <stdlib.h>
#include <string.h>

struct uni_sysinfo_cpu_affinity {
  GROUP_AFFINITY previous;
};

typedef struct {
  WORD group;
  BYTE number;
  size_t package_index;
  size_t core_index;
} uni_sysinfo_windows_location_t;

typedef struct {
  ULONG number;
  ULONG max_mhz;
  ULONG current_mhz;
  ULONG mhz_limit;
  ULONG max_idle_state;
  ULONG current_idle_state;
} uni_sysinfo_windows_power_info_t;

typedef LONG(WINAPI *uni_sysinfo_windows_call_nt_power_information_t)(int, PVOID, ULONG, PVOID, ULONG);

static int uni_sysinfo_windows_valid_record(const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *entry, size_t remaining,
                                            LOGICAL_PROCESSOR_RELATIONSHIP relationship) {
  const size_t header_size = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor);
  size_t required_size;

  if (remaining < header_size || entry->Size < header_size || entry->Size > remaining ||
      entry->Relationship != relationship) {
    return 0;
  }
  if (relationship == RelationProcessorPackage || relationship == RelationProcessorCore) {
    required_size = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor) +
                    offsetof(PROCESSOR_RELATIONSHIP, GroupMask) +
                    (size_t)entry->Processor.GroupCount * sizeof(GROUP_AFFINITY);
    return entry->Processor.GroupCount > 0 && required_size <= entry->Size;
  }
  if (relationship == RelationCache) {
    required_size = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Cache) + sizeof(CACHE_RELATIONSHIP);
    return required_size <= entry->Size;
  }
  if (relationship == RelationNumaNode) {
    required_size = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, NumaNode) + sizeof(NUMA_NODE_RELATIONSHIP);
    return required_size <= entry->Size;
  }
  return 0;
}

static PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX uni_sysinfo_windows_query(LOGICAL_PROCESSOR_RELATIONSHIP relationship,
                                                                          DWORD *result_length) {
  DWORD length = 0;
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer;

  (void)GetLogicalProcessorInformationEx(relationship, NULL, &length);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0) {
    return NULL;
  }
  buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(length);
  if (buffer == NULL) {
    return NULL;
  }
  if (!GetLogicalProcessorInformationEx(relationship, buffer, &length)) {
    free(buffer);
    return NULL;
  }
  *result_length = length;
  return buffer;
}

static uni_sysinfo_windows_location_t *uni_sysinfo_windows_find(uni_sysinfo_windows_location_t *locations,
                                                                size_t location_count, WORD group, BYTE number) {
  size_t index;
  for (index = 0; index < location_count; ++index) {
    if (locations[index].group == group && locations[index].number == number) {
      return &locations[index];
    }
  }
  return NULL;
}

static uni_sysinfo_cpu_architecture_e uni_sysinfo_windows_architecture(WORD architecture) {
  if (architecture == PROCESSOR_ARCHITECTURE_INTEL || architecture == PROCESSOR_ARCHITECTURE_AMD64) {
    return UNI_SYSINFO_CPU_ARCHITECTURE_X86;
  }
#ifdef PROCESSOR_ARCHITECTURE_ARM64
  if (architecture == PROCESSOR_ARCHITECTURE_ARM64)
    return UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64;
#endif
#ifdef PROCESSOR_ARCHITECTURE_ARM
  if (architecture == PROCESSOR_ARCHITECTURE_ARM)
    return UNI_SYSINFO_CPU_ARCHITECTURE_ARM;
#endif
  return UNI_SYSINFO_CPU_ARCHITECTURE_UNKNOWN;
}

static void uni_sysinfo_windows_identity(char *name, size_t name_capacity,
                                         uni_sysinfo_cpu_architecture_e *host_architecture,
                                         uni_sysinfo_cpu_vendor_e *vendor) {
  SYSTEM_INFO native_info;
  uni_sysinfo_cpu_architecture_e process_architecture = uni_sysinfo_cpu_process_architecture();

  GetNativeSystemInfo(&native_info);
  *host_architecture = uni_sysinfo_windows_architecture(native_info.wProcessorArchitecture);
  if (process_architecture == UNI_SYSINFO_CPU_ARCHITECTURE_X86) {
    uni_sysinfo_cpu_x86_detect(name, name_capacity, vendor, NULL);
  } else {
    *vendor = UNI_SYSINFO_CPU_VENDOR_UNKNOWN;
    name[0] = '\0';
  }
  if (name[0] == '\0') {
    DWORD length   = (DWORD)name_capacity;
    LSTATUS status = RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                                  "ProcessorNameString", RRF_RT_REG_SZ, NULL, name, &length);
    if (status != ERROR_SUCCESS) {
      lstrcpynA(name, "Unknown CPU", (int)name_capacity);
    }
  }
}

static void uni_sysinfo_windows_apply_cache(uni_sysinfo_cpu_core_info_t *core, const CACHE_RELATIONSHIP *cache) {
  size_t size = (size_t)cache->CacheSize;
  if (cache->Level == 1 && (cache->Type == CacheData || cache->Type == CacheUnified) && size > core->cache.l1d_size) {
    core->cache.l1d_size = size;
  }
  if (cache->Level == 1 && (cache->Type == CacheInstruction || cache->Type == CacheUnified) &&
      size > core->cache.l1i_size) {
    core->cache.l1i_size = size;
  }
  if (cache->Level == 2 && size > core->cache.l2_size)
    core->cache.l2_size = size;
  if (cache->Level == 3 && size > core->cache.l3_size)
    core->cache.l3_size = size;
}

static BYTE uni_sysinfo_windows_efficiency_class(const PROCESSOR_RELATIONSHIP *relationship) {
  /* Windows 10 repurposed the first byte of Win7's Reserved[21] as
     EfficiencyClass without changing the binary layout. */
  return ((const BYTE *)relationship)[sizeof(relationship->Flags)];
}

static void uni_sysinfo_windows_apply_max_frequency(uni_sysinfo_cpu_snapshot_t *snapshot,
                                                    const uni_sysinfo_windows_location_t *locations,
                                                    size_t location_count) {
  HMODULE module;
  FARPROC address;
  uni_sysinfo_windows_call_nt_power_information_t query;
  uni_sysinfo_windows_power_info_t *information;
  size_t information_size;
  size_t location_index;

  if (GetActiveProcessorGroupCount() != 1 || location_count == 0 || location_count > UINT32_MAX / sizeof(*information))
    return;
  module = LoadLibraryA("powrprof.dll");
  if (module == NULL)
    return;
  address = GetProcAddress(module, "CallNtPowerInformation");
  if (address == NULL) {
    FreeLibrary(module);
    return;
  }
  query            = (uni_sysinfo_windows_call_nt_power_information_t)address;
  information_size = location_count * sizeof(*information);
  information      = (uni_sysinfo_windows_power_info_t *)calloc(location_count, sizeof(*information));
  if (information == NULL) {
    FreeLibrary(module);
    return;
  }

  /* ProcessorInformation is available before Win7; dynamic loading avoids a
     mandatory PowrProf import for an optional value. */
  if (query(11, NULL, 0, information, (ULONG)information_size) == 0) {
    for (location_index = 0; location_index < location_count; ++location_index) {
      const uni_sysinfo_windows_location_t *location = &locations[location_index];
      size_t native_index                            = (size_t)location->group * 64u + location->number;
      size_t information_index;
      for (information_index = 0; information_index < location_count; ++information_index) {
        if ((size_t)information[information_index].number == native_index &&
            information[information_index].max_mhz != 0) {
          uni_sysinfo_cpu_core_info_t *core = &snapshot->packages[location->package_index].cores[location->core_index];
          core->max_frequency_hz            = (uint64_t)information[information_index].max_mhz * UINT64_C(1000000);
          core->valid_fields |= UNI_SYSINFO_CPU_VALID_MAX_FREQUENCY;
          break;
        }
      }
    }
  }
  free(information);
  FreeLibrary(module);
}

static uni_sysinfo_cpu_core_type_e uni_sysinfo_windows_cpuid_core_type(const uni_sysinfo_cpu_core_info_t *core,
                                                                       int *restoration_failed) {
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
  uni_sysinfo_cpu_affinity_t *affinity = NULL;
  uni_sysinfo_cpu_core_type_e result;

  *restoration_failed = 0;
  if (uni_sysinfo_cpu_platform_affinity_apply(core, &affinity) != UNI_SYSINFO_RESULT_OK)
    return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
  result = uni_sysinfo_cpu_x86_current_core_type();
  if (uni_sysinfo_cpu_platform_affinity_restore(affinity) != UNI_SYSINFO_RESULT_OK) {
    result              = UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
    *restoration_failed = 1;
  }
  uni_sysinfo_cpu_platform_affinity_destroy(affinity);
  return result;
#else
  (void)core;
  *restoration_failed = 0;
  return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
#endif
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_snapshot(uni_sysinfo_cpu_snapshot_t *snapshot) {
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX package_buffer;
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX core_buffer;
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX cache_buffer;
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX numa_buffer;
  uni_sysinfo_windows_location_t *locations = NULL;
  size_t location_count                     = 0;
  BYTE *cursor;
  BYTE *end;
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX entry;
  size_t package_index = 0;
  char cpu_name[UNI_SYSINFO_CPU_NAME_CAPACITY];
  uni_sysinfo_cpu_architecture_e process_architecture;
  uni_sysinfo_cpu_architecture_e host_architecture;
  uni_sysinfo_cpu_abi_e process_abi;
  uni_sysinfo_cpu_vendor_e vendor       = UNI_SYSINFO_CPU_VENDOR_UNKNOWN;
  size_t *next_physical_core            = NULL;
  BYTE minimum_efficiency               = 255;
  BYTE maximum_efficiency               = 0;
  unsigned char efficiency_classes[256] = {0};
  size_t efficiency_class_count         = 0;
  DWORD package_buffer_length           = 0;
  DWORD core_buffer_length              = 0;
  DWORD cache_buffer_length             = 0;
  DWORD numa_buffer_length              = 0;
  GROUP_AFFINITY thread_affinity;
  int thread_affinity_valid;

  if (snapshot == NULL) {
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  }
  memset(snapshot, 0, sizeof(*snapshot));
  package_buffer = uni_sysinfo_windows_query(RelationProcessorPackage, &package_buffer_length);
  if (package_buffer == NULL) {
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }

  cursor = (BYTE *)package_buffer;
  end    = cursor + package_buffer_length;
  while (cursor < end) {
    entry = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)cursor;
    if (!uni_sysinfo_windows_valid_record(entry, (size_t)(end - cursor), RelationProcessorPackage)) {
      free(package_buffer);
      return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
    }
    ++snapshot->package_count;
    cursor += entry->Size;
  }
  if (cursor != end || snapshot->package_count == 0) {
    free(package_buffer);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }
  snapshot->packages = (uni_sysinfo_cpu_package_info_t *)calloc(snapshot->package_count, sizeof(*snapshot->packages));
  if (snapshot->packages == NULL) {
    free(package_buffer);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }

  cursor = (BYTE *)package_buffer;
  end    = cursor + package_buffer_length;
  while (cursor < end && package_index < snapshot->package_count) {
    entry = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)cursor;
    WORD mask_index;
    for (mask_index = 0; mask_index < entry->Processor.GroupCount; ++mask_index) {
      GROUP_AFFINITY mask = entry->Processor.GroupMask[mask_index];
      BYTE bit;
      for (bit = 0; bit < (BYTE)(sizeof(KAFFINITY) * 8); ++bit) {
        if ((mask.Mask & ((KAFFINITY)1 << bit)) != 0) {
          uni_sysinfo_windows_location_t *resized =
              (uni_sysinfo_windows_location_t *)realloc(locations, (location_count + 1) * sizeof(*locations));
          uni_sysinfo_cpu_core_info_t *cores;
          if (resized == NULL) {
            free(locations);
            free(package_buffer);
            uni_sysinfo_cpu_snapshot_destroy(snapshot);
            return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
          }
          locations = resized;
          cores     = (uni_sysinfo_cpu_core_info_t *)realloc(snapshot->packages[package_index].cores,
                                                             (snapshot->packages[package_index].core_count + 1) *
                                                                 sizeof(*cores));
          if (cores == NULL) {
            free(locations);
            free(package_buffer);
            uni_sysinfo_cpu_snapshot_destroy(snapshot);
            return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
          }
          snapshot->packages[package_index].cores = cores;
          memset(&cores[snapshot->packages[package_index].core_count], 0, sizeof(*cores));
          locations[location_count].group                               = mask.Group;
          locations[location_count].number                              = bit;
          locations[location_count].package_index                       = package_index;
          locations[location_count].core_index                          = snapshot->packages[package_index].core_count;
          cores[snapshot->packages[package_index].core_count].os_group  = mask.Group;
          cores[snapshot->packages[package_index].core_count].os_cpu_id = bit;
          ++snapshot->packages[package_index].core_count;
          ++location_count;
        }
      }
    }
    ++package_index;
    cursor += entry->Size;
  }
  free(package_buffer);

  process_architecture = uni_sysinfo_cpu_process_architecture();
  process_abi          = uni_sysinfo_cpu_process_abi();
  uni_sysinfo_windows_identity(cpu_name, sizeof(cpu_name), &host_architecture, &vendor);
  memset(&thread_affinity, 0, sizeof(thread_affinity));
  thread_affinity_valid = GetThreadGroupAffinity(GetCurrentThread(), &thread_affinity) != 0;
  for (package_index = 0; package_index < snapshot->package_count; ++package_index) {
    size_t core_index;
    for (core_index = 0; core_index < snapshot->packages[package_index].core_count; ++core_index) {
      uni_sysinfo_cpu_core_info_t *core = &snapshot->packages[package_index].cores[core_index];
      lstrcpynA(core->name, cpu_name, (int)sizeof(core->name));
      core->architecture      = process_architecture;
      core->host_architecture = host_architecture;
      core->abi               = process_abi;
      core->vendor            = vendor;
      core->online            = 1;
      core->allowed           = thread_affinity_valid && core->os_group == thread_affinity.Group &&
                                core->os_cpu_id < sizeof(KAFFINITY) * 8 &&
                                (thread_affinity.Mask & ((KAFFINITY)1 << core->os_cpu_id)) != 0;
      core->valid_fields =
          UNI_SYSINFO_CPU_VALID_NAME | UNI_SYSINFO_CPU_VALID_ARCHITECTURE | UNI_SYSINFO_CPU_VALID_NATIVE_ID;
      if (thread_affinity_valid && GetActiveProcessorGroupCount() == 1)
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_ALLOWED_SET;
      if (vendor != UNI_SYSINFO_CPU_VENDOR_UNKNOWN)
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_VENDOR;
    }
  }

  core_buffer        = uni_sysinfo_windows_query(RelationProcessorCore, &core_buffer_length);
  next_physical_core = (size_t *)calloc(snapshot->package_count, sizeof(*next_physical_core));
  if (core_buffer == NULL || next_physical_core == NULL) {
    free(next_physical_core);
    free(core_buffer);
    free(locations);
    uni_sysinfo_cpu_snapshot_destroy(snapshot);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }
  {
    cursor = (BYTE *)core_buffer;
    end    = cursor + core_buffer_length;
    while (cursor < end) {
      entry = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)cursor;
      if (!uni_sysinfo_windows_valid_record(entry, (size_t)(end - cursor), RelationProcessorCore)) {
        free(next_physical_core);
        free(core_buffer);
        free(locations);
        uni_sysinfo_cpu_snapshot_destroy(snapshot);
        return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
      }
      WORD mask_index;
      size_t smt_index        = 0;
      size_t relation_package = (size_t)-1;
      BYTE efficiency         = uni_sysinfo_windows_efficiency_class(&entry->Processor);

      if (efficiency < minimum_efficiency)
        minimum_efficiency = efficiency;
      if (efficiency > maximum_efficiency)
        maximum_efficiency = efficiency;
      if (!efficiency_classes[efficiency]) {
        efficiency_classes[efficiency] = 1;
        ++efficiency_class_count;
      }

      for (mask_index = 0; mask_index < entry->Processor.GroupCount; ++mask_index) {
        GROUP_AFFINITY mask = entry->Processor.GroupMask[mask_index];
        BYTE bit;
        for (bit = 0; bit < (BYTE)(sizeof(KAFFINITY) * 8); ++bit) {
          if ((mask.Mask & ((KAFFINITY)1 << bit)) != 0) {
            uni_sysinfo_windows_location_t *location =
                uni_sysinfo_windows_find(locations, location_count, mask.Group, bit);
            if (location != NULL) {
              uni_sysinfo_cpu_core_info_t *core =
                  &snapshot->packages[location->package_index].cores[location->core_index];
              relation_package           = location->package_index;
              core->topology.hw_core_idx = next_physical_core[relation_package];
              core->topology.smt_idx     = smt_index++;
              core->raw_efficiency_class = (unsigned int)efficiency;
              core->valid_fields |= UNI_SYSINFO_CPU_VALID_TOPOLOGY;
            }
          }
        }
      }
      if (relation_package != (size_t)-1)
        ++next_physical_core[relation_package];
      cursor += entry->Size;
    }

    for (package_index = 0; package_index < snapshot->package_count; ++package_index) {
      size_t core_index;
      for (core_index = 0; core_index < snapshot->packages[package_index].core_count; ++core_index) {
        uni_sysinfo_cpu_core_info_t *core    = &snapshot->packages[package_index].cores[core_index];
        uni_sysinfo_cpu_topology_t *topology = &core->topology;
        BYTE efficiency                      = (BYTE)core->raw_efficiency_class;

        if ((core->valid_fields & UNI_SYSINFO_CPU_VALID_TOPOLOGY) == 0) {
          free(next_physical_core);
          free(core_buffer);
          free(locations);
          uni_sysinfo_cpu_snapshot_destroy(snapshot);
          return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
        }
        topology->core_type = UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
        if (maximum_efficiency != 0) {
          core->performance_rank = (unsigned int)efficiency + 1;
          core->valid_fields |= UNI_SYSINFO_CPU_VALID_PERFORMANCE_CLASS;
        }
        if (minimum_efficiency != maximum_efficiency && maximum_efficiency != 0) {
          if (efficiency == maximum_efficiency) {
            topology->core_type = UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE;
          } else if (efficiency_class_count > 2 && efficiency == minimum_efficiency) {
            topology->core_type = UNI_SYSINFO_CPU_CORE_TYPE_LOW_POWER;
          } else {
            topology->core_type = UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY;
          }
        }
        if (process_architecture == UNI_SYSINFO_CPU_ARCHITECTURE_X86 &&
            (((core->valid_fields & UNI_SYSINFO_CPU_VALID_ALLOWED_SET) == 0) || core->allowed)) {
          int restoration_failed = 0;
          uni_sysinfo_cpu_core_type_e cpuid_type = uni_sysinfo_windows_cpuid_core_type(core, &restoration_failed);
          if (restoration_failed) {
            free(next_physical_core);
            free(core_buffer);
            free(locations);
            uni_sysinfo_cpu_snapshot_destroy(snapshot);
            return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
          }
          if (cpuid_type != UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN) {
            topology->core_type = cpuid_type;
          }
        }
        if (topology->core_type != UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN)
          core->valid_fields |= UNI_SYSINFO_CPU_VALID_CORE_TYPE;
      }
    }
  }
  free(next_physical_core);
  free(core_buffer);

  numa_buffer = uni_sysinfo_windows_query(RelationNumaNode, &numa_buffer_length);
  if (numa_buffer != NULL) {
    cursor = (BYTE *)numa_buffer;
    end    = cursor + numa_buffer_length;
    while (cursor < end) {
      GROUP_AFFINITY mask;
      BYTE bit;

      entry = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)cursor;
      if (!uni_sysinfo_windows_valid_record(entry, (size_t)(end - cursor), RelationNumaNode)) {
        free(numa_buffer);
        free(locations);
        uni_sysinfo_cpu_snapshot_destroy(snapshot);
        return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
      }
      mask = entry->NumaNode.GroupMask;
      for (bit = 0; bit < (BYTE)(sizeof(KAFFINITY) * 8); ++bit) {
        if ((mask.Mask & ((KAFFINITY)1 << bit)) != 0) {
          uni_sysinfo_windows_location_t *location =
              uni_sysinfo_windows_find(locations, location_count, mask.Group, bit);
          if (location != NULL) {
            uni_sysinfo_cpu_core_info_t *core =
                &snapshot->packages[location->package_index].cores[location->core_index];
            core->numa_node_idx = (size_t)entry->NumaNode.NodeNumber;
            core->valid_fields |= UNI_SYSINFO_CPU_VALID_NUMA_NODE;
          }
        }
      }
      cursor += entry->Size;
    }
    free(numa_buffer);
  }

  cache_buffer = uni_sysinfo_windows_query(RelationCache, &cache_buffer_length);
  if (cache_buffer != NULL) {
    cursor = (BYTE *)cache_buffer;
    end    = cursor + cache_buffer_length;
    while (cursor < end) {
      entry = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)cursor;
      if (!uni_sysinfo_windows_valid_record(entry, (size_t)(end - cursor), RelationCache)) {
        free(cache_buffer);
        free(locations);
        uni_sysinfo_cpu_snapshot_destroy(snapshot);
        return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
      }
      GROUP_AFFINITY mask = entry->Cache.GroupMask;
      BYTE bit;
      for (bit = 0; bit < (BYTE)(sizeof(KAFFINITY) * 8); ++bit) {
        if ((mask.Mask & ((KAFFINITY)1 << bit)) != 0) {
          uni_sysinfo_windows_location_t *location =
              uni_sysinfo_windows_find(locations, location_count, mask.Group, bit);
          if (location != NULL) {
            uni_sysinfo_windows_apply_cache(&snapshot->packages[location->package_index].cores[location->core_index],
                                            &entry->Cache);
          }
        }
      }
      cursor += entry->Size;
    }
    free(cache_buffer);
  }

  for (package_index = 0; package_index < snapshot->package_count; ++package_index) {
    size_t core_index;
    for (core_index = 0; core_index < snapshot->packages[package_index].core_count; ++core_index) {
      uni_sysinfo_cpu_core_info_t *core = &snapshot->packages[package_index].cores[core_index];
      if (core->cache.l1d_size != 0 || core->cache.l1i_size != 0 || core->cache.l2_size != 0 ||
          core->cache.l3_size != 0)
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_CACHE;
    }
  }
  uni_sysinfo_windows_apply_max_frequency(snapshot, locations, location_count);

  free(locations);
  if (location_count == 0) {
    uni_sysinfo_cpu_snapshot_destroy(snapshot);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_aarch64_features(uni_sysinfo_cpu_aarch64_features_t *result) {
  if (result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *result = 0;
#if defined(_M_ARM64) || defined(__aarch64__)
#ifdef PF_ARM_NEON_INSTRUCTIONS_AVAILABLE
  if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_NEON;
#elif defined(PF_ARM_V8_INSTRUCTIONS_AVAILABLE)
  if (IsProcessorFeaturePresent(PF_ARM_V8_INSTRUCTIONS_AVAILABLE))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_NEON;
#endif
#ifdef PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE
  if (IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE)) {
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_AES | UNI_SYSINFO_CPU_AARCH64_FEATURE_PMULL |
               UNI_SYSINFO_CPU_AARCH64_FEATURE_SHA1 | UNI_SYSINFO_CPU_AARCH64_FEATURE_SHA2;
  }
#endif
#ifdef PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE
  if (IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_CRC32;
#endif
#ifdef PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE
  if (IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_ATOMICS;
#endif
#ifdef PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE
  if (IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_DOTPROD;
#endif
#ifdef PF_ARM_SVE_INSTRUCTIONS_AVAILABLE
  if (IsProcessorFeaturePresent(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SVE;
#endif
#ifdef PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE
  if (IsProcessorFeaturePresent(PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SVE2;
#endif
  return UNI_SYSINFO_RESULT_OK;
#else
  return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
#endif
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_apply(const uni_sysinfo_cpu_core_info_t *core,
                                                             uni_sysinfo_cpu_affinity_t **previous) {
  GROUP_AFFINITY target = {0};
  uni_sysinfo_cpu_affinity_t *state;
  DWORD error;

  if (core == NULL || previous == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *previous = NULL;
  if (((core->valid_fields & UNI_SYSINFO_CPU_VALID_ALLOWED_SET) != 0 && !core->allowed) ||
      core->os_cpu_id >= sizeof(KAFFINITY) * 8)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  state = (uni_sysinfo_cpu_affinity_t *)malloc(sizeof(*state));
  if (state == NULL)
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  target.Group = core->os_group;
  target.Mask  = (KAFFINITY)1 << core->os_cpu_id;
  if (!SetThreadGroupAffinity(GetCurrentThread(), &target, &state->previous)) {
    error = GetLastError();
    free(state);
    if (error == ERROR_INVALID_PARAMETER || error == ERROR_ACCESS_DENIED || error == ERROR_NOT_SUPPORTED)
      return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }
  *previous = state;
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_restore(uni_sysinfo_cpu_affinity_t *previous) {
  if (previous == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  return SetThreadGroupAffinity(GetCurrentThread(), &previous->previous, NULL) ? UNI_SYSINFO_RESULT_OK
                                                                               : UNI_SYSINFO_RESULT_SYSTEM_ERROR;
}

void uni_sysinfo_cpu_platform_affinity_destroy(uni_sysinfo_cpu_affinity_t *affinity) { free(affinity); }
