# Report Generation Pipeline Sequence

```mermaid
sequenceDiagram
    participant main as main
    participant PM as ProcessMetric
    participant MM as MemoryMetric
    participant CM as CpuIdleMetric(optional)
    participant JRG as JsonReportGenerator
    participant FS as FileSystem

    main->>PM: SaveResults()
    PM->>JRG: addProcesses(mMeasurements)
    PM->>JRG: addToAccumulatedMemoryUsage(pssSum)

    main->>MM: SaveResults()
    MM->>JRG: addDataset(...)
    MM->>JRG: setAverageLinuxMemoryUsage(...)
    MM->>JRG: addToAccumulatedMemoryUsage(...)

    opt CPU idle enabled
      main->>CM: SaveResults()
      CM->>JRG: addCpuIdleMetrics(...)
    end

    opt -j flag
      main->>JRG: getJson()
      main->>FS: write report.json
    end

    main->>JRG: getJson()
    main->>main: inja render(template, json)
    main->>FS: write report.html
```

## Evidence

- SaveResults call sequence in `main`: `main.cpp:277` through `main.cpp:281`.
- JSON write then HTML render/write: `main.cpp:327` through `main.cpp:344`.
- Report generator APIs: `JsonReportGenerator.cpp:38`, `JsonReportGenerator.cpp:92`, `JsonReportGenerator.cpp:106`.
