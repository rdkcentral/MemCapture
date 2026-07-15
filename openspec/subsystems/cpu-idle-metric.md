# CpuIdleMetric Subsystem

## Facts

- Compiled only when `ENABLE_CPU_IDLE_METRICS` is enabled (`CMakeLists.txt:83`, `CpuIdleMetric.cpp:20`).
- Start path warns if `collectd` appears to be running (using Procrank process list), then resets/reads kernel idle metrics via `prctl(PR_GET_IDLE_METRICS, ...)` (`CpuIdleMetric.cpp:36` through `CpuIdleMetric.cpp:58`).
- Stop path reads final idle metrics via `prctl` (`CpuIdleMetric.cpp:64`).
- Save path forwards captured structure to `JsonReportGenerator::addCpuIdleMetrics` (`CpuIdleMetric.cpp:75`).

## Inferences

- This metric is interval-based (capture at start and stop) rather than periodic sampling thread model.

## Assumptions

- Kernel headers and ABI for idle metrics are compatible with target image when feature is enabled.

## Unknowns

- Cross-platform support matrix for the required kernel patch is not encoded in source.
