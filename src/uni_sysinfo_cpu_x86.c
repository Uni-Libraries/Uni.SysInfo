#include "uni_sysinfo_cpu_internal.h"

#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#define UNI_SYSINFO_X86 1
#else
#define UNI_SYSINFO_X86 0
#endif

#if UNI_SYSINFO_X86
static void uni_sysinfo_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t registers[4]) {
#if defined(_MSC_VER)
  int values[4];
  __cpuidex(values, (int)leaf, (int)subleaf);
  registers[0] = (uint32_t)values[0];
  registers[1] = (uint32_t)values[1];
  registers[2] = (uint32_t)values[2];
  registers[3] = (uint32_t)values[3];
#else
  __cpuid_count(leaf, subleaf, registers[0], registers[1], registers[2], registers[3]);
#endif
}

static uint64_t uni_sysinfo_xgetbv(void) {
#if defined(_MSC_VER)
  return _xgetbv(0);
#else
  uint32_t low;
  uint32_t high;
  __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
  return ((uint64_t)high << 32) | low;
#endif
}
#endif

int uni_sysinfo_cpu_x86_identity(unsigned int *family, unsigned int *model, unsigned int *stepping) {
#if UNI_SYSINFO_X86
  uint32_t registers[4];
  unsigned int base_family;
  unsigned int base_model;
  unsigned int extended_family;
  unsigned int extended_model;

  if (family == NULL || model == NULL || stepping == NULL)
    return 0;
  uni_sysinfo_cpuid(0, 0, registers);
  if (registers[0] < 1)
    return 0;
  uni_sysinfo_cpuid(1, 0, registers);
  *stepping       = registers[0] & 0x0fu;
  base_model      = (registers[0] >> 4) & 0x0fu;
  base_family     = (registers[0] >> 8) & 0x0fu;
  extended_model  = (registers[0] >> 16) & 0x0fu;
  extended_family = (registers[0] >> 20) & 0xffu;
  *family         = base_family == 0x0fu ? base_family + extended_family : base_family;
  *model          = (base_family == 0x06u || base_family == 0x0fu) ? base_model | (extended_model << 4) : base_model;
  return 1;
#else
  (void)family;
  (void)model;
  (void)stepping;
  return 0;
#endif
}

void uni_sysinfo_cpu_x86_detect(char *name, size_t name_capacity, uni_sysinfo_cpu_vendor_e *vendor,
                                uni_sysinfo_cpu_x86_features_t *features) {
  if (name != NULL && name_capacity > 0) {
    name[0] = '\0';
  }
  if (vendor != NULL) {
    *vendor = UNI_SYSINFO_CPU_VENDOR_UNKNOWN;
  }
  if (features != NULL) {
    *features = 0;
  }

#if UNI_SYSINFO_X86
  {
    uint32_t registers[4];
    uint32_t maximum_leaf;
    uint32_t maximum_extended_leaf;
    uint64_t xcr0 = 0;
    int avx_usable;
    int avx512_usable;
    char vendor_name[13];

    uni_sysinfo_cpuid(0, 0, registers);
    maximum_leaf = registers[0];
    memcpy(vendor_name, &registers[1], 4);
    memcpy(vendor_name + 4, &registers[3], 4);
    memcpy(vendor_name + 8, &registers[2], 4);
    vendor_name[12] = '\0';

    if (vendor != NULL) {
      if (strcmp(vendor_name, "GenuineIntel") == 0) {
        *vendor = UNI_SYSINFO_CPU_VENDOR_INTEL;
      } else if (strcmp(vendor_name, "AuthenticAMD") == 0) {
        *vendor = UNI_SYSINFO_CPU_VENDOR_AMD;
      }
    }

    uni_sysinfo_cpuid(0x80000000u, 0, registers);
    maximum_extended_leaf = registers[0];
    if (name != NULL && name_capacity > 0 && maximum_extended_leaf >= 0x80000004u) {
      char brand[49];
      char *begin;
      char *end;
      size_t length;
      uint32_t leaf;

      for (leaf = 0; leaf < 3; ++leaf) {
        uni_sysinfo_cpuid(0x80000002u + leaf, 0, registers);
        memcpy(brand + leaf * 16, registers, 16);
      }
      brand[48] = '\0';
      begin     = brand;
      while (*begin == ' ') {
        ++begin;
      }
      end = begin + strlen(begin);
      while (end > begin && end[-1] == ' ') {
        --end;
      }
      length = (size_t)(end - begin);
      if (length >= name_capacity) {
        length = name_capacity - 1;
      }
      memcpy(name, begin, length);
      name[length] = '\0';
    }

    if (features == NULL || maximum_leaf < 1) {
      return;
    }

    uni_sysinfo_cpuid(1, 0, registers);
    if ((registers[3] & (1u << 25)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_SSE;
    if ((registers[3] & (1u << 26)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_SSE2;
    if ((registers[2] & (1u << 1)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_PCLMULQDQ;
    if ((registers[2] & (1u << 9)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_SSSE3;
    if ((registers[2] & (1u << 19)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_SSE41;
    if ((registers[2] & (1u << 20)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_SSE42;
    if ((registers[2] & (1u << 23)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_POPCNT;
    if ((registers[2] & (1u << 25)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_AES;

    if ((registers[2] & (1u << 27)) != 0 && (registers[2] & (1u << 26)) != 0) {
      xcr0 = uni_sysinfo_xgetbv();
    }
    avx_usable    = (registers[2] & (1u << 28)) != 0 && (xcr0 & 0x6u) == 0x6u;
    avx512_usable = avx_usable && (xcr0 & 0xe0u) == 0xe0u;
    if (avx_usable)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX;
    if (avx_usable && (registers[2] & (1u << 12)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_FMA;
    if (avx_usable && (registers[2] & (1u << 29)) != 0)
      *features |= UNI_SYSINFO_CPU_X86_FEATURE_F16C;

    if (maximum_extended_leaf >= 0x80000001u) {
      uni_sysinfo_cpuid(0x80000001u, 0, registers);
      if ((registers[2] & (1u << 6)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_SSE4A;
    }

    if (maximum_leaf >= 7) {
      uint32_t maximum_subleaf;
      uni_sysinfo_cpuid(7, 0, registers);
      maximum_subleaf = registers[0];
      if ((registers[1] & (1u << 3)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_BMI1;
      if ((registers[1] & (1u << 8)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_BMI2;
      if (avx_usable && (registers[1] & (1u << 5)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX2;
      if (avx512_usable && (registers[1] & (1u << 16)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512F;
      if (avx512_usable && (registers[1] & (1u << 17)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512DQ;
      if (avx512_usable && (registers[1] & (1u << 21)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512IFMA;
      if (avx512_usable && (registers[1] & (1u << 28)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512CD;
      if (avx512_usable && (registers[1] & (1u << 30)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512BW;
      if (avx512_usable && (registers[1] & (1u << 31)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512VL;
      if (avx512_usable && (registers[2] & (1u << 1)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512VBMI;
      if (avx512_usable && (registers[2] & (1u << 6)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512VBMI2;
      if (avx512_usable && (registers[2] & (1u << 11)) != 0)
        *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512VNNI;
      if (avx512_usable && maximum_subleaf >= 1) {
        uni_sysinfo_cpuid(7, 1, registers);
        if ((registers[0] & (1u << 5)) != 0)
          *features |= UNI_SYSINFO_CPU_X86_FEATURE_AVX512BF16;
      }
    }
  }
#endif
}

uni_sysinfo_cpu_core_type_e uni_sysinfo_cpu_x86_current_core_type(void) {
#if UNI_SYSINFO_X86
  uint32_t registers[4];
  char vendor_name[13];
  uint32_t subleaf;

  uni_sysinfo_cpuid(0, 0, registers);
  memcpy(vendor_name, &registers[1], 4);
  memcpy(vendor_name + 4, &registers[3], 4);
  memcpy(vendor_name + 8, &registers[2], 4);
  vendor_name[12] = '\0';

  if (strcmp(vendor_name, "GenuineIntel") == 0) {
    uni_sysinfo_cpuid(0, 0, registers);
    if (registers[0] < 0x1au)
      return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
    uni_sysinfo_cpuid(0x1au, 0, registers);
    if (((registers[0] >> 24) & 0xffu) == 0x40u)
      return UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE;
    if (((registers[0] >> 24) & 0xffu) == 0x20u)
      return UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY;
    return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
  }
  if (strcmp(vendor_name, "AuthenticAMD") != 0)
    return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;

  uni_sysinfo_cpuid(0x80000000u, 0, registers);
  if (registers[0] < 0x80000026u) {
    return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
  }

  for (subleaf = 0; subleaf < 256; ++subleaf) {
    uint32_t level_type;
    uint32_t core_type;

    uni_sysinfo_cpuid(0x80000026u, subleaf, registers);
    level_type = (registers[2] >> 8) & 0xffu;
    if (level_type == 0) {
      break;
    }
    if (level_type != 1) {
      continue;
    }

    core_type = (registers[1] >> 28) & 0x0fu;
    if (core_type == 0) {
      return UNI_SYSINFO_CPU_CORE_TYPE_PERFORMANCE;
    }
    if (core_type == 1) {
      return UNI_SYSINFO_CPU_CORE_TYPE_EFFICIENCY;
    }
    if (core_type == 2) {
      return UNI_SYSINFO_CPU_CORE_TYPE_LOW_POWER;
    }
    break;
  }
#endif
  return UNI_SYSINFO_CPU_CORE_TYPE_UNKNOWN;
}

uni_sysinfo_result_e uni_sysinfo_cpu_x86_info(const uni_sysinfo_cpu_snapshot_t *snapshot, size_t logical_index,
                                              uni_sysinfo_cpu_x86_info_t *result) {
  const uni_sysinfo_cpu_core_info_t *core;
  uni_sysinfo_cpu_affinity_t *affinity = NULL;
  uni_sysinfo_result_e status;

  if (result == NULL)
    return UNI_SYSINFO_RESULT_INVALID_ARGUMENT;
  memset(result, 0, sizeof(*result));
  status = uni_sysinfo_cpu_snapshot_get_core(snapshot, logical_index, NULL, NULL, &core);
  if (status != UNI_SYSINFO_RESULT_OK)
    return status;
  if (core->architecture != UNI_SYSINFO_CPU_ARCHITECTURE_X86)
    return UNI_SYSINFO_RESULT_NOT_SUPPORTED;
  if ((core->valid_fields & UNI_SYSINFO_CPU_VALID_ALLOWED_SET) != 0 && !core->allowed)
    return UNI_SYSINFO_RESULT_OK;

  if ((core->valid_fields & UNI_SYSINFO_CPU_VALID_NATIVE_ID) != 0) {
    status = uni_sysinfo_cpu_platform_affinity_apply(core, &affinity);
    if (status == UNI_SYSINFO_RESULT_NOT_SUPPORTED)
      return UNI_SYSINFO_RESULT_OK;
    if (status != UNI_SYSINFO_RESULT_OK)
      return status;
  }

  uni_sysinfo_cpu_x86_detect(NULL, 0, NULL, &result->features);
  result->valid_fields |= UNI_SYSINFO_CPU_X86_VALID_FEATURES;
  if (uni_sysinfo_cpu_x86_identity(&result->family, &result->model, &result->stepping)) {
    result->valid_fields |= UNI_SYSINFO_CPU_X86_VALID_IDENTITY;
  }
  if (affinity != NULL) {
    status = uni_sysinfo_cpu_platform_affinity_restore(affinity);
    uni_sysinfo_cpu_platform_affinity_destroy(affinity);
    if (status != UNI_SYSINFO_RESULT_OK) {
      memset(result, 0, sizeof(*result));
      return status;
    }
  }
  return UNI_SYSINFO_RESULT_OK;
}

uni_sysinfo_result_e uni_sysinfo_cpu_x86_features_common(const uni_sysinfo_cpu_snapshot_t *snapshot,
                                                         uni_sysinfo_cpu_x86_features_t *result) {
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
    uni_sysinfo_cpu_x86_info_t info;
    status = uni_sysinfo_cpu_snapshot_get_core(snapshot, logical_index, NULL, NULL, &core);
    if (status != UNI_SYSINFO_RESULT_OK)
      break;
    if (core->architecture != UNI_SYSINFO_CPU_ARCHITECTURE_X86)
      continue;
    if ((core->valid_fields & UNI_SYSINFO_CPU_VALID_ALLOWED_SET) != 0 && !core->allowed)
      continue;
    status = uni_sysinfo_cpu_x86_info(snapshot, logical_index, &info);
    if (status != UNI_SYSINFO_RESULT_OK)
      return status;
    if ((info.valid_fields & UNI_SYSINFO_CPU_X86_VALID_FEATURES) == 0) {
      status = UNI_SYSINFO_RESULT_NOT_SUPPORTED;
      break;
    }
    if (!found) {
      *result = info.features;
      found   = 1;
    } else {
      *result &= info.features;
    }
  }
  if (status != UNI_SYSINFO_RESULT_OK)
    return status;
  return found ? UNI_SYSINFO_RESULT_OK : UNI_SYSINFO_RESULT_NOT_SUPPORTED;
}
