# Data Flow: proc/sysfs to Report Schema

```mermaid
flowchart LR
    subgraph Sources
      A[/proc/meminfo/]
      B[/proc/<pid>/smaps or smaps_rollup/]
      C[/sys/kernel/debug/cma/*/]
      D[/sys/kernel/debug/mali0/gpu_memory/]
      E[/sys/kernel/debug/dri/0/*/client/]
      F[/proc/buddyinfo/]
      G[/sys/fs/cgroup/*/]
      H[/sys/class/aml_ddr/*/]
      I[/proc/brcm/core/]
    end

    subgraph ParsersAndCollectors
      J[MemInfo parser]
      K[Smaps parser]
      L[Procrank]
      M[MemoryMetric collectors]
      N[ProcessMetric collector]
    end

    subgraph Aggregation
      O[Measurement min/max/average]
      P[processMeasurement vector]
    end

    subgraph Report
      Q[JsonReportGenerator mJson]
      R[report.json optional]
      S[inja template render]
      T[report.html]
    end

    A --> J --> M --> O --> Q
    B --> K --> L --> N --> P --> Q
    C --> M
    D --> M
    E --> M
    F --> M
    G --> M
    H --> M
    I --> M
    Q --> R
    Q --> S --> T
```

## Evidence

- MemInfo parser entry: `FileParsers/MemInfo.cpp:31`.
- Smaps parser entries: `FileParsers/Smaps.cpp:40`, `FileParsers/Smaps.cpp:87`.
- Procrank uses Smaps for process memory: `Procrank.cpp:165`.
- Measurement accumulation API: `Measurement.cpp:40`.
- Json root object build: `JsonReportGenerator.cpp:92`.
