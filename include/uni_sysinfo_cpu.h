#pragma once

//
// Includes
//

// stdlib
#include <stddef.h>
#include <stdint.h>

// uni.sysinfo
#include "uni_sysinfo_export.h"
#include "uni_sysinfo_typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Enums
//

/** Processor instruction-set architecture family. */
typedef enum {
  /** Architecture could not be determined. */
  UNI_SYSINFO_CPU_ARCHITECTURE_UNKNOWN   = 0,
  /** 32-bit or 64-bit x86 instruction-set architecture. */
  UNI_SYSINFO_CPU_ARCHITECTURE_X86       = 1,
  /** 64-bit Arm instruction-set architecture. */
  UNI_SYSINFO_CPU_ARCHITECTURE_AARCH64   = 2,
  /** 32-bit Arm instruction-set architecture. */
  UNI_SYSINFO_CPU_ARCHITECTURE_ARM       = 3,
  /** 32-bit or 64-bit RISC-V instruction-set architecture. */
  UNI_SYSINFO_CPU_ARCHITECTURE_RISCV     = 4,
  /** 32-bit or 64-bit Power instruction-set architecture. */
  UNI_SYSINFO_CPU_ARCHITECTURE_POWER     = 5,
  /** 64-bit LoongArch instruction-set architecture. */
  UNI_SYSINFO_CPU_ARCHITECTURE_LOONGARCH = 6,
  /** 32-bit or 64-bit WebAssembly instruction-set architecture. */
  UNI_SYSINFO_CPU_ARCHITECTURE_WASM      = 7,
} uni_sysinfo_cpu_architecture_e;

/** Instruction mode used by the current process ABI. */
typedef enum {
  /** Process ABI could not be determined. */
  UNI_SYSINFO_CPU_ABI_UNKNOWN = 0,
  /** Process uses the 32-bit x86 instruction mode. */
  UNI_SYSINFO_CPU_ABI_X86_32,
  /** Process uses the x86-64 instruction mode. */
  UNI_SYSINFO_CPU_ABI_X86_64,
  /** Process uses the 32-bit Arm instruction mode. */
  UNI_SYSINFO_CPU_ABI_ARM_32,
  /** Process uses the AArch64 instruction mode. */
  UNI_SYSINFO_CPU_ABI_AARCH64,
  /** 32-bit RISC-V process ABI. */
  UNI_SYSINFO_CPU_ABI_RISCV_32,
  /** 64-bit RISC-V process ABI. */
  UNI_SYSINFO_CPU_ABI_RISCV_64,
  /** 32-bit Power process ABI. */
  UNI_SYSINFO_CPU_ABI_POWER_32,
  /** 64-bit Power process ABI. */
  UNI_SYSINFO_CPU_ABI_POWER_64,
  /** 64-bit LoongArch process ABI. */
  UNI_SYSINFO_CPU_ABI_LOONGARCH_64,
  /** 32-bit WebAssembly process ABI. */
  UNI_SYSINFO_CPU_ABI_WASM_32,
  /** 64-bit WebAssembly process ABI. */
  UNI_SYSINFO_CPU_ABI_WASM_64,
} uni_sysinfo_cpu_abi_e;

/** Normalized processor vendor. */
typedef enum {
  /** CPU vendor could not be determined or is not represented by this API. */
  UNI_SYSINFO_CPU_VENDOR_UNKNOWN = 0,
  /** Intel CPU vendor. */
  UNI_SYSINFO_CPU_VENDOR_INTEL   = 1,
  /** AMD CPU vendor. */
  UNI_SYSINFO_CPU_VENDOR_AMD     = 2,
  /** Apple CPU vendor. */
  UNI_SYSINFO_CPU_VENDOR_APPLE   = 3,
} uni_sysinfo_cpu_vendor_e;

// https://patchew.org/linux/178494358921.112641.8632342092707397307.tip-bot2@tip-bot2/
/** Normalized heterogeneous processor-core type. */
typedef enum {
  /** Core type could not be determined. */
  UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN     = 0,
  /** High-performance processor core. */
  UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE = 1,
  /** Energy-efficient processor core. */
  UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY  = 2,
  /** Lowest-power processor core class. */
  UNI_SYSINFO_CPU_CORE_TYPE_LOW_POWER   = 3,
} uni_sysinfo_cpu_core_type_e;

//
// Structs
//

/** Cache capacities associated with a logical processor. */
typedef struct {
  /** L1 data-cache capacity in bytes, or zero when unavailable. */
  size_t l1d_size;
  /** L1 instruction-cache capacity in bytes, or zero when unavailable. */
  size_t l1i_size;
  /** L2 cache capacity in bytes, or zero when unavailable. */
  size_t l2_size;
  /** L3 cache capacity in bytes, or zero when unavailable. */
  size_t l3_size;
} uni_sysinfo_cpu_cache_t;

/** Hardware-topology indices associated with a logical processor. */
typedef struct {
  /** Logical die index within the processor package. */
  size_t die_idx;
  /** Logical physical-core index within the processor package. */
  size_t hw_core_idx;
  /** Hardware-thread index within the core (SMT or CMT). */
  size_t smt_idx;
  /** Normalized heterogeneous-core type. */
  uni_sysinfo_cpu_core_type_e core_type;
} uni_sysinfo_cpu_topology_t;

/** Capacity of uni_sysinfo_cpu_logical_processor_t::name, including the terminator. */
#define UNI_SYSINFO_CPU_NAME_CAPACITY 128

/** Fixed-width validity mask for uni_sysinfo_cpu_logical_processor_t fields. */
typedef uint64_t uni_sysinfo_cpu_valid_fields_t;

/** The logical-processor display-name field contains a NUL-terminated string. */
#define UNI_SYSINFO_CPU_VALID_NAME              (UINT64_C(1) << 0)
/** The process architecture is known; host architecture contains the best available value. */
#define UNI_SYSINFO_CPU_VALID_ARCHITECTURE      (UINT64_C(1) << 1)
/** The CPU vendor field is valid. */
#define UNI_SYSINFO_CPU_VALID_VENDOR            (UINT64_C(1) << 2)
/** At least one cache-capacity field is available; zero marks unavailable levels. */
#define UNI_SYSINFO_CPU_VALID_CACHE             (UINT64_C(1) << 3)
/** The package, physical-core, and SMT topology fields are valid. */
#define UNI_SYSINFO_CPU_VALID_TOPOLOGY          (UINT64_C(1) << 4)
/** The native OS processor ID and processor group are valid. */
#define UNI_SYSINFO_CPU_VALID_NATIVE_ID         (UINT64_C(1) << 5)
/** The NUMA-node index is valid. */
#define UNI_SYSINFO_CPU_VALID_NUMA_NODE         (UINT64_C(1) << 6)
/** The maximum-frequency field is valid. */
#define UNI_SYSINFO_CPU_VALID_MAX_FREQUENCY     (UINT64_C(1) << 7)
/** At least one performance-class value is available; unavailable values remain zero. */
#define UNI_SYSINFO_CPU_VALID_PERFORMANCE_CLASS (UINT64_C(1) << 8)
/** The die topology field is valid. */
#define UNI_SYSINFO_CPU_VALID_DIE_TOPOLOGY      (UINT64_C(1) << 9)
/** The allowed field represents the calling thread's complete affinity set. */
#define UNI_SYSINFO_CPU_VALID_ALLOWED_SET       (UINT64_C(1) << 10)
/** The normalized core-type field is valid. */
#define UNI_SYSINFO_CPU_VALID_CORE_TYPE         (UINT64_C(1) << 11)

/** Generic information for one logical processor in a CPU snapshot. */
typedef struct {
  /** Logical processor-package index in the snapshot. */
  size_t package_idx;
  /** Logical-processor index within its package. */
  size_t package_logical_idx;
  /** Native OS processor number, valid with UNI_SYSINFO_CPU_VALID_NATIVE_ID. */
  size_t os_cpu_id;
  /** Native Windows processor group; zero on platforms without groups. */
  unsigned short os_group;
  /** Nonzero when the logical processor was online when the snapshot was created. */
  int online;
  /** Nonzero when allowed by the calling thread's captured affinity set. */
  int allowed;
  /** NUMA-node index, valid with UNI_SYSINFO_CPU_VALID_NUMA_NODE. */
  size_t numa_node_idx;
  /** Maximum reported processor frequency in hertz. */
  uint64_t max_frequency_hz;
  /** Comparable performance rank; larger values represent faster cores. */
  unsigned int performance_rank;
  /** Unmodified platform-specific efficiency or performance class. */
  unsigned int raw_efficiency_class;
  /** Instruction-set architecture used by the current process. */
  uni_sysinfo_cpu_architecture_e architecture;
  /** Best available host architecture; may fall back to the process architecture. */
  uni_sysinfo_cpu_architecture_e host_architecture;
  /** ABI used by the current process. */
  uni_sysinfo_cpu_abi_e abi;
  /** Processor vendor. */
  uni_sysinfo_cpu_vendor_e vendor;
  /** Cache capacities associated with this logical processor. */
  uni_sysinfo_cpu_cache_t cache;
  /** Die, core, SMT, and core-type topology. */
  uni_sysinfo_cpu_topology_t topology;
  /** Bit mask describing which optional fields in this record are valid. */
  uni_sysinfo_cpu_valid_fields_t valid_fields;
  /** NUL-terminated processor display name, which may be "Unknown CPU". */
  char name[UNI_SYSINFO_CPU_NAME_CAPACITY];
} uni_sysinfo_cpu_logical_processor_t;

/** Immutable CPU-topology snapshot handle. */
typedef struct uni_sysinfo_cpu_snapshot uni_sysinfo_cpu_snapshot_t;
/** Opaque saved thread-affinity state. */
typedef struct uni_sysinfo_cpu_affinity uni_sysinfo_cpu_affinity_t;

//
// Functions
//

/**
 * @brief Creates an immutable snapshot of the current CPU topology.
 *
 * Optional logical-processor fields are described by each record's
 * `valid_fields` mask. The caller owns the returned snapshot and must release
 * it with uni_sysinfo_cpu_snapshot_destroy().
 *
 * @param[out] result Receives the created snapshot on success.
 * @return UNI_SYSINFO_RESULT_OK on success, otherwise an error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_snapshot_create(uni_sysinfo_cpu_snapshot_t **result);

/**
 * @brief Destroys a CPU topology snapshot.
 *
 * @param[in] snapshot Snapshot to destroy; may be NULL.
 */
UNI_SYSINFO_EXPORT void uni_sysinfo_cpu_snapshot_destroy(uni_sysinfo_cpu_snapshot_t *snapshot);

/**
 * @brief Returns the number of processor-package groups in a snapshot.
 *
 * Backends may synthesize one package group when the physical package mapping
 * is unavailable.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[out] result Receives the package count.
 * @return UNI_SYSINFO_RESULT_OK on success, otherwise an error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e
uni_sysinfo_cpu_snapshot_package_count(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t *result);

/**
 * @brief Returns the number of logical processors in a snapshot.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[out] result Receives the logical-processor count.
 * @return UNI_SYSINFO_RESULT_OK on success, otherwise an error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e
uni_sysinfo_cpu_snapshot_logical_count(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t *result);

/**
 * @brief Returns generic information for one logical processor.
 *
 * The returned record remains valid independently of the snapshot. Optional
 * fields must be checked through `result->valid_fields` before use.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[in] logical_index Zero-based logical-processor index in the snapshot.
 * @param[out] result Receives the logical-processor information.
 * @return UNI_SYSINFO_RESULT_OK on success, OUT_OF_RANGE for an invalid index,
 *         or another error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_snapshot_logical_processor(
    const uni_sysinfo_cpu_snapshot_t *snapshot, size_t logical_index, uni_sysinfo_cpu_logical_processor_t *result);

/**
 * @brief Reads the current clock of one snapshot logical processor.
 *
 * Unlike the snapshot fields this is a live reading from the operating system's
 * frequency driver, so it can change between calls. Only Linux with cpufreq
 * reports it; a core that has no usable driver returns NOT_SUPPORTED.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[in] logical_index Zero-based logical-processor index in the snapshot.
 * @param[out] result_hz Receives the frequency in hertz on success, otherwise zero.
 * @return UNI_SYSINFO_RESULT_OK on success, OUT_OF_RANGE for an invalid index,
 *         NOT_SUPPORTED when no reading is available, or another error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_current_frequency(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                                          size_t logical_index, uint64_t *result_hz);

/**
 * @brief Pins the calling thread to one snapshot logical processor.
 *
 * The previous affinity state is captured for restoration. Restore it on the
 * same thread that called this function. The state does not reference the
 * snapshot and may be destroyed without restoring affinity. Platforms without
 * hard logical-CPU affinity return NOT_SUPPORTED.
 *
 * @param[in] snapshot CPU topology snapshot.
 * @param[in] logical_index Target logical-processor index.
 * @param[out] previous Receives the owned previous-affinity state on success.
 * @return UNI_SYSINFO_RESULT_OK on success, NOT_SUPPORTED when hard affinity
 *         is unavailable, or another error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_affinity_apply(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                                       size_t logical_index,
                                                                       uni_sysinfo_cpu_affinity_t **previous);

/**
 * @brief Restores a previously captured thread-affinity state.
 *
 * This function does not destroy the state. Call
 * uni_sysinfo_cpu_affinity_destroy() after restoration.
 *
 * @param[in] previous State returned by uni_sysinfo_cpu_affinity_apply().
 * @return UNI_SYSINFO_RESULT_OK on success, otherwise an error code.
 */
UNI_SYSINFO_EXPORT uni_sysinfo_result_e uni_sysinfo_cpu_affinity_restore(uni_sysinfo_cpu_affinity_t *previous);

/**
 * @brief Destroys a captured affinity state without changing thread affinity.
 *
 * @param[in] affinity Affinity state to destroy; may be NULL.
 */
UNI_SYSINFO_EXPORT void uni_sysinfo_cpu_affinity_destroy(uni_sysinfo_cpu_affinity_t *affinity);

#ifdef __cplusplus
}
#endif
