#include "uni_sysinfo_cpu_internal.h"

#include <limits.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/types.h>

struct uni_sysinfo_cpu_affinity {
  thread_t thread;
  thread_affinity_policy_data_t policy;
};

static int uni_sysinfo_darwin_get_uint64(const char *name, uint64_t *result) {
  uint64_t value = 0;
  size_t length  = sizeof(value);

  if (sysctlbyname(name, &value, &length, NULL, 0) != 0)
    return 0;
  if (length == sizeof(uint64_t)) {
    *result = value;
    return 1;
  }
  if (length == sizeof(uint32_t)) {
    uint32_t value32;
    memcpy(&value32, &value, sizeof(value32));
    *result = value32;
    return 1;
  }
  return 0;
}

static uint64_t uni_sysinfo_darwin_uint64(const char *name, uint64_t fallback) {
  uint64_t value;
  return uni_sysinfo_darwin_get_uint64(name, &value) ? value : fallback;
}

static int uni_sysinfo_darwin_string(const char *key, char *value, size_t capacity) {
  size_t length;

  if (capacity == 0)
    return 0;
  length = capacity;
  if (sysctlbyname(key, value, &length, NULL, 0) != 0 || length == 0) {
    value[0] = '\0';
    return 0;
  }
  value[capacity - 1] = '\0';
  return 1;
}

#if defined(__aarch64__) || defined(__arm64__)
static int uni_sysinfo_darwin_feature(const char *name) {
  uint64_t value;
  return uni_sysinfo_darwin_get_uint64(name, &value) && value != 0;
}
#endif

static int uni_sysinfo_darwin_cache_valid(const uni_sysinfo_cpu_cache_t *cache) {
  return cache->l1d_size != 0 || cache->l1i_size != 0 || cache->l2_size != 0 || cache->l3_size != 0;
}

static void uni_sysinfo_darwin_init_core(uni_sysinfo_cpu_core_info_t *core, const char *name, int name_valid,
                                         uni_sysinfo_cpu_architecture_e architecture,
                                         uni_sysinfo_cpu_architecture_e host_architecture, uni_sysinfo_cpu_abi_e abi,
                                         uni_sysinfo_cpu_vendor_e vendor, const uni_sysinfo_cpu_cache_t *cache,
                                         size_t os_cpu_id) {
  snprintf(core->name, sizeof(core->name), "%s", name);
  core->architecture      = architecture;
  core->host_architecture = host_architecture;
  core->abi               = abi;
  core->vendor            = vendor;
  core->cache             = *cache;
  core->os_cpu_id         = os_cpu_id;
  core->online            = 1;
  core->allowed           = 1;
  core->valid_fields      = UNI_SYSINFO_CPU_VALID_ARCHITECTURE | UNI_SYSINFO_CPU_VALID_ALLOWED_SET;
  if (name_valid)
    core->valid_fields |= UNI_SYSINFO_CPU_VALID_NAME;
  if (vendor != UNI_SYSINFO_CPU_VENDOR_UNKNOWN)
    core->valid_fields |= UNI_SYSINFO_CPU_VALID_VENDOR;
  if (uni_sysinfo_darwin_cache_valid(cache))
    core->valid_fields |= UNI_SYSINFO_CPU_VALID_CACHE;
}

static uni_sysinfo_cpu_core_type_e uni_sysinfo_darwin_core_type(const char *level_name, size_t level,
                                                                size_t level_count) {
  if (strstr(level_name, "Performance") != NULL)
    return UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE;
  if (strstr(level_name, "Low") != NULL)
    return UNI_SYSINFO_CPU_CORE_TYPE_LOW_POWER;
  if (strstr(level_name, "Efficiency") != NULL)
    return UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY;
  if (level == 0)
    return UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE;
  if (level + 1 == level_count && level_count > 2)
    return UNI_SYSINFO_CPU_CORE_TYPE_LOW_POWER;
  return UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY;
}

static void uni_sysinfo_darwin_set_max_frequency(uni_sysinfo_cpu_core_info_t *core, uint64_t frequency, int valid) {
  if (!valid || frequency == 0)
    return;
  core->max_frequency_hz = frequency;
  core->valid_fields |= UNI_SYSINFO_CPU_VALID_MAX_FREQUENCY;
}

static void uni_sysinfo_darwin_set_performance_class(uni_sysinfo_cpu_core_info_t *core, size_t level,
                                                     size_t level_count) {
  const size_t rank          = level_count - level;
  core->raw_efficiency_class = level > UINT_MAX ? UINT_MAX : (unsigned int)level;
  core->performance_rank     = rank > UINT_MAX ? UINT_MAX : (unsigned int)rank;
  core->valid_fields |= UNI_SYSINFO_CPU_VALID_PERFORMANCE_CLASS;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_snapshot(uni_sysinfo_cpu_snapshot_t *snapshot) {
  uint64_t package_count;
  uint64_t logical_count;
  uint64_t physical_count;
  uint64_t max_frequency           = 0;
  uint64_t performance_level_count = 1;
  uint64_t translated_value        = 0;
  char cpu_name[UNI_SYSINFO_CPU_NAME_CAPACITY];
  uni_sysinfo_cpu_architecture_e architecture;
  uni_sysinfo_cpu_architecture_e host_architecture;
  uni_sysinfo_cpu_abi_e abi;
  uni_sysinfo_cpu_vendor_e vendor = UNI_SYSINFO_CPU_VENDOR_UNKNOWN;
  uni_sysinfo_cpu_cache_t cache;
  size_t package_index;
  size_t global_core_index = 0;
  uint64_t logical_per_package;
  uint64_t physical_per_package;
  int name_valid = 0;
  int performance_levels_valid;
  int max_frequency_valid;
  int translated;

  if (snapshot == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  memset(snapshot, 0, sizeof(*snapshot));

  package_count  = uni_sysinfo_darwin_uint64("hw.packages", 1);
  logical_count  = uni_sysinfo_darwin_uint64("hw.logicalcpu", 0);
  physical_count = uni_sysinfo_darwin_uint64("hw.physicalcpu", logical_count);
  if (package_count == 0 || logical_count == 0 || logical_count > SIZE_MAX)
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  if (package_count > logical_count)
    package_count = logical_count;

  architecture      = uni_sysinfo_cpu_process_architecture();
  host_architecture = architecture;
  abi               = uni_sysinfo_cpu_process_abi();
  translated = uni_sysinfo_darwin_get_uint64("sysctl.proc_translated", &translated_value) && translated_value != 0;
  if (translated && architecture == UNI_SYSINFO_CPU_ARCHITECTURE_X86)
    host_architecture = UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64;

  cpu_name[0] = '\0';
  if (host_architecture == UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64) {
    vendor     = UNI_SYSINFO_CPU_VENDOR_APPLE;
    name_valid = uni_sysinfo_darwin_string("machdep.cpu.brand_string", cpu_name, sizeof(cpu_name));
    if (!name_valid)
      name_valid = uni_sysinfo_darwin_string("hw.model", cpu_name, sizeof(cpu_name));
  } else if (architecture == UNI_SYSINFO_CPU_ARCHITECTURE_X86) {
    uni_sysinfo_cpu_x86_detect(cpu_name, sizeof(cpu_name), &vendor, NULL);
    name_valid = cpu_name[0] != '\0';
    if (!name_valid)
      name_valid = uni_sysinfo_darwin_string("machdep.cpu.brand_string", cpu_name, sizeof(cpu_name));
  } else {
    name_valid = uni_sysinfo_darwin_string("hw.model", cpu_name, sizeof(cpu_name));
  }
  cache.l1d_size      = (size_t)uni_sysinfo_darwin_uint64("hw.l1dcachesize", 0);
  cache.l1i_size      = (size_t)uni_sysinfo_darwin_uint64("hw.l1icachesize", 0);
  cache.l2_size       = (size_t)uni_sysinfo_darwin_uint64("hw.l2cachesize", 0);
  cache.l3_size       = (size_t)uni_sysinfo_darwin_uint64("hw.l3cachesize", 0);
  max_frequency_valid = uni_sysinfo_darwin_get_uint64("hw.cpufrequency_max", &max_frequency) && max_frequency != 0;

  if (host_architecture == UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64) {
    size_t level;
    size_t output_index    = 0;
    size_t physical_offset = 0;

    performance_levels_valid =
        uni_sysinfo_darwin_get_uint64("hw.nperflevels", &performance_level_count) && performance_level_count != 0;
    if (!performance_levels_valid)
      performance_level_count = 1;
    if (performance_level_count > SIZE_MAX)
      return UNI_SYSINFO_RESULT_SYSTEM_ERROR;

    snapshot->package_count = 1;
    snapshot->packages      = (uni_sysinfo_cpu_package_info_t *)calloc(1, sizeof(*snapshot->packages));
    if (snapshot->packages == NULL)
      return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
    snapshot->packages[0].cores =
        (uni_sysinfo_cpu_core_info_t *)calloc((size_t)logical_count, sizeof(*snapshot->packages[0].cores));
    if (snapshot->packages[0].cores == NULL) {
      uni_sysinfo_cpu_snapshot_destroy(snapshot);
      return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
    }
    snapshot->packages[0].core_count = (size_t)logical_count;

    for (level = 0; level < (size_t)performance_level_count && output_index < (size_t)logical_count; ++level) {
      char key[64];
      char level_name[32];
      uint64_t level_logical;
      uint64_t level_physical;
      uint64_t level_max_frequency = max_frequency;
      uni_sysinfo_cpu_cache_t level_cache;
      size_t local_index;
      int level_max_frequency_valid = max_frequency_valid;

      snprintf(key, sizeof(key), "hw.perflevel%zu.logicalcpu", level);
      level_logical = uni_sysinfo_darwin_uint64(
          key, level + 1 == (size_t)performance_level_count ? logical_count - output_index : 0);
      if (level_logical > logical_count - output_index)
        level_logical = logical_count - output_index;
      snprintf(key, sizeof(key), "hw.perflevel%zu.physicalcpu", level);
      level_physical = uni_sysinfo_darwin_uint64(key, level_logical);
      if (level_physical == 0)
        level_physical = level_logical;

      snprintf(key, sizeof(key), "hw.perflevel%zu.l1dcachesize", level);
      level_cache.l1d_size = (size_t)uni_sysinfo_darwin_uint64(key, cache.l1d_size);
      snprintf(key, sizeof(key), "hw.perflevel%zu.l1icachesize", level);
      level_cache.l1i_size = (size_t)uni_sysinfo_darwin_uint64(key, cache.l1i_size);
      snprintf(key, sizeof(key), "hw.perflevel%zu.l2cachesize", level);
      level_cache.l2_size = (size_t)uni_sysinfo_darwin_uint64(key, cache.l2_size);
      snprintf(key, sizeof(key), "hw.perflevel%zu.l3cachesize", level);
      level_cache.l3_size = (size_t)uni_sysinfo_darwin_uint64(key, cache.l3_size);
      snprintf(key, sizeof(key), "hw.perflevel%zu.cpufrequency_max", level);
      if (uni_sysinfo_darwin_get_uint64(key, &level_max_frequency) && level_max_frequency != 0) {
        level_max_frequency_valid = 1;
      }
      level_name[0] = '\0';
      snprintf(key, sizeof(key), "hw.perflevel%zu.name", level);
      (void)uni_sysinfo_darwin_string(key, level_name, sizeof(level_name));

      for (local_index = 0; local_index < (size_t)level_logical; ++local_index, ++output_index) {
        uni_sysinfo_cpu_core_info_t *core = &snapshot->packages[0].cores[output_index];
        uni_sysinfo_darwin_init_core(core, cpu_name, name_valid, architecture, host_architecture, abi, vendor,
                                     &level_cache, output_index);
        core->topology.hw_core_idx = physical_offset + local_index % (size_t)level_physical;
        core->topology.smt_idx     = local_index / (size_t)level_physical;
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_TOPOLOGY;
        if (performance_levels_valid) {
          core->topology.core_type = uni_sysinfo_darwin_core_type(level_name, level, (size_t)performance_level_count);
          core->valid_fields |= UNI_SYSINFO_CPU_VALID_CORE_TYPE;
          uni_sysinfo_darwin_set_performance_class(core, level, (size_t)performance_level_count);
        }
        uni_sysinfo_darwin_set_max_frequency(core, level_max_frequency, level_max_frequency_valid);
      }
      physical_offset += (size_t)level_physical;
    }
    while (output_index < (size_t)logical_count) {
      uni_sysinfo_cpu_core_info_t *core = &snapshot->packages[0].cores[output_index];
      uni_sysinfo_darwin_init_core(core, cpu_name, name_valid, architecture, host_architecture, abi, vendor, &cache,
                                   output_index);
      core->topology.hw_core_idx = output_index;
      core->topology.core_type   = UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_TOPOLOGY;
      uni_sysinfo_darwin_set_max_frequency(core, max_frequency, max_frequency_valid);
      ++output_index;
    }
    return UNI_SYSINFO_RESULT_OK;
  }

  logical_per_package =
      uni_sysinfo_darwin_uint64("machdep.cpu.logical_per_package", (logical_count + package_count - 1) / package_count);
  physical_per_package =
      uni_sysinfo_darwin_uint64("machdep.cpu.cores_per_package", (physical_count + package_count - 1) / package_count);
  if (logical_per_package == 0)
    logical_per_package = 1;
  if (physical_per_package == 0)
    physical_per_package = 1;

  snapshot->package_count = (size_t)package_count;
  snapshot->packages = (uni_sysinfo_cpu_package_info_t *)calloc(snapshot->package_count, sizeof(*snapshot->packages));
  if (snapshot->packages == NULL)
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;

  for (package_index = 0; package_index < snapshot->package_count; ++package_index) {
    size_t remaining = (size_t)logical_count - global_core_index;
    size_t count     = (size_t)logical_per_package;
    size_t core_index;
    if (count > remaining || package_index + 1 == snapshot->package_count)
      count = remaining;
    snapshot->packages[package_index].cores =
        (uni_sysinfo_cpu_core_info_t *)calloc(count, sizeof(*snapshot->packages[package_index].cores));
    if (snapshot->packages[package_index].cores == NULL) {
      uni_sysinfo_cpu_snapshot_destroy(snapshot);
      return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
    }
    snapshot->packages[package_index].core_count = count;
    for (core_index = 0; core_index < count; ++core_index, ++global_core_index) {
      uni_sysinfo_cpu_core_info_t *core = &snapshot->packages[package_index].cores[core_index];
      uni_sysinfo_darwin_init_core(core, cpu_name, name_valid, architecture, host_architecture, abi, vendor, &cache,
                                   global_core_index);
      core->topology.hw_core_idx = core_index % (size_t)physical_per_package;
      core->topology.smt_idx     = core_index / (size_t)physical_per_package;
      core->topology.core_type   = UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_TOPOLOGY;
      uni_sysinfo_darwin_set_max_frequency(core, max_frequency, max_frequency_valid);
    }
  }
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_aarch64_features(uni_sysinfo_cpu_aarch64_features_t *result) {
  if (result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *result = 0;
#if defined(__aarch64__) || defined(__arm64__)
  /* Advanced SIMD is part of the AArch64 execution environment on Darwin. */
  (void)uni_sysinfo_darwin_feature("hw.optional.AdvSIMD");
  (void)uni_sysinfo_darwin_feature("hw.optional.neon");
  *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_NEON;
  if (uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_AES"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_AES;
  if (uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_PMULL"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_PMULL;
  if (uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_SHA1"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SHA1;
  if (uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_SHA256"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SHA2;
  if (uni_sysinfo_darwin_feature("hw.optional.armv8_crc32") || uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_CRC32"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_CRC32;
  if (uni_sysinfo_darwin_feature("hw.optional.armv8_1_atomics") ||
      uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_LSE"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_ATOMICS;
  if (uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_FP16"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_FP16_SCALAR | UNI_SYSINFO_CPU_AARCH64_FEATURE_FP16_SIMD;
  else if (uni_sysinfo_darwin_feature("hw.optional.neon_fp16"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_FP16_SIMD;
  if (uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_DotProd"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_DOTPROD;
  if (uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_SVE"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SVE;
  if (uni_sysinfo_darwin_feature("hw.optional.arm.FEAT_SVE2"))
    *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SVE2;
  return UNI_SYSINFO_RESULT_OK;
#else
  return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
#endif
}

static uni_sysinfo_result_e uni_sysinfo_darwin_mach_policy_result(kern_return_t status) {
  if (status == KERN_SUCCESS)
    return UNI_SYSINFO_RESULT_OK;
#ifdef KERN_NOT_SUPPORTED
  if (status == KERN_NOT_SUPPORTED)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
#endif
  return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_current_frequency(const uni_sysinfo_cpu_core_info_t *core,
                                                                uint64_t *result_hz) {
  (void)core;
  *result_hz = 0;
  return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_apply(const uni_sysinfo_cpu_core_info_t *core,
                                                             uni_sysinfo_cpu_affinity_t **previous) {
  uni_sysinfo_cpu_affinity_t *state;
  thread_affinity_policy_data_t current;
  thread_affinity_policy_data_t target;
  mach_msg_type_number_t count = THREAD_AFFINITY_POLICY_COUNT;
  boolean_t get_default        = 0;
  kern_return_t status;

  if (core == NULL || previous == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *previous = NULL;
  if (core->host_architecture == UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64 || core->os_cpu_id >= (size_t)INT_MAX)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;

  state = (uni_sysinfo_cpu_affinity_t *)calloc(1, sizeof(*state));
  if (state == NULL)
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  state->thread = mach_thread_self();
  if (state->thread == MACH_PORT_NULL) {
    free(state);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }

  status = thread_policy_get(state->thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&current, &count, &get_default);
  if (status != KERN_SUCCESS) {
    uni_sysinfo_cpu_platform_affinity_destroy(state);
    return uni_sysinfo_darwin_mach_policy_result(status);
  }
  state->policy.affinity_tag = get_default ? THREAD_AFFINITY_TAG_NULL : current.affinity_tag;
  target.affinity_tag        = (integer_t)core->os_cpu_id + 1;
  status = thread_policy_set(state->thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&target,
                             THREAD_AFFINITY_POLICY_COUNT);
  if (status != KERN_SUCCESS) {
    uni_sysinfo_cpu_platform_affinity_destroy(state);
    return uni_sysinfo_darwin_mach_policy_result(status);
  }
  *previous = state;
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_restore(uni_sysinfo_cpu_affinity_t *previous) {
  kern_return_t status;

  if (previous == NULL || previous->thread == MACH_PORT_NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  status = thread_policy_set(previous->thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&previous->policy,
                             THREAD_AFFINITY_POLICY_COUNT);
  return uni_sysinfo_darwin_mach_policy_result(status);
}

void uni_sysinfo_cpu_platform_affinity_destroy(uni_sysinfo_cpu_affinity_t *affinity) {
  if (affinity == NULL)
    return;
  if (affinity->thread != MACH_PORT_NULL)
    (void)mach_port_deallocate(mach_task_self(), affinity->thread);
  free(affinity);
}
