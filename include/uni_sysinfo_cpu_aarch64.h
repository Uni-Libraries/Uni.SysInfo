#pragma once

#include <stdint.h>

#include "uni_sysinfo_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fixed-width bit mask of OS-usable AArch64 instruction-set features. */
typedef uint64_t uni_sysinfo_cpu_aarch64_features_t;

/** Advanced SIMD (NEON) instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_NEON        (UINT64_C(1) << 0)
/** Arm AES instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_AES         (UINT64_C(1) << 1)
/** Arm polynomial multiply instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_PMULL       (UINT64_C(1) << 2)
/** Arm SHA-1 instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_SHA1        (UINT64_C(1) << 3)
/** Arm SHA-2 instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_SHA2        (UINT64_C(1) << 4)
/** Arm CRC32 instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_CRC32       (UINT64_C(1) << 5)
/** Arm large-system-extension atomic instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_ATOMICS     (UINT64_C(1) << 6)
/** Scalar half-precision floating-point instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_FP16_SCALAR (UINT64_C(1) << 7)
/** Advanced SIMD half-precision floating-point instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_FP16_SIMD   (UINT64_C(1) << 8)
/** Advanced SIMD dot-product instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_DOTPROD     (UINT64_C(1) << 9)
/** Scalable Vector Extension instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_SVE         (UINT64_C(1) << 10)
/** Scalable Vector Extension 2 instructions are OS-usable. */
#define UNI_SYSINFO_CPU_AARCH64_FEATURE_SVE2        (UINT64_C(1) << 11)

/** AArch64-specific information available to the current process. */
typedef struct {
  /** OS-usable AArch64 instruction-set feature mask. */
  uni_sysinfo_cpu_aarch64_features_t features;
} uni_sysinfo_cpu_aarch64_info_t;

/**
 * @brief Returns process-wide AArch64 information after validating a snapshot entry.
 *
 * Call this function only after the generic processor record reports
 * UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64. The feature mask is process-wide rather
 * than processor-local and is safe to execute according to the operating
 * system.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[in] logical_index Zero-based logical-processor index in the snapshot.
 * @param[out] result Receives AArch64 information.
 * @return UNI_SYSINFO_RESULT_OK on success, NOT_SUPPORTED for another
 *         architecture, or another error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_aarch64_info(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                                     size_t logical_index,
                                                                     uni_sysinfo_cpu_aarch64_info_t *result);

/**
 * @brief Returns AArch64 features usable on every allowed logical processor.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[out] result Receives the common AArch64 feature mask.
 * @return UNI_SYSINFO_RESULT_OK on success, NOT_SUPPORTED when a safe common
 *         mask cannot be determined, or another error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_aarch64_features_common(
    const uni_sysinfo_cpu_snapshot_t *snapshot, uni_sysinfo_cpu_aarch64_features_t *result);

#ifdef __cplusplus
}
#endif
