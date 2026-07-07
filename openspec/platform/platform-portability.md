# Platform Portability Model

## Platform Enum and CLI Mapping

### Facts - Enum and CLI Mapping

- Enum values: `AMLOGIC`, `AMLOGIC_950D4`, `REALTEK`, `REALTEK64`, `BROADCOM`, `MEDIATEK` (`Platform.h:22`).
- CLI parser maps exact strings to those enum values and exits on unknown string (`main.cpp:116` through `main.cpp:131`).

## Universal vs Platform-Specific Metrics

### Universal (code-path universal)

- Process snapshots from `/proc` via Procrank/Smaps (`ProcessMetric.cpp`, `Procrank.cpp`).
- Linux memory from `/proc/meminfo` (`MemoryMetric.cpp:381`).
- Metadata collection (with Unknown fallback) (`Metadata.cpp`).

### Platform-Specific or environment-specific

- GPU memory format/path dispatch (`MemoryMetric.cpp:468`).
- DDR bandwidth collector for Amlogic variants (`MemoryMetric.cpp:625`).
- Broadcom BMEM collector (`MemoryMetric.cpp:657`).
- Fragmentation parser expected column count differs by platform (`MemoryMetric.cpp:722` onward).

## Per-Platform GPU Sources

### Facts - GPU Sources

- AMLOGIC, AMLOGIC_950D4: `/sys/kernel/debug/mali0/gpu_memory` page-based parse (`MemoryMetric.cpp:909`).
- REALTEK, REALTEK64: `/sys/kernel/debug/mali0/gpu_memory` Realtek format parse (`MemoryMetric.cpp:1004`).
- BROADCOM: `/sys/kernel/debug/dri/0/*/client` plus `/proc/<tid>/status` TGID mapping (`MemoryMetric.cpp:825`, `MemoryMetric.cpp:1043`).
- MEDIATEK: `/sys/kernel/debug/mali0/gpu_memory` parser expecting KB values (`MemoryMetric.cpp:958`).

## Build Portability

### Facts - Build Portability

- C++17 target with POSIX/Linux file APIs.
- Desktop dependency model uses CMake + vcpkg (`README.md:23`, `vcpkg.json`).
- Device build notes expect Yocto dependency wiring for json/inja (`README.md:29`).
- Optional Breakpad integration and optional CPU idle support are compile-time toggles (`CMakeLists.txt:30`, `CMakeLists.txt:83`).

## Inferences

- Portability work for new SoCs should focus first on `MemoryMetric` and filesystem path detection.
