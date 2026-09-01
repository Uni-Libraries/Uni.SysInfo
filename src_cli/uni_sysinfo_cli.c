#include "uni_sysinfo.h"
#include "uni_sysinfo_cpu_x86.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UNI_SYSINFO_VERSION
#define UNI_SYSINFO_VERSION "unknown"
#endif

typedef struct {
  size_t api_index;
  char name[128];
  uni_sysinfo_cpu_architecture_e architecture;
  uni_sysinfo_cpu_vendor_e vendor;
  uni_sysinfo_cpu_cache_t cache;
  uni_sysinfo_cpu_topology_t topology;
  uni_sysinfo_cpu_x86_features_t x86_features;
  uni_sysinfo_result_e name_status;
  uni_sysinfo_result_e architecture_status;
  uni_sysinfo_result_e vendor_status;
  uni_sysinfo_result_e cache_status;
  uni_sysinfo_result_e topology_status;
  uni_sysinfo_result_e features_status;
} cpu_thread_info_t;

typedef struct {
  uni_sysinfo_cpu_x86_features_t flag;
  const char *name;
} x86_feature_name_t;

static const x86_feature_name_t X86_FEATURE_NAMES[] = {
    {UNI_SYSINFO_CPU_X86_FEATURE_SSE, "SSE"},
    {UNI_SYSINFO_CPU_X86_FEATURE_SSE2, "SSE2"},
    {UNI_SYSINFO_CPU_X86_FEATURE_SSSE3, "SSSE3"},
    {UNI_SYSINFO_CPU_X86_FEATURE_FMA, "FMA"},
    {UNI_SYSINFO_CPU_X86_FEATURE_SSE41, "SSE4.1"},
    {UNI_SYSINFO_CPU_X86_FEATURE_SSE42, "SSE4.2"},
    {UNI_SYSINFO_CPU_X86_FEATURE_SSE4A, "SSE4A"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX, "AVX"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX2, "AVX2"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512F, "AVX512F"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512DQ, "AVX512DQ"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512IFMA, "AVX512IFMA"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512CD, "AVX512CD"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512BW, "AVX512BW"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512VL, "AVX512VL"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512BF16, "AVX512BF16"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512VBMI, "AVX512VBMI"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AES, "AES"},
    {UNI_SYSINFO_CPU_X86_FEATURE_PCLMULQDQ, "PCLMULQDQ"},
    {UNI_SYSINFO_CPU_X86_FEATURE_POPCNT, "POPCNT"},
    {UNI_SYSINFO_CPU_X86_FEATURE_F16C, "F16C"},
    {UNI_SYSINFO_CPU_X86_FEATURE_BMI1, "BMI1"},
    {UNI_SYSINFO_CPU_X86_FEATURE_BMI2, "BMI2"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512VBMI2, "AVX512VBMI2"},
    {UNI_SYSINFO_CPU_X86_FEATURE_AVX512VNNI, "AVX512VNNI"},
};

static const char *result_name(uni_sysinfo_result_e result) {
  switch (result) {
  case UNI_SYSINFO_RESULT_OK:
    return "ok";
  case UNI_SYSINFO_RESULT_INVALID_ARGUMENT:
    return "invalid argument";
  case UNI_SYSINFO_RESULT_OUT_OF_RANGE:
    return "out of range";
  case UNI_SYSINFO_RESULT_BUFFER_TOO_SMALL:
    return "buffer too small";
  case UNI_SYSINFO_RESULT_NOT_SUPPORTED:
    return "not supported";
  case UNI_SYSINFO_RESULT_SYSTEM_ERROR:
    return "system error";
  case UNI_SYSINFO_RESULT_UNKNOWN:
  default:
    return "unknown error";
  }
}

static const char *architecture_name(uni_sysinfo_cpu_architecture_e architecture) {
  switch (architecture) {
  case UNI_SYSINFO_CPU_ARCHITECTURE_X86:
    return "x86";
  case UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64:
    return "AArch64";
  case UNI_SYSINFO_CPU_ARCHITECTURE_ARM:
    return "ARM";
  case UNI_SYSINFO_CPU_ARCHITECTURE_RISCV:
    return "RISC-V";
  case UNI_SYSINFO_CPU_ARCHITECTURE_POWER:
    return "Power";
  case UNI_SYSINFO_CPU_ARCHITECTURE_LOONGARCH:
    return "LoongArch";
  case UNI_SYSINFO_CPU_ARCHITECTURE_WASM:
    return "WebAssembly";
  case UNI_SYSINFO_CPU_ARCHITECTURE_UNKNOWN:
  default:
    return "unknown";
  }
}

static const char *vendor_name(uni_sysinfo_cpu_vendor_e vendor) {
  switch (vendor) {
  case UNI_SYSINFO_CPU_VENDOR_INTEL:
    return "Intel";
  case UNI_SYSINFO_CPU_VENDOR_AMD:
    return "AMD";
  case UNI_SYSINFO_CPU_VENDOR_APPLE:
    return "Apple";
  case UNI_SYSINFO_CPU_VENDOR_UNKNOWN:
  default:
    return "unknown";
  }
}

static const char *core_type_name(uni_sysinfo_cpu_core_type_e core_type) {
  switch (core_type) {
  case UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE:
    return "performance";
  case UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY:
    return "efficiency";
  case UNI_SYSINFO_CPU_CORE_TYPE_LOW_POWER:
    return "low power";
  case UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN:
  default:
    return "unknown";
  }
}

static void format_size(size_t bytes, char *output, size_t capacity) {
  const size_t kibibyte = 1024u;
  const size_t mebibyte = 1024u * 1024u;
  const size_t gibibyte = 1024u * 1024u * 1024u;

  if (bytes == 0) {
    snprintf(output, capacity, "unknown");
  } else if (bytes % gibibyte == 0) {
    snprintf(output, capacity, "%zu GiB", bytes / gibibyte);
  } else if (bytes % mebibyte == 0) {
    snprintf(output, capacity, "%zu MiB", bytes / mebibyte);
  } else if (bytes % kibibyte == 0) {
    snprintf(output, capacity, "%zu KiB", bytes / kibibyte);
  } else {
    snprintf(output, capacity, "%zu B", bytes);
  }
}

static void format_x86_features(uni_sysinfo_cpu_x86_features_t features, char *output, size_t capacity) {
  size_t feature_index;
  size_t used = 0;
  int first   = 1;

  output[0] = '\0';
  for (feature_index = 0; feature_index < sizeof(X86_FEATURE_NAMES) / sizeof(X86_FEATURE_NAMES[0]); ++feature_index) {
    int written;
    if ((features & X86_FEATURE_NAMES[feature_index].flag) == 0)
      continue;
    written =
        snprintf(output + used, capacity - used, "%s%s", first ? "" : ", ", X86_FEATURE_NAMES[feature_index].name);
    if (written < 0 || (size_t)written >= capacity - used) {
      output[capacity - 1] = '\0';
      return;
    }
    used += (size_t)written;
    first = 0;
  }
  if (first)
    snprintf(output, capacity, "none");
}

static void tree_line(size_t depth, const int *ancestor_is_last, int is_last, const char *format, ...) {
  size_t level;
  va_list arguments;

  for (level = 0; level < depth; ++level) {
    fputs(ancestor_is_last[level] ? "    " : "|   ", stdout);
  }
  fputs(is_last ? "`-- " : "|-- ", stdout);
  va_start(arguments, format);
  vprintf(format, arguments);
  va_end(arguments);
  putchar('\n');
}

static int compare_threads(const void *left_value, const void *right_value) {
  const cpu_thread_info_t *left  = (const cpu_thread_info_t *)left_value;
  const cpu_thread_info_t *right = (const cpu_thread_info_t *)right_value;

#define COMPARE_FIELD(field)                                                                                           \
  do {                                                                                                                 \
    if (left->field < right->field)                                                                                    \
      return -1;                                                                                                       \
    if (left->field > right->field)                                                                                    \
      return 1;                                                                                                        \
  } while (0)

  COMPARE_FIELD(topology.die_idx);
  COMPARE_FIELD(topology.hw_core_idx);
  COMPARE_FIELD(topology.smt_idx);
  COMPARE_FIELD(api_index);
#undef COMPARE_FIELD
  return 0;
}

static void query_thread(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t logical_index, cpu_thread_info_t *thread) {
  uni_sysinfo_cpu_logical_processor_t processor;
  uni_sysinfo_result_e status;

  memset(thread, 0, sizeof(*thread));
  status = uni_sysinfo_cpu_snapshot_logical_processor(snapshot, logical_index, &processor);
  if (status != UNI_SYSINFO_RESULT_OK) {
    thread->name_status         = status;
    thread->architecture_status = status;
    thread->vendor_status       = status;
    thread->cache_status        = status;
    thread->topology_status     = status;
    thread->features_status     = status;
    return;
  }

  thread->api_index = processor.package_logical_idx;
  memcpy(thread->name, processor.name, sizeof(thread->name));
  thread->architecture = processor.architecture;
  thread->vendor       = processor.vendor;
  thread->cache        = processor.cache;
  thread->topology     = processor.topology;
  thread->name_status  = (processor.valid_fields & UNI_SYSINFO_CPU_VALID_NAME) != 0 ? UNI_SYSINFO_RESULT_OK
                                                                                    : UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  thread->architecture_status = (processor.valid_fields & UNI_SYSINFO_CPU_VALID_ARCHITECTURE) != 0
                                    ? UNI_SYSINFO_RESULT_OK
                                    : UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  thread->vendor_status       = (processor.valid_fields & UNI_SYSINFO_CPU_VALID_VENDOR) != 0
                                    ? UNI_SYSINFO_RESULT_OK
                                    : UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  thread->cache_status = (processor.valid_fields & UNI_SYSINFO_CPU_VALID_CACHE) != 0 ? UNI_SYSINFO_RESULT_OK
                                                                                     : UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  thread->topology_status = (processor.valid_fields & UNI_SYSINFO_CPU_VALID_TOPOLOGY) != 0
                                ? UNI_SYSINFO_RESULT_OK
                                : UNI_SYSINFO_RESULT_NOT_SUPPORTED;

  if (thread->topology_status != UNI_SYSINFO_RESULT_OK) {
    thread->topology.die_idx     = 0;
    thread->topology.hw_core_idx = thread->api_index;
    thread->topology.smt_idx     = 0;
  }
  if (thread->architecture_status == UNI_SYSINFO_RESULT_OK &&
      thread->architecture == UNI_SYSINFO_CPU_ARCHITECTURE_X86) {
    uni_sysinfo_cpu_x86_info_t x86_info;
    thread->features_status = uni_sysinfo_cpu_x86_info(snapshot, logical_index, &x86_info);
    if (thread->features_status == UNI_SYSINFO_RESULT_OK &&
        (x86_info.valid_fields & UNI_SYSINFO_CPU_X86_VALID_FEATURES) != 0) {
      thread->x86_features = x86_info.features;
    } else if (thread->features_status == UNI_SYSINFO_RESULT_OK) {
      thread->features_status = UNI_SYSINFO_RESULT_NOT_SUPPORTED;
    }
  } else {
    thread->features_status = UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  }
}

static void print_thread_details(const cpu_thread_info_t *thread, size_t depth, int *ancestor_is_last) {
  char l1d[32];
  char l1i[32];
  char l2[32];
  char l3[32];
  char features[512];

  tree_line(depth, ancestor_is_last, 0, "Name: %s",
            thread->name_status == UNI_SYSINFO_RESULT_OK ? thread->name : result_name(thread->name_status));
  tree_line(depth, ancestor_is_last, 0, "Architecture: %s",
            thread->architecture_status == UNI_SYSINFO_RESULT_OK ? architecture_name(thread->architecture)
                                                                 : result_name(thread->architecture_status));
  tree_line(depth, ancestor_is_last, 0, "Vendor: %s",
            thread->vendor_status == UNI_SYSINFO_RESULT_OK ? vendor_name(thread->vendor)
                                                           : result_name(thread->vendor_status));
  tree_line(depth, ancestor_is_last, 0, "Core type: %s",
            thread->topology_status == UNI_SYSINFO_RESULT_OK ? core_type_name(thread->topology.core_type)
                                                             : result_name(thread->topology_status));

  if (thread->cache_status == UNI_SYSINFO_RESULT_OK) {
    format_size(thread->cache.l1d_size, l1d, sizeof(l1d));
    format_size(thread->cache.l1i_size, l1i, sizeof(l1i));
    format_size(thread->cache.l2_size, l2, sizeof(l2));
    format_size(thread->cache.l3_size, l3, sizeof(l3));
    tree_line(depth, ancestor_is_last, 0, "Cache: L1D %s, L1I %s, L2 %s, L3 %s", l1d, l1i, l2, l3);
  } else {
    tree_line(depth, ancestor_is_last, 0, "Cache: %s", result_name(thread->cache_status));
  }

  if (thread->features_status == UNI_SYSINFO_RESULT_OK) {
    format_x86_features(thread->x86_features, features, sizeof(features));
    tree_line(depth, ancestor_is_last, 1, "x86 features: %s", features);
  } else {
    tree_line(depth, ancestor_is_last, 1, "x86 features: %s", result_name(thread->features_status));
  }
}

static void print_package(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t package_index, size_t package_count,
                          size_t logical_count, int *ancestor_is_last) {
  size_t thread_count = 0;
  cpu_thread_info_t *threads;
  size_t thread_index;
  size_t die_begin;

  ancestor_is_last[0] = package_index + 1 == package_count;
  tree_line(0, ancestor_is_last, ancestor_is_last[0], "Package %zu", package_index);
  for (thread_index = 0; thread_index < logical_count; ++thread_index) {
    uni_sysinfo_cpu_logical_processor_t processor;
    if (uni_sysinfo_cpu_snapshot_logical_processor(snapshot, thread_index, &processor) == UNI_SYSINFO_RESULT_OK &&
        processor.package_idx == package_index) {
      ++thread_count;
    }
  }
  tree_line(1, ancestor_is_last, thread_count == 0, "Logical processors: %zu", thread_count);
  if (thread_count == 0)
    return;

  threads = (cpu_thread_info_t *)calloc(thread_count, sizeof(*threads));
  if (threads == NULL) {
    tree_line(1, ancestor_is_last, 1, "Unable to allocate CPU information");
    return;
  }
  {
    size_t output_index = 0;
    for (thread_index = 0; thread_index < logical_count; ++thread_index) {
      uni_sysinfo_cpu_logical_processor_t processor;
      if (uni_sysinfo_cpu_snapshot_logical_processor(snapshot, thread_index, &processor) == UNI_SYSINFO_RESULT_OK &&
          processor.package_idx == package_index) {
        query_thread(snapshot, thread_index, &threads[output_index++]);
      }
    }
  }
  qsort(threads, thread_count, sizeof(*threads), compare_threads);

  die_begin = 0;
  while (die_begin < thread_count) {
    size_t die_end = die_begin + 1;
    size_t core_begin;
    while (die_end < thread_count && threads[die_end].topology.die_idx == threads[die_begin].topology.die_idx) {
      ++die_end;
    }
    ancestor_is_last[1] = die_end == thread_count;
    tree_line(1, ancestor_is_last, ancestor_is_last[1], "Die %zu", threads[die_begin].topology.die_idx);

    core_begin = die_begin;
    while (core_begin < die_end) {
      size_t core_end = core_begin + 1;
      while (core_end < die_end && threads[core_end].topology.hw_core_idx == threads[core_begin].topology.hw_core_idx) {
        ++core_end;
      }
      ancestor_is_last[2] = core_end == die_end;
      tree_line(2, ancestor_is_last, ancestor_is_last[2], "Core %zu", threads[core_begin].topology.hw_core_idx);

      for (thread_index = core_begin; thread_index < core_end; ++thread_index) {
        ancestor_is_last[3] = thread_index + 1 == core_end;
        tree_line(3, ancestor_is_last, ancestor_is_last[3], "Thread %zu [core_index=%zu]",
                  threads[thread_index].topology.smt_idx, threads[thread_index].api_index);
        print_thread_details(&threads[thread_index], 4, ancestor_is_last);
      }
      core_begin = core_end;
    }
    die_begin = die_end;
  }
  free(threads);
}

static void print_usage(const char *program_name) {
  printf("Usage: %s [--help] [--version]\n", program_name);
  puts("Print all CPU information available through Uni.SysInfo.");
}

int main(int argc, char **argv) {
  uni_sysinfo_cpu_snapshot_t *snapshot = NULL;
  size_t logical_count                 = 0;
  size_t package_count                 = 0;
  size_t package_index;
  uni_sysinfo_result_e status;
  int ancestor_is_last[5] = {0};

  if (argc > 1) {
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
      print_usage(argv[0]);
      return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
      printf("uni-sysinfo %s\n", UNI_SYSINFO_VERSION);
      return EXIT_SUCCESS;
    }
    fprintf(stderr, "Unknown option. Use --help for usage.\n");
    return EXIT_FAILURE;
  }

  status = uni_sysinfo_cpu_snapshot_create(&snapshot);
  if (status != UNI_SYSINFO_RESULT_OK) {
    fprintf(stderr, "Unable to query CPU snapshot: %s\n", result_name(status));
    return EXIT_FAILURE;
  }
  status = uni_sysinfo_cpu_snapshot_logical_count(snapshot, &logical_count);
  if (status != UNI_SYSINFO_RESULT_OK) {
    uni_sysinfo_cpu_snapshot_destroy(snapshot);
    fprintf(stderr, "Unable to query logical processors: %s\n", result_name(status));
    return EXIT_FAILURE;
  }
  status = uni_sysinfo_cpu_snapshot_package_count(snapshot, &package_count);
  if (status != UNI_SYSINFO_RESULT_OK) {
    uni_sysinfo_cpu_snapshot_destroy(snapshot);
    fprintf(stderr, "Unable to query CPU packages: %s\n", result_name(status));
    return EXIT_FAILURE;
  }

  puts("CPU");
  tree_line(0, ancestor_is_last, package_count == 0, "Packages: %zu", package_count);
  for (package_index = 0; package_index < package_count; ++package_index) {
    print_package(snapshot, package_index, package_count, logical_count, ancestor_is_last);
  }
  uni_sysinfo_cpu_snapshot_destroy(snapshot);
  return EXIT_SUCCESS;
}
