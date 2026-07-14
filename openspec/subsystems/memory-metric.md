# MemoryMetric Subsystem

## Responsibilities

### Facts - Responsibilities

- Collects Linux memory, swap details, CMA usage, GPU memory, cgroup container usage, memory bandwidth, fragmentation, and Broadcom BMEM (`MemoryMetric.cpp:194` through `MemoryMetric.cpp:204`).
- Emits datasets into report generator in `SaveResults` (`MemoryMetric.cpp:218` onward).

## Sampling and Accumulation Model

### Facts - Sampling

- Runs on dedicated collection thread with periodic `wait_for(frequency)` (`MemoryMetric.cpp:169`, `MemoryMetric.cpp:212`).
- Uses `Measurement::AddDataPoint` for min/max/average accumulation across the capture window.

### Inferences - Sampling

- Dataset rows represent aggregate capture-window statistics, not single snapshots.

## Platform-Specific Logic

### Facts - Platform Dispatch

- GPU collector dispatches by platform enum (`MemoryMetric.cpp:468`).
- Amlogic/950D4 bandwidth uses `/sys/class/aml_ddr/mode` and `/sys/class/aml_ddr/bandwidth` (`MemoryMetric.cpp:123`, `MemoryMetric.cpp:631`).
- Broadcom GPU uses `/sys/kernel/debug/dri/0/*/client` and TID->TGID conversion via `/proc/<tid>/status` (`MemoryMetric.cpp:825`, `MemoryMetric.cpp:1043`).
- Amlogic/Realtek read `/sys/kernel/debug/mali0/gpu_memory`; Amlogic/Realtek values are page-based, Mediatek parser treats values as KB (`MemoryMetric.cpp:909`, `MemoryMetric.cpp:1004`, `MemoryMetric.cpp:958`).

## Output Datasets Emitted

### Facts - Output Datasets

- `Linux Memory`
- `Swap Memory` (only if parser succeeded)
- `GPU Memory` (if supported)
- `CMA Regions`
- `CMA Summary`
- `Containers`
- `Memory Bandwidth` (if supported)
- `Memory Fragmentation - Zone <zone>`
- `BMEM` (Broadcom only)
(see `MemoryMetric.cpp:228` through `MemoryMetric.cpp:368`)

## Interactions

### Facts - Interactions

- Reads MemInfo parser for Linux memory and CMA free estimate (`MemoryMetric.cpp:381`, `MemoryMetric.cpp:453`).
- Contributes to global calculated usage via `addToAccumulatedMemoryUsage` for GPU/CMA/BMEM (`MemoryMetric.cpp:272`, `MemoryMetric.cpp:294`, `MemoryMetric.cpp:368`).

## Unknowns

- Runtime availability of debugfs/cgroup files differs by board image and kernel config.
