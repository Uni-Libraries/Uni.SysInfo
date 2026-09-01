#pragma once

//
// Includes
//

#include <stdint.h>

// uni.sysinfo
#include "uni_sysinfo_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Enums
//

/** Fixed-width bit mask of x86 instruction-set features. AVX bits include OS-state checks. */
typedef uint64_t uni_sysinfo_cpu_x86_features_t;
/** Fixed-width bit mask describing valid fields in uni_sysinfo_cpu_x86_info_t. */
typedef uint64_t uni_sysinfo_cpu_x86_valid_fields_t;

/** The x86 feature mask is valid. */
#define UNI_SYSINFO_CPU_X86_VALID_FEATURES (UINT64_C(1) << 0)
/** The x86 family, model, and stepping fields are valid. */
#define UNI_SYSINFO_CPU_X86_VALID_IDENTITY (UINT64_C(1) << 1)

/** Streaming SIMD Extensions are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_SSE         (UINT64_C(1) << 0)
/** Streaming SIMD Extensions 2 are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_SSE2        (UINT64_C(1) << 1)
/** Supplemental Streaming SIMD Extensions 3 are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_SSSE3       (UINT64_C(1) << 2)
/** Fused multiply-add instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_FMA         (UINT64_C(1) << 3)
/** Streaming SIMD Extensions 4.1 are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_SSE41       (UINT64_C(1) << 4)
/** Streaming SIMD Extensions 4.2 are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_SSE42       (UINT64_C(1) << 5)
/** AMD Streaming SIMD Extensions 4a are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_SSE4A       (UINT64_C(1) << 6)
/** Advanced Vector Extensions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX         (UINT64_C(1) << 7)
/** Advanced Vector Extensions 2 are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX2        (UINT64_C(1) << 8)
/** AVX-512 Foundation instructions and register state are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512F     (UINT64_C(1) << 9)
/** AVX-512 doubleword and quadword instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512DQ    (UINT64_C(1) << 10)
/** AVX-512 integer fused multiply-add instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512IFMA  (UINT64_C(1) << 11)
/** AVX-512 conflict-detection instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512CD    (UINT64_C(1) << 12)
/** AVX-512 byte and word instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512BW    (UINT64_C(1) << 13)
/** AVX-512 vector-length extensions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512VL    (UINT64_C(1) << 14)
/** AVX-512 bfloat16 instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512BF16  (UINT64_C(1) << 15)
/** AVX-512 vector byte-manipulation instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512VBMI  (UINT64_C(1) << 16)
/** AES-NI instructions are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AES         (UINT64_C(1) << 17)
/** Carry-less multiplication instructions are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_PCLMULQDQ   (UINT64_C(1) << 18)
/** Population-count instructions are supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_POPCNT      (UINT64_C(1) << 19)
/** Half-precision conversion instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_F16C        (UINT64_C(1) << 20)
/** Bit Manipulation Instruction Set 1 is supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_BMI1        (UINT64_C(1) << 21)
/** Bit Manipulation Instruction Set 2 is supported by the processor. */
#define UNI_SYSINFO_CPU_X86_FEATURE_BMI2        (UINT64_C(1) << 22)
/** AVX-512 vector byte-manipulation instructions 2 are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512VBMI2 (UINT64_C(1) << 23)
/** AVX-512 vector neural-network instructions are OS-usable. */
#define UNI_SYSINFO_CPU_X86_FEATURE_AVX512VNNI  (UINT64_C(1) << 24)

/** x86-specific information for a snapshot logical processor. */
typedef struct {
  /** x86 feature mask; AVX-family bits include operating-system state checks. */
  uni_sysinfo_cpu_x86_features_t features;
  /** Display family after applying CPUID extended-family rules. */
  unsigned int family;
  /** Display model after applying CPUID extended-model rules. */
  unsigned int model;
  /** CPUID processor stepping. */
  unsigned int stepping;
  /** Bit mask describing which fields in this record are valid. */
  uni_sysinfo_cpu_x86_valid_fields_t valid_fields;
} uni_sysinfo_cpu_x86_info_t;

//
// Functions
//

/**
 * @brief Returns x86-specific information for one snapshot entry.
 *
 * Call this function only after the generic processor record reports
 * UNI_SYSINFO_CPU_ARCHITECTURE_X86. When a native processor ID is available,
 * the query is executed on that processor. Otherwise it describes the
 * process-wide x86 view. AVX-family bits include OSXSAVE and XCR0 checks.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[in] logical_index Zero-based logical-processor index in the snapshot.
 * @param[out] result Receives x86 information and its validity mask.
 * @return UNI_SYSINFO_RESULT_OK on success, NOT_SUPPORTED for another
 *         architecture, or another error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_x86_info(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                                 size_t logical_index,
                                                                 uni_sysinfo_cpu_x86_info_t *result);

/**
 * @brief Returns x86 features reported for every allowed logical processor.
 *
 * The result is the intersection of per-processor feature masks across the
 * calling thread's affinity set represented by the snapshot. AVX-family bits
 * include the required operating-system state checks.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[out] result Receives the common x86 feature mask.
 * @return UNI_SYSINFO_RESULT_OK on success, NOT_SUPPORTED when a safe common
 *         mask cannot be determined, or another error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_x86_features_common(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                                            uni_sysinfo_cpu_x86_features_t *result);

#ifdef __cplusplus
}
#endif
