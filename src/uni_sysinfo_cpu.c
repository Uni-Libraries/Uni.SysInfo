#include "uni_sysinfo_cpu_internal.h"

#include <stdlib.h>
#include <string.h>

uni_sysinfo_cpu_architecture_e uni_sysinfo_cpu_process_architecture(void) {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
  return UNI_SYSINFO_CPU_ARCHITECTURE_X86;
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
  return UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64;
#elif defined(__arm__) || defined(_M_ARM)
  return UNI_SYSINFO_CPU_ARCHITECTURE_ARM;
#elif defined(__riscv)
  return UNI_SYSINFO_CPU_ARCHITECTURE_RISCV;
#elif defined(__powerpc__) || defined(__powerpc64__)
  return UNI_SYSINFO_CPU_ARCHITECTURE_POWER;
#elif defined(__loongarch__)
  return UNI_SYSINFO_CPU_ARCHITECTURE_LOONGARCH;
#elif defined(__wasm__)
  return UNI_SYSINFO_CPU_ARCHITECTURE_WASM;
#else
  return UNI_SYSINFO_CPU_ARCHITECTURE_UNKNOWN;
#endif
}

uni_sysinfo_cpu_abi_e uni_sysinfo_cpu_process_abi(void) {
#if defined(__x86_64__) || defined(_M_X64)
  return UNI_SYSINFO_CPU_ABI_X86_64;
#elif defined(__i386__) || defined(_M_IX86)
  return UNI_SYSINFO_CPU_ABI_X86_32;
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
  return UNI_SYSINFO_CPU_ABI_AARCH64;
#elif defined(__arm__) || defined(_M_ARM)
  return UNI_SYSINFO_CPU_ABI_ARM_32;
#elif defined(__riscv) && __riscv_xlen == 64
  return UNI_SYSINFO_CPU_ABI_RISCV_64;
#elif defined(__riscv) && __riscv_xlen == 32
  return UNI_SYSINFO_CPU_ABI_RISCV_32;
#elif defined(__powerpc64__)
  return UNI_SYSINFO_CPU_ABI_POWER_64;
#elif defined(__powerpc__)
  return UNI_SYSINFO_CPU_ABI_POWER_32;
#elif defined(__loongarch64)
  return UNI_SYSINFO_CPU_ABI_LOONGARCH_64;
#elif defined(__wasm64__)
  return UNI_SYSINFO_CPU_ABI_WASM_64;
#elif defined(__wasm32__)
  return UNI_SYSINFO_CPU_ABI_WASM_32;
#else
  return UNI_SYSINFO_CPU_ABI_UNKNOWN;
#endif
}

void uni_sysinfo_cpu_snapshot_destroy(uni_sysinfo_cpu_snapshot_t *snapshot) {
  size_t index;
  int heap_owned;

  if (snapshot == NULL) {
    return;
  }
  heap_owned = snapshot->heap_owned;

  if (snapshot->packages != NULL) {
    for (index = 0; index < snapshot->package_count; ++index) {
      free(snapshot->packages[index].cores);
    }
  }
  free(snapshot->packages);
  snapshot->packages      = NULL;
  snapshot->package_count = 0;
  if (heap_owned) {
    free(snapshot);
  }
}

uni_sysinfo_result_e uni_sysinfo_cpu_snapshot_get_core(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t logical_index,
                                                       size_t *package_index_out, size_t *package_logical_index_out,
                                                       const uni_sysinfo_cpu_core_info_t **core_out) {
  size_t package_index;

  if (snapshot == NULL || core_out == NULL) {
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  }
  for (package_index = 0; package_index < snapshot->package_count; ++package_index) {
    const size_t count = snapshot->packages[package_index].core_count;
    if (logical_index < count) {
      if (package_index_out != NULL)
        *package_index_out = package_index;
      if (package_logical_index_out != NULL)
        *package_logical_index_out = logical_index;
      *core_out = &snapshot->packages[package_index].cores[logical_index];
      return UNI_SYSINFO_RESULT_OK;
    }
    logical_index -= count;
  }
  return UNI_SYSINFO_RESULT_OUT_OF_RANGE;
}

uni_sysinfo_result_e uni_sysinfo_cpu_snapshot_create(uni_sysinfo_cpu_snapshot_t **result) {
  uni_sysinfo_cpu_snapshot_t *snapshot;
  uni_sysinfo_result_e status;
  size_t package_index;

  if (result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *result  = NULL;
  snapshot = (uni_sysinfo_cpu_snapshot_t *)calloc(1, sizeof(*snapshot));
  if (snapshot == NULL)
    return UNI_SYSINFO_RESULT_SYSTEM_ERROR;

  status = uni_sysinfo_cpu_platform_snapshot(snapshot);
  if (status != UNI_SYSINFO_RESULT_OK) {
    uni_sysinfo_cpu_snapshot_destroy(snapshot);
    free(snapshot);
    return status;
  }
  snapshot->heap_owned = 1;

  for (package_index = 0; package_index < snapshot->package_count; ++package_index) {
    size_t core_index;
    for (core_index = 0; core_index < snapshot->packages[package_index].core_count; ++core_index) {
      uni_sysinfo_cpu_core_info_t *core = &snapshot->packages[package_index].cores[core_index];

      if (core->architecture == UNI_SYSINFO_CPU_ARCHITECTURE_UNKNOWN)
        core->architecture = uni_sysinfo_cpu_process_architecture();
      if (core->host_architecture == UNI_SYSINFO_CPU_ARCHITECTURE_UNKNOWN)
        core->host_architecture = core->architecture;
      if (core->abi == UNI_SYSINFO_CPU_ABI_UNKNOWN)
        core->abi = uni_sysinfo_cpu_process_abi();
      if (core->architecture != UNI_SYSINFO_CPU_ARCHITECTURE_UNKNOWN)
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_ARCHITECTURE;
      else
        core->valid_fields &= ~UNI_SYSINFO_CPU_VALID_ARCHITECTURE;
      if (core->name[0] != '\0')
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_NAME;
      if (core->vendor != UNI_SYSINFO_CPU_VENDOR_UNKNOWN)
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_VENDOR;

      if ((core->valid_fields & UNI_SYSINFO_CPU_VALID_MAX_FREQUENCY) != 0 && core->performance_rank == 0) {
        uint64_t rank          = core->max_frequency_hz / UINT64_C(1000000);
        core->performance_rank = rank > UINT32_MAX ? UINT32_MAX : (unsigned int)rank;
        core->valid_fields |= UNI_SYSINFO_CPU_VALID_PERFORMANCE_CLASS;
      } else if (core->performance_rank == 0) {
        if (core->topology.core_type == UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE)
          core->performance_rank = 3;
        else if (core->topology.core_type == UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY)
          core->performance_rank = 2;
        else if (core->topology.core_type == UNI_SYSINFO_CPU_CORE_TYPE_LOW_POWER)
          core->performance_rank = 1;
        if (core->performance_rank != 0)
          core->valid_fields |= UNI_SYSINFO_CPU_VALID_PERFORMANCE_CLASS;
      }
    }
  }

  *result = snapshot;
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_snapshot_logical_count(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                            size_t *result) {
  size_t package_index;
  size_t count = 0;

  if (snapshot == NULL || result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  for (package_index = 0; package_index < snapshot->package_count; ++package_index)
    count += snapshot->packages[package_index].core_count;
  *result = count;
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_snapshot_package_count(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                            size_t *result) {
  if (snapshot == NULL || result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *result = snapshot->package_count;
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_snapshot_logical_processor(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                                size_t logical_index,
                                                                uni_sysinfo_cpu_logical_processor_t *result) {
  const uni_sysinfo_cpu_core_info_t *core;
  size_t package_index;
  size_t package_logical_index;
  uni_sysinfo_result_e status;

  if (result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  status = uni_sysinfo_cpu_snapshot_get_core(snapshot, logical_index, &package_index, &package_logical_index, &core);
  if (status != UNI_SYSINFO_RESULT_OK)
    return status;

  memset(result, 0, sizeof(*result));
  result->package_idx          = package_index;
  result->package_logical_idx  = package_logical_index;
  result->os_cpu_id            = core->os_cpu_id;
  result->os_group             = core->os_group;
  result->online               = core->online;
  result->allowed              = core->allowed;
  result->numa_node_idx        = core->numa_node_idx;
  result->max_frequency_hz     = core->max_frequency_hz;
  result->performance_rank     = core->performance_rank;
  result->raw_efficiency_class = core->raw_efficiency_class;
  result->architecture         = core->architecture;
  result->host_architecture    = core->host_architecture;
  result->abi                  = core->abi;
  result->vendor               = core->vendor;
  result->cache                = core->cache;
  result->topology             = core->topology;
  result->valid_fields         = core->valid_fields;
  memcpy(result->name, core->name, sizeof(result->name));
  result->name[sizeof(result->name) - 1] = '\0';
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_affinity_apply(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t logical_index,
                                                    uni_sysinfo_cpu_affinity_t **previous) {
  const uni_sysinfo_cpu_core_info_t *core;
  uni_sysinfo_result_e status;

  if (previous == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *previous = NULL;
  status    = uni_sysinfo_cpu_snapshot_get_core(snapshot, logical_index, NULL, NULL, &core);
  if (status != UNI_SYSINFO_RESULT_OK)
    return status;
  if ((core->valid_fields & UNI_SYSINFO_CPU_VALID_ALLOWED_SET) != 0 && !core->allowed)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  return uni_sysinfo_cpu_platform_affinity_apply(core, previous);
}

uni_sysinfo_result_e uni_sysinfo_cpu_affinity_restore(uni_sysinfo_cpu_affinity_t *previous) {
  if (previous == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  return uni_sysinfo_cpu_platform_affinity_restore(previous);
}

void uni_sysinfo_cpu_affinity_destroy(uni_sysinfo_cpu_affinity_t *affinity) {
  uni_sysinfo_cpu_platform_affinity_destroy(affinity);
}
