#define _GNU_SOURCE
#include "uni_sysinfo_cpu_internal.h"

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <unistd.h>

#if defined(__aarch64__)
#include <asm/hwcap.h>
#endif

struct uni_sysinfo_cpu_affinity {
  cpu_set_t *set;
  size_t set_size;
  size_t cpu_count;
};

typedef struct {
  long os_cpu_id;
  long package_id;
  long die_id;
  long core_id;
  int package_id_valid;
  int die_id_valid;
  int core_id_valid;
  size_t logical_die_id;
  size_t logical_core_id;
  size_t thread_id;
} uni_sysinfo_linux_cpu_t;

static int uni_sysinfo_linux_read_text(const char *path, char *buffer, size_t capacity) {
  FILE *file;
  size_t length;

  if (capacity == 0) {
    return 0;
  }
  file = fopen(path, "r");
  if (file == NULL) {
    return 0;
  }
  if (fgets(buffer, (int)capacity, file) == NULL) {
    fclose(file);
    return 0;
  }
  fclose(file);

  length = strlen(buffer);
  while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r' || buffer[length - 1] == ' ' ||
                        buffer[length - 1] == '\t')) {
    buffer[--length] = '\0';
  }
  return 1;
}

static int uni_sysinfo_linux_read_long(const char *path, long *value) {
  char buffer[64];
  char *end;
  long parsed;

  if (!uni_sysinfo_linux_read_text(path, buffer, sizeof(buffer))) {
    return 0;
  }
  errno  = 0;
  parsed = strtol(buffer, &end, 10);
  if (errno != 0 || end == buffer) {
    return 0;
  }
  *value = parsed;
  return 1;
}

static int uni_sysinfo_linux_online_cpu_ids(long **cpu_ids, size_t *cpu_count) {
  char online[4096];
  const char *cursor;

  *cpu_ids   = NULL;
  *cpu_count = 0;
  if (!uni_sysinfo_linux_read_text("/sys/devices/system/cpu/online", online, sizeof(online))) {
    long configured_count = sysconf(_SC_NPROCESSORS_CONF);
    long cpu_id;
    if (configured_count <= 0)
      return 0;
    for (cpu_id = 0; cpu_id < configured_count; ++cpu_id) {
      char path[256];
      long is_online = 1;
      long *resized;
      snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/online", cpu_id);
      if (uni_sysinfo_linux_read_long(path, &is_online) && is_online == 0)
        continue;
      snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld", cpu_id);
      if (access(path, F_OK) != 0)
        continue;
      resized = (long *)realloc(*cpu_ids, (*cpu_count + 1) * sizeof(**cpu_ids));
      if (resized == NULL) {
        free(*cpu_ids);
        *cpu_ids   = NULL;
        *cpu_count = 0;
        return 0;
      }
      *cpu_ids                   = resized;
      (*cpu_ids)[(*cpu_count)++] = cpu_id;
    }
    return *cpu_count != 0;
  }

  cursor = online;
  while (*cursor != '\0') {
    char *end;
    long first;
    long last;
    long cpu_id;

    errno = 0;
    first = strtol(cursor, &end, 10);
    if (errno != 0 || end == cursor || first < 0)
      goto error;
    cursor = end;
    last   = first;
    if (*cursor == '-') {
      errno = 0;
      last  = strtol(cursor + 1, &end, 10);
      if (errno != 0 || end == cursor + 1 || last < first)
        goto error;
      cursor = end;
    }
    for (cpu_id = first; cpu_id <= last; ++cpu_id) {
      long *resized = (long *)realloc(*cpu_ids, (*cpu_count + 1) * sizeof(**cpu_ids));
      if (resized == NULL)
        goto error;
      *cpu_ids                   = resized;
      (*cpu_ids)[(*cpu_count)++] = cpu_id;
      if (cpu_id == last)
        break;
    }
    if (*cursor == ',') {
      ++cursor;
    } else if (*cursor != '\0') {
      goto error;
    }
  }
  return *cpu_count != 0;

error:
  free(*cpu_ids);
  *cpu_ids   = NULL;
  *cpu_count = 0;
  return 0;
}

static size_t uni_sysinfo_linux_parse_size(const char *text) {
  char *end;
  unsigned long long value;

  errno = 0;
  value = strtoull(text, &end, 10);
  if (errno != 0 || end == text) {
    return 0;
  }
  if (*end == 'K' || *end == 'k') {
    value *= 1024u;
  } else if (*end == 'M' || *end == 'm') {
    value *= 1024u * 1024u;
  } else if (*end == 'G' || *end == 'g') {
    value *= 1024u * 1024u * 1024u;
  }
  return (size_t)value;
}

static int uni_sysinfo_linux_get_affinity(size_t minimum_cpu_count, cpu_set_t **result, size_t *set_size_result,
                                          size_t *cpu_count_result) {
  long configured_count = sysconf(_SC_NPROCESSORS_CONF);
  size_t cpu_count      = minimum_cpu_count;
  unsigned int attempt;

  if (configured_count > 0 && (size_t)configured_count > cpu_count)
    cpu_count = (size_t)configured_count;
  if (cpu_count < CPU_SETSIZE)
    cpu_count = CPU_SETSIZE;

  for (attempt = 0; attempt < 16; ++attempt) {
    const size_t set_size = CPU_ALLOC_SIZE(cpu_count);
    cpu_set_t *set        = CPU_ALLOC(cpu_count);
    int error;

    if (set == NULL)
      return 0;
    if (sched_getaffinity(0, set_size, set) == 0) {
      *result           = set;
      *set_size_result  = set_size;
      *cpu_count_result = cpu_count;
      return 1;
    }
    error = errno;
    CPU_FREE(set);
    if (error != EINVAL || cpu_count > SIZE_MAX / 2)
      return 0;
    cpu_count *= 2;
  }
  return 0;
}

static int uni_sysinfo_linux_cpu_allowed(long cpu_id, int *valid) {
  size_t set_size;
  size_t cpu_count;
  cpu_set_t *set;
  int allowed;

  *valid = 0;
  if (!uni_sysinfo_linux_get_affinity((size_t)cpu_id + 1, &set, &set_size, &cpu_count))
    return 0;
  *valid = 1;
  (void)cpu_count;
  allowed = CPU_ISSET_S((size_t)cpu_id, set_size, set);
  CPU_FREE(set);
  return allowed;
}

static int uni_sysinfo_linux_numa_node(long cpu_id, size_t *result) {
  size_t node;
  for (node = 0; node < 4096; ++node) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/node%zu", cpu_id, node);
    if (access(path, F_OK) == 0) {
      *result = node;
      return 1;
    }
  }
  return 0;
}

static int uni_sysinfo_linux_max_frequency(long cpu_id, uint64_t *result) {
  const char *names[] = {"cpuinfo_max_freq", "scaling_max_freq"};
  size_t index;
  for (index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
    char path[256];
    long khz;
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cpufreq/%s", cpu_id, names[index]);
    if (uni_sysinfo_linux_read_long(path, &khz) && khz > 0) {
      *result = (uint64_t)khz * UINT64_C(1000);
      return 1;
    }
  }
  return 0;
}

static void uni_sysinfo_linux_cpu_name(char *name, size_t capacity) {
  FILE *file;
  char line[512];
  char fallback[UNI_SYSINFO_CPU_NAME_CAPACITY] = {0};

  uni_sysinfo_cpu_x86_detect(name, capacity, NULL, NULL);
  if (name[0] != '\0') {
    return;
  }

  file = fopen("/proc/cpuinfo", "r");
  if (file != NULL) {
    while (fgets(line, sizeof(line), file) != NULL) {
      char *separator = strchr(line, ':');
      char *value;
      size_t length;

      if (separator == NULL) {
        continue;
      }
      value = separator + 1;
      while (*value == ' ' || *value == '\t') {
        ++value;
      }
      length        = strcspn(value, "\r\n");
      value[length] = '\0';

      if (strncmp(line, "model name", 10) == 0) {
        snprintf(name, capacity, "%s", value);
        break;
      }
      if (fallback[0] == '\0' && (strncmp(line, "Hardware", 8) == 0 || strncmp(line, "Processor", 9) == 0)) {
        snprintf(fallback, sizeof(fallback), "%s", value);
      }
    }
    fclose(file);
  }

  if (name[0] == '\0' && fallback[0] != '\0') {
    snprintf(name, capacity, "%s", fallback);
  }
  if (name[0] == '\0') {
    snprintf(name, capacity, "%s", "Unknown CPU");
  }
}

static uni_sysinfo_cpu_architecture_e uni_sysinfo_linux_architecture(void) {
  return uni_sysinfo_cpu_process_architecture();
}

static uni_sysinfo_cpu_vendor_e uni_sysinfo_linux_vendor(void) {
  uni_sysinfo_cpu_vendor_e vendor = UNI_SYSINFO_CPU_VENDOR_UNKNOWN;
  uni_sysinfo_cpu_x86_detect(NULL, 0, &vendor, NULL);
  return vendor;
}

static void uni_sysinfo_linux_cache(long cpu_id, uni_sysinfo_cpu_cache_t *cache) {
  unsigned int index;

  memset(cache, 0, sizeof(*cache));
  for (index = 0; index < 32; ++index) {
    char path[256];
    char type[32];
    char size_text[64];
    long level;
    size_t size;

    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cache/index%u/level", cpu_id, index);
    if (!uni_sysinfo_linux_read_long(path, &level)) {
      if (index == 0) {
        return;
      }
      break;
    }
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cache/index%u/type", cpu_id, index);
    if (!uni_sysinfo_linux_read_text(path, type, sizeof(type))) {
      continue;
    }
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/cache/index%u/size", cpu_id, index);
    if (!uni_sysinfo_linux_read_text(path, size_text, sizeof(size_text))) {
      continue;
    }
    size = uni_sysinfo_linux_parse_size(size_text);

    if (level == 1 && (strcmp(type, "Data") == 0 || strcmp(type, "Unified") == 0)) {
      if (size > cache->l1d_size)
        cache->l1d_size = size;
    }
    if (level == 1 && (strcmp(type, "Instruction") == 0 || strcmp(type, "Unified") == 0)) {
      if (size > cache->l1i_size)
        cache->l1i_size = size;
    }
    if (level == 2 && size > cache->l2_size)
      cache->l2_size = size;
    if (level == 3 && size > cache->l3_size)
      cache->l3_size = size;
  }
}

static uni_sysinfo_cpu_core_type_e uni_sysinfo_linux_core_type(long cpu_id, unsigned int *raw_class,
                                                               int *raw_class_valid) {
  char path[256];
  long value;

  snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/topology/core_type", cpu_id);
  if (!uni_sysinfo_linux_read_long(path, &value)) {
    return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
  }
  if (value >= 0) {
    *raw_class       = (unsigned int)value;
    *raw_class_valid = 1;
  }
  if (value == 1 || value == 64) {
    return UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE;
  }
  if (value == 2 || value == 32) {
    return UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY;
  }
  if (value == 3) {
    return UNI_SYSINFO_CPU_CORE_TYPE_LOW_POWER;
  }
  return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
}

static uni_sysinfo_cpu_core_type_e uni_sysinfo_linux_cpuid_core_type(long cpu_id, int *restoration_failed) {
#if defined(__i386__) || defined(__x86_64__)
  uni_sysinfo_cpu_core_info_t core     = {0};
  uni_sysinfo_cpu_affinity_t *affinity = NULL;
  uni_sysinfo_cpu_core_type_e result;

  if (cpu_id < 0)
    return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
  *restoration_failed = 0;
  core.os_cpu_id      = (size_t)cpu_id;
  core.allowed        = 1;
  if (uni_sysinfo_cpu_platform_affinity_apply(&core, &affinity) != UNI_SYSINFO_RESULT_OK)
    return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
  result = uni_sysinfo_cpu_x86_current_core_type();
  if (uni_sysinfo_cpu_platform_affinity_restore(affinity) != UNI_SYSINFO_RESULT_OK) {
    result              = UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
    *restoration_failed = 1;
  }
  uni_sysinfo_cpu_platform_affinity_destroy(affinity);
  return result;
#else
  (void)cpu_id;
  *restoration_failed = 0;
  return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
#endif
}

static uni_sysinfo_cpu_package_info_t *uni_sysinfo_linux_get_package(uni_sysinfo_cpu_snapshot_t *snapshot,
                                                                     long package_id, const long *package_ids) {
  size_t index;

  for (index = 0; index < snapshot->package_count; ++index) {
    if (package_ids[index] == package_id) {
      return &snapshot->packages[index];
    }
  }
  return NULL;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_snapshot(uni_sysinfo_cpu_snapshot_t *snapshot) {
  long cpu_id;
  long *online_cpu_ids    = NULL;
  size_t online_cpu_count = 0;
  size_t online_index;
  uni_sysinfo_linux_cpu_t *cpus = NULL;
  size_t cpu_count              = 0;
  long *package_ids             = NULL;
  char cpu_name[UNI_SYSINFO_CPU_NAME_CAPACITY];
  uni_sysinfo_cpu_architecture_e architecture;
  uni_sysinfo_cpu_vendor_e vendor;
  size_t index;

  if (snapshot == NULL) {
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  }
  memset(snapshot, 0, sizeof(*snapshot));

  if (!uni_sysinfo_linux_online_cpu_ids(&online_cpu_ids, &online_cpu_count)) {
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }

  for (online_index = 0; online_index < online_cpu_count; ++online_index) {
    char path[256];
    long package_id = 0;
    long die_id     = 0;
    long core_id;
    int package_id_valid;
    int die_id_valid;
    int core_id_valid;
    uni_sysinfo_linux_cpu_t *resized;

    cpu_id  = online_cpu_ids[online_index];
    core_id = cpu_id;
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/topology/physical_package_id", cpu_id);
    package_id_valid = uni_sysinfo_linux_read_long(path, &package_id);
    if (!package_id_valid) {
      snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld", cpu_id);
      if (access(path, F_OK) != 0) {
        continue;
      }
    }
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/topology/die_id", cpu_id);
    die_id_valid = uni_sysinfo_linux_read_long(path, &die_id);
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%ld/topology/core_id", cpu_id);
    core_id_valid = uni_sysinfo_linux_read_long(path, &core_id);

    resized = (uni_sysinfo_linux_cpu_t *)realloc(cpus, (cpu_count + 1) * sizeof(*cpus));
    if (resized == NULL) {
      free(online_cpu_ids);
      free(cpus);
      return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
    }
    cpus = resized;
    memset(&cpus[cpu_count], 0, sizeof(cpus[cpu_count]));
    cpus[cpu_count].os_cpu_id        = cpu_id;
    cpus[cpu_count].package_id       = package_id;
    cpus[cpu_count].die_id           = die_id;
    cpus[cpu_count].package_id_valid = package_id_valid;
    cpus[cpu_count].die_id_valid     = die_id_valid;
    cpus[cpu_count].core_id          = core_id;
    cpus[cpu_count].core_id_valid    = core_id_valid;
    ++cpu_count;
  }
  free(online_cpu_ids);

  if (cpu_count == 0) {
    free(cpus);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }

  uni_sysinfo_linux_cpu_name(cpu_name, sizeof(cpu_name));
  architecture = uni_sysinfo_linux_architecture();
  vendor       = uni_sysinfo_linux_vendor();

  for (index = 0; index < cpu_count; ++index) {
    size_t previous;
    size_t next_die_id  = 0;
    size_t next_core_id = 0;
    int die_found       = 0;
    int core_found      = 0;

    for (previous = 0; previous < index; ++previous) {
      if (cpus[previous].package_id != cpus[index].package_id)
        continue;
      if (cpus[previous].logical_die_id >= next_die_id)
        next_die_id = cpus[previous].logical_die_id + 1;
      if (cpus[previous].logical_core_id >= next_core_id)
        next_core_id = cpus[previous].logical_core_id + 1;
      if (cpus[previous].die_id == cpus[index].die_id) {
        cpus[index].logical_die_id = cpus[previous].logical_die_id;
        die_found                  = 1;
        if (cpus[previous].core_id == cpus[index].core_id) {
          cpus[index].logical_core_id = cpus[previous].logical_core_id;
          ++cpus[index].thread_id;
          core_found = 1;
        }
      }
    }
    if (!die_found)
      cpus[index].logical_die_id = next_die_id;
    if (!core_found)
      cpus[index].logical_core_id = next_core_id;
  }

  for (index = 0; index < cpu_count; ++index) {
    uni_sysinfo_cpu_package_info_t *package =
        uni_sysinfo_linux_get_package(snapshot, cpus[index].package_id, package_ids);
    uni_sysinfo_cpu_core_info_t *core;
    int allowed_set_valid;

    if (package == NULL) {
      uni_sysinfo_cpu_package_info_t *resized_packages;
      long *resized_ids;

      resized_packages = (uni_sysinfo_cpu_package_info_t *)realloc(snapshot->packages, (snapshot->package_count + 1) *
                                                                                           sizeof(*snapshot->packages));
      if (resized_packages == NULL) {
        free(package_ids);
        free(cpus);
        uni_sysinfo_cpu_snapshot_destroy(snapshot);
        return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
      }
      snapshot->packages = resized_packages;
      memset(&snapshot->packages[snapshot->package_count], 0, sizeof(*snapshot->packages));

      resized_ids = (long *)realloc(package_ids, (snapshot->package_count + 1) * sizeof(*package_ids));
      if (resized_ids == NULL) {
        free(package_ids);
        free(cpus);
        uni_sysinfo_cpu_snapshot_destroy(snapshot);
        return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
      }
      package_ids                          = resized_ids;
      package_ids[snapshot->package_count] = cpus[index].package_id;
      package                              = &snapshot->packages[snapshot->package_count];
      ++snapshot->package_count;
    }

    core = (uni_sysinfo_cpu_core_info_t *)realloc(package->cores, (package->core_count + 1) * sizeof(*package->cores));
    if (core == NULL) {
      free(package_ids);
      free(cpus);
      uni_sysinfo_cpu_snapshot_destroy(snapshot);
      return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
    }
    package->cores = core;
    core           = &package->cores[package->core_count];
    memset(core, 0, sizeof(*core));
    snprintf(core->name, sizeof(core->name), "%s", cpu_name);
    core->architecture         = architecture;
    core->host_architecture    = architecture;
    core->abi                  = uni_sysinfo_cpu_process_abi();
    core->vendor               = vendor;
    core->os_cpu_id            = (size_t)cpus[index].os_cpu_id;
    core->online               = 1;
    core->allowed              = uni_sysinfo_linux_cpu_allowed(cpus[index].os_cpu_id, &allowed_set_valid);
    core->topology.die_idx     = cpus[index].logical_die_id;
    core->topology.hw_core_idx = cpus[index].logical_core_id;
    {
      int raw_class_valid = 0;
      core->topology.core_type =
          uni_sysinfo_linux_core_type(cpus[index].os_cpu_id, &core->raw_efficiency_class, &raw_class_valid);
      if (raw_class_valid)
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_PERFORMANCE_CLASS;
    }
    if (core->topology.core_type == UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN &&
        architecture == UNI_SYSINFO_CPU_ARCHITECTURE_X86) {
      int restoration_failed = 0;
      core->topology.core_type = uni_sysinfo_linux_cpuid_core_type(cpus[index].os_cpu_id, &restoration_failed);
      if (restoration_failed) {
        free(package_ids);
        free(cpus);
        uni_sysinfo_cpu_snapshot_destroy(snapshot);
        return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
      }
    }
    if (core->topology.core_type != UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN)
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_CORE_TYPE;
    core->topology.smt_idx = cpus[index].thread_id;
    uni_sysinfo_linux_cache(cpus[index].os_cpu_id, &core->cache);
    core->valid_fields |=
        UNI_SYSINFO_CPU_VALID_NAME | UNI_SYSINFO_CPU_VALID_ARCHITECTURE | UNI_SYSINFO_CPU_VALID_NATIVE_ID;
    if (allowed_set_valid)
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_ALLOWED_SET;
    if (cpus[index].package_id_valid && cpus[index].core_id_valid)
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_TOPOLOGY;
    if (cpus[index].package_id_valid && cpus[index].core_id_valid && cpus[index].die_id_valid)
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_DIE_TOPOLOGY;
    if (core->vendor != UNI_SYSINFO_CPU_VENDOR_UNKNOWN)
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_VENDOR;
    if (core->cache.l1d_size != 0 || core->cache.l1i_size != 0 || core->cache.l2_size != 0 || core->cache.l3_size != 0)
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_CACHE;
    if (uni_sysinfo_linux_numa_node(cpus[index].os_cpu_id, &core->numa_node_idx))
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_NUMA_NODE;
    if (uni_sysinfo_linux_max_frequency(cpus[index].os_cpu_id, &core->max_frequency_hz))
      core->valid_fields |= UNI_SYSINFO_CPU_VALID_MAX_FREQUENCY;
    ++package->core_count;
  }

  free(package_ids);
  free(cpus);
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_aarch64_features(uni_sysinfo_cpu_aarch64_features_t *result) {
  if (result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *result = 0;
#if defined(__aarch64__)
  {
    const unsigned long hwcap  = getauxval(AT_HWCAP);
    const unsigned long hwcap2 = getauxval(AT_HWCAP2);
#ifdef HWCAP_ASIMD
    if ((hwcap & HWCAP_ASIMD) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_NEON;
#endif
#ifdef HWCAP_AES
    if ((hwcap & HWCAP_AES) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_AES;
#endif
#ifdef HWCAP_PMULL
    if ((hwcap & HWCAP_PMULL) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_PMULL;
#endif
#ifdef HWCAP_SHA1
    if ((hwcap & HWCAP_SHA1) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SHA1;
#endif
#ifdef HWCAP_SHA2
    if ((hwcap & HWCAP_SHA2) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SHA2;
#endif
#ifdef HWCAP_CRC32
    if ((hwcap & HWCAP_CRC32) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_CRC32;
#endif
#ifdef HWCAP_ATOMICS
    if ((hwcap & HWCAP_ATOMICS) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_ATOMICS;
#endif
#ifdef HWCAP_FPHP
    if ((hwcap & HWCAP_FPHP) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_FP16_SCALAR;
#endif
#ifdef HWCAP_ASIMDHP
    if ((hwcap & HWCAP_ASIMDHP) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_FP16_SIMD;
#endif
#ifdef HWCAP_ASIMDDP
    if ((hwcap & HWCAP_ASIMDDP) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_DOTPROD;
#endif
#ifdef HWCAP_SVE
    if ((hwcap & HWCAP_SVE) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SVE;
#endif
#ifdef HWCAP2_SVE2
    if ((hwcap2 & HWCAP2_SVE2) != 0)
      *result |= UNI_SYSINFO_CPU_AARCH64_FEATURE_SVE2;
#else
    (void)hwcap2;
#endif
  }
  return UNI_SYSINFO_RESULT_OK;
#else
  return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
#endif
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_current_frequency(const uni_sysinfo_cpu_core_info_t *core,
                                                                uint64_t *result_hz) {
  char path[128];
  long khz = 0;

  *result_hz = 0;
  if ((core->valid_fields & UNI_SYSINFO_CPU_VALID_NATIVE_ID) == 0)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%zu/cpufreq/scaling_cur_freq", core->os_cpu_id);
  if (!uni_sysinfo_linux_read_long(path, &khz) || khz <= 0)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  *result_hz = (uint64_t)khz * UINT64_C(1000);
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_apply(const uni_sysinfo_cpu_core_info_t *core,
                                                             uni_sysinfo_cpu_affinity_t **previous) {
  cpu_set_t *target;
  uni_sysinfo_cpu_affinity_t *state;

  if (core == NULL || previous == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *previous = NULL;
  state     = (uni_sysinfo_cpu_affinity_t *)calloc(1, sizeof(*state));
  if (state == NULL)
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  if (!uni_sysinfo_linux_get_affinity(core->os_cpu_id + 1, &state->set, &state->set_size, &state->cpu_count)) {
    free(state);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }
  if (!CPU_ISSET_S(core->os_cpu_id, state->set_size, state->set)) {
    uni_sysinfo_cpu_platform_affinity_destroy(state);
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  }
  target = CPU_ALLOC(state->cpu_count);
  if (target == NULL) {
    uni_sysinfo_cpu_platform_affinity_destroy(state);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }
  CPU_ZERO_S(state->set_size, target);
  CPU_SET_S(core->os_cpu_id, state->set_size, target);
  if (sched_setaffinity(0, state->set_size, target) != 0) {
    uni_sysinfo_cpu_platform_affinity_destroy(state);
    CPU_FREE(target);
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;
  }
  CPU_FREE(target);
  *previous = state;
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_restore(uni_sysinfo_cpu_affinity_t *previous) {
  if (previous == NULL || previous->set == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  return sched_setaffinity(0, previous->set_size, previous->set) == 0 ? UNI_SYSINFO_RESULT_OK
                                                                      : UNI_SYSINFO_RESULT_SYSTEM_ERROR;
}

void uni_sysinfo_cpu_platform_affinity_destroy(uni_sysinfo_cpu_affinity_t *affinity) {
  if (affinity == NULL)
    return;
  CPU_FREE(affinity->set);
  free(affinity);
}
