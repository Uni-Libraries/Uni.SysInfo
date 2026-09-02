#include "uni_sysinfo_cpu_internal.h"

uni_sysinfo_result_e uni_sysinfo_cpu_aarch64_info(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t logical_index,
                                                  uni_sysinfo_cpu_aarch64_info_t *result) {
  const uni_sysinfo_cpu_core_info_t *core;
  uni_sysinfo_result_e status;

  if (result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  result->features = 0;
  status           = uni_sysinfo_cpu_snapshot_get_core(snapshot, logical_index, NULL, NULL, &core);
  if (status != UNI_SYSINFO_RESULT_OK)
    return status;
  if (core->architecture != UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  return uni_sysinfo_cpu_platform_aarch64_features(&result->features);
}

uni_sysinfo_result_e uni_sysinfo_cpu_aarch64_features_common(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                             uni_sysinfo_cpu_aarch64_features_t *result) {
  uni_sysinfo_result_e status;
  size_t logical_count;
  size_t logical_index;
  int found = 0;

  if (snapshot == NULL || result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *result = 0;
  status  = uni_sysinfo_cpu_snapshot_logical_count(snapshot, &logical_count);
  if (status != UNI_SYSINFO_RESULT_OK)
    return status;
  for (logical_index = 0; logical_index < logical_count; ++logical_index) {
    const uni_sysinfo_cpu_core_info_t *core;
    status = uni_sysinfo_cpu_snapshot_get_core(snapshot, logical_index, NULL, NULL, &core);
    if (status != UNI_SYSINFO_RESULT_OK)
      return status;
    if (core->architecture != UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64)
      continue;
    if ((core->valid_fields & UNI_SYSINFO_CPU_VALID_ALLOWED_SET) != 0 && !core->allowed)
      continue;
    found = 1;
  }
  if (!found)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  return uni_sysinfo_cpu_platform_aarch64_features(result);
}
