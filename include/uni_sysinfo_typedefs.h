#pragma once

/** Result codes returned by Uni.SysInfo operations. */
typedef enum {
  /** Unspecified result value; public functions do not return it intentionally. */
  UNI_SYSINFO_RESULT_UNKNOWN          = 0,
  /** Operation completed successfully. */
  UNI_SYSINFO_RESULT_OK               = 1,
  /** A required pointer or argument value was invalid. */
  UNI_SYSINFO_RESULT_INVALID_ARGUMENT = 2,
  /** An index was outside the corresponding snapshot range. */
  UNI_SYSINFO_RESULT_OUT_OF_RANGE     = 3,
  /** A caller-provided buffer could not hold the complete result. */
  UNI_SYSINFO_RESULT_BUFFER_TOO_SMALL = 4,
  /** The requested operation or a safe complete result is unavailable. */
  UNI_SYSINFO_RESULT_NOT_SUPPORTED    = 5,
  /** The operating system or runtime environment failed the operation. */
  UNI_SYSINFO_RESULT_SYSTEM_ERROR     = 6,
} uni_sysinfo_result_e;
