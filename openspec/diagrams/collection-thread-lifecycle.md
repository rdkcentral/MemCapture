# Collection Thread Lifecycle per Metric

```mermaid
flowchart LR
    subgraph MainThread
      A[StartCollection called] --> B[metric spawns std::thread]
      C[StopCollection called] --> D[set mQuit=true + notify]
      D --> E[join thread]
      E --> F[SaveResults on main thread]
    end

    subgraph MetricThread
      B --> G[loop while !mQuit]
      G --> H[collect from proc/sysfs]
      H --> I[AddDataPoint on Measurement]
      I --> J[wait_for(frequency)]
      J --> G
      G -->|mQuit true| K[thread exits]
    end

    K --> E
```

## Metric-Specific Notes

- ProcessMetric: one thread, loops Procrank snapshots and updates process measurements.
- MemoryMetric: one thread, loops Linux/CMA/GPU/container/bandwidth/fragmentation collection.
- CpuIdleMetric: no periodic loop thread; start/reset and stop/read are implemented via `prctl` interface when enabled.

## Evidence

- IMetric contract methods: `IMetric.h:40`, `IMetric.h:45`, `IMetric.h:52`.
- ProcessMetric lifecycle: `ProcessMetric.cpp:39`, `ProcessMetric.cpp:45`, `ProcessMetric.cpp:134`.
- MemoryMetric lifecycle: `MemoryMetric.cpp:169`, `MemoryMetric.cpp:175`, `MemoryMetric.cpp:212`.
- CpuIdleMetric behavior: `CpuIdleMetric.cpp:36`, `CpuIdleMetric.cpp:64`.
