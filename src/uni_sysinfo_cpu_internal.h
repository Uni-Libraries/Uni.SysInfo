#ifndef UNI_SYSINFO_CPU_INTERNAL_H
#define UNI_SYSINFO_CPU_INTERNAL_H

#include <stddef.h>

#include "uni_sysinfo_cpu.h"
#include "uni_sysinfo_cpu_aarch64.h"
#include "uni_sysinfo_cpu_x86.h"

typedef struct {
  char name[UNI_SYSINFO_CPU_NAME_CAPACITY];
  uni_sysinfo_cpu_architecture_e architecture;
  uni_sysinfo_cpu_architecture_e host_architecture;
  uni_sysinfo_cpu_abi_e abi;
  uni_sysinfo_cpu_vendor_e vendor;
  uni_sysinfo_cpu_cache_t cache;
  uni_sysinfo_cpu_topology_t topology;
  size_t os_cpu_id;
  unsigned short os_group;
  int online;
  int allowed;
  size_t numa_node_idx;
  uint64_t max_frequency_hz;
  unsigned int performance_rank;
  unsigned int raw_efficiency_class;
  uni_sysinfo_cpu_valid_fields_t valid_fields;
} uni_sysinfo_cpu_core_info_t;

typedef struct {
  uni_sysinfo_cpu_core_info_t *cores;
  size_t core_count;
} uni_sysinfo_cpu_package_info_t;

struct uni_sysinfo_cpu_snapshot {
  uni_sysinfo_cpu_package_info_t *packages;
  size_t package_count;
  int heap_owned;
};

uni_sysinfo_result_e uni_sysinfo_cpu_platform_snapshot(uni_sysinfo_cpu_snapshot_t *snapshot);
void uni_sysinfo_cpu_snapshot_destroy(uni_sysinfo_cpu_snapshot_t *snapshot);

uni_sysinfo_result_e uni_sysinfo_cpu_snapshot_get_core(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t logical_index,
                                                       size_t *package_index_out, size_t *package_logical_index_out,
                                                       const uni_sysinfo_cpu_core_info_t **core_out);

void uni_sysinfo_cpu_x86_detect(char *name, size_t name_capacity, uni_sysinfo_cpu_vendor_e *vendor,
                                uni_sysinfo_cpu_x86_features_t *features);

int uni_sysinfo_cpu_x86_identity(unsigned int *family, unsigned int *model, unsigned int *stepping);

uni_sysinfo_cpu_core_type_e uni_sysinfo_cpu_x86_current_core_type(void);

#if defined(_WIN32)
uni_sysinfo_result_e uni_sysinfo_cpu_windows_query_x86(const uni_sysinfo_cpu_core_info_t *core,
                                                       uni_sysinfo_cpu_x86_info_t *info,
                                                       uni_sysinfo_cpu_core_type_e *core_type);
#endif

uni_sysinfo_result_e uni_sysinfo_cpu_platform_aarch64_features(uni_sysinfo_cpu_aarch64_features_t *result);

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_apply(const uni_sysinfo_cpu_core_info_t *core,
                                                             uni_sysinfo_cpu_affinity_t **previous);

uni_sysinfo_result_e uni_sysinfo_cpu_platform_affinity_restore(uni_sysinfo_cpu_affinity_t *previous);

void uni_sysinfo_cpu_platform_affinity_destroy(uni_sysinfo_cpu_affinity_t *affinity);

uni_sysinfo_cpu_architecture_e uni_sysinfo_cpu_process_architecture(void);
uni_sysinfo_cpu_abi_e uni_sysinfo_cpu_process_abi(void);

#endif
