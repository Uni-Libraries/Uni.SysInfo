# Uni.SysInfo

Cross-platform C11 library for querying system information. The API provides
immutable CPU topology snapshots, native logical-processor identities, current
affinity membership, NUMA and performance data, cache sizes, and OS-usable x86
and AArch64 instruction-set features.

The snapshot API is intended for schedulers and dispatch libraries. It keeps
topology indices coherent while the snapshot is alive and reports optional
fields through `valid_fields`. Generic topology is declared by
`uni_sysinfo_cpu.h`; consumers include `uni_sysinfo_cpu_x86.h` or
`uni_sysinfo_cpu_aarch64.h` explicitly and query architecture-specific data
from the same snapshot only after checking a processor's architecture.

The `uni-sysinfo` CLI prints every available CPU field as a
`package -> die -> core -> thread` tree.

Supported CPU backends: Linux, Windows, and Darwin. Other systems build with a
stub backend that reports `UNI_SYSINFO_RESULT_NOT_SUPPORTED`.

```sh
cmake -S . -B build -DUNI_SYSINFO_BUILD_CLI=ON
cmake --build build
./build/uni-sysinfo
```

Use `-DUNI_SYSINFO_BUILD_CLI=OFF` when only the library is required.
