#include "uni_sysinfo.h"
#include "uni_sysinfo_cpu_aarch64.h"
#include "uni_sysinfo_cpu_x86.h"

#include <stddef.h>

#define CHECK(expression)                                                                                              \
  do {                                                                                                                 \
    if (!(expression))                                                                                                 \
      return __LINE__;                                                                                                 \
  } while (0)

int main(void) {
  uni_sysinfo_cpu_snapshot_t *snapshot = NULL;
  uni_sysinfo_cpu_logical_processor_t processor;
  size_t package_count = 0;
  size_t logical_count = 0;
  size_t logical_index;
  size_t allowed_count                            = 0;
  size_t allowed_x86_count                        = 0;
  uni_sysinfo_cpu_x86_features_t x86_intersection = UINT64_MAX;

  CHECK(uni_sysinfo_cpu_snapshot_create(&snapshot) == UNI_SYSINFO_RESULT_OK);
  CHECK(snapshot != NULL);
  CHECK(uni_sysinfo_cpu_snapshot_package_count(snapshot, &package_count) == UNI_SYSINFO_RESULT_OK);
  CHECK(package_count > 0);
  CHECK(uni_sysinfo_cpu_snapshot_logical_count(snapshot, &logical_count) == UNI_SYSINFO_RESULT_OK);
  CHECK(logical_count > 0);

  for (logical_index = 0; logical_index < logical_count; ++logical_index) {
    size_t previous_index;
    CHECK(uni_sysinfo_cpu_snapshot_logical_processor(snapshot, logical_index, &processor) == UNI_SYSINFO_RESULT_OK);
    CHECK(processor.online);
    CHECK((processor.valid_fields & UNI_SYSINFO_CPU_VALID_ARCHITECTURE) != 0);

    for (previous_index = 0;
         (processor.valid_fields & UNI_SYSINFO_CPU_VALID_NATIVE_ID) != 0 && previous_index < logical_index;
         ++previous_index) {
      uni_sysinfo_cpu_logical_processor_t previous;
      CHECK(uni_sysinfo_cpu_snapshot_logical_processor(snapshot, previous_index, &previous) == UNI_SYSINFO_RESULT_OK);
      CHECK((previous.valid_fields & UNI_SYSINFO_CPU_VALID_NATIVE_ID) == 0 || previous.os_group != processor.os_group ||
            previous.os_cpu_id != processor.os_cpu_id);
    }

    if ((processor.valid_fields & UNI_SYSINFO_CPU_VALID_ALLOWED_SET) == 0 || processor.allowed) {
      uni_sysinfo_cpu_affinity_t *affinity = NULL;
      uni_sysinfo_result_e affinity_status;
      ++allowed_count;
      if (processor.architecture == UNI_SYSINFO_CPU_ARCHITECTURE_X86) {
        uni_sysinfo_cpu_x86_info_t x86_info;
        CHECK(uni_sysinfo_cpu_x86_info(snapshot, logical_index, &x86_info) == UNI_SYSINFO_RESULT_OK);
        if ((x86_info.valid_fields & UNI_SYSINFO_CPU_X86_VALID_FEATURES) != 0) {
          CHECK((x86_info.valid_fields & UNI_SYSINFO_CPU_X86_VALID_IDENTITY) != 0);
          x86_intersection &= x86_info.features;
          ++allowed_x86_count;
        }
      }
      affinity_status = uni_sysinfo_cpu_affinity_apply(snapshot, logical_index, &affinity);
      CHECK(affinity_status == UNI_SYSINFO_RESULT_OK || affinity_status == UNI_SYSINFO_RESULT_NOT_SUPPORTED);
      if (affinity_status == UNI_SYSINFO_RESULT_OK) {
        CHECK(affinity != NULL);
        CHECK(uni_sysinfo_cpu_affinity_restore(affinity) == UNI_SYSINFO_RESULT_OK);
        uni_sysinfo_cpu_affinity_destroy(affinity);
      }
    }
  }
  CHECK(allowed_count > 0);

  CHECK(uni_sysinfo_cpu_snapshot_logical_processor(snapshot, logical_count, &processor) ==
        UNI_SYSINFO_RESULT_OUT_OF_RANGE);

  CHECK(uni_sysinfo_cpu_snapshot_logical_processor(snapshot, 0, &processor) == UNI_SYSINFO_RESULT_OK);
  if (processor.architecture == UNI_SYSINFO_CPU_ARCHITECTURE_X86) {
    uni_sysinfo_cpu_aarch64_info_t aarch64_info;
    uni_sysinfo_cpu_x86_features_t features = 0;
    uni_sysinfo_result_e status             = uni_sysinfo_cpu_x86_features_common(snapshot, &features);
    if (allowed_x86_count > 0) {
      CHECK(status == UNI_SYSINFO_RESULT_OK);
      CHECK(features == x86_intersection);
      CHECK((features & UNI_SYSINFO_CPU_X86_FEATURE_SSE2) != 0);
    } else {
      CHECK(status == UNI_SYSINFO_RESULT_NOT_SUPPORTED);
    }
    CHECK(uni_sysinfo_cpu_aarch64_info(snapshot, 0, &aarch64_info) == UNI_SYSINFO_RESULT_NOT_SUPPORTED);
  } else if (processor.architecture == UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64) {
    uni_sysinfo_cpu_aarch64_info_t info;
    uni_sysinfo_cpu_x86_info_t x86_info;
    uni_sysinfo_cpu_aarch64_features_t features = 0;
    CHECK(uni_sysinfo_cpu_aarch64_info(snapshot, 0, &info) == UNI_SYSINFO_RESULT_OK);
    CHECK(uni_sysinfo_cpu_aarch64_features_common(snapshot, &features) == UNI_SYSINFO_RESULT_OK);
    CHECK(features == info.features);
    CHECK((features & UNI_SYSINFO_CPU_AARCH64_FEATURE_NEON) != 0);
    CHECK(uni_sysinfo_cpu_x86_info(snapshot, 0, &x86_info) == UNI_SYSINFO_RESULT_NOT_SUPPORTED);
  }

  uni_sysinfo_cpu_snapshot_destroy(snapshot);
  return 0;
}
