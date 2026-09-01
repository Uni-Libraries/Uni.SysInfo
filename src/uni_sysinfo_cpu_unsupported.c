#include "uni_sysinfo_cpu_internal.h"

#include <string.h>

struct uni_sysinfo_cpu_affinity {
  int unused;
};

uni_sysinfo_result_e uni_sysinfo_cpu_platform_snapshot(uni_sysinfo_cpu_snapshot_t *snapshot) {
  if (snapshot == NULL) {
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  }
  memset(snapshot, 0, sizeof(*snapshot));
  return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_aarch64_features(uni_sysinfo_cpu_aarch64_features_t *result) {
  if (result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *result = 0;
  return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_apply(const uni_sysinfo_cpu_core_info_t *core,
                                                             uni_sysinfo_cpu_affinity_t **previous) {
  (void)core;
  if (previous == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  *previous = NULL;
  return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
}

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_restore(uni_sysinfo_cpu_affinity_t *previous) {
  (void)previous;
  return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
}

void uni_sysinfo_cpu_platform_affinity_destroy(uni_sysinfo_cpu_affinity_t *affinity) { (void)affinity; }
