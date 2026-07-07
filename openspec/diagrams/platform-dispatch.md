# Platform Dispatch Decision Tree (GPU Memory)

```mermaid
flowchart TD
    A[MemoryMetric::GetGpuMemoryUsage] --> B{mGPUMemorySupported?}
    B -->|no| C[return]
    B -->|yes| D{mPlatform}

    D -->|AMLOGIC or AMLOGIC_950D4| E[GetGpuMemoryUsageAmlogic]
    D -->|REALTEK or REALTEK64| F[GetGpuMemoryUsageRealtek]
    D -->|BROADCOM| G[GetGpuMemoryUsageBroadcom]
    D -->|MEDIATEK| H[GetGpuMemoryUsageMediatek]

    E --> E1[/sys/kernel/debug/mali0/gpu_memory pages -> KB/]
    F --> F1[/sys/kernel/debug/mali0/gpu_memory pages -> KB/]
    G --> G1[/sys/kernel/debug/dri/0/*/client TID -> TGID/]
    H --> H1[/sys/kernel/debug/mali0/gpu_memory currentKB/]
```

## Evidence

- Dispatch switch: `MemoryMetric.cpp:468` through `MemoryMetric.cpp:493`.
- Amlogic collector: `MemoryMetric.cpp:909`.
- Realtek collector: `MemoryMetric.cpp:1004`.
- Broadcom collector: `MemoryMetric.cpp:825`.
- Mediatek collector: `MemoryMetric.cpp:958`.
- Platform enum values: `Platform.h:22`.
