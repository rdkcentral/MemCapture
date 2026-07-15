# End-to-End Execution Flow

```mermaid
flowchart TD
    A[CLI invocation] --> B[parseArgs]
    B --> C[register SIGTERM/SIGINT handlers]
    C --> D[create output directory]
    D --> E{groups file enabled?}
    E -->|yes| F[parse groups JSON -> GroupManager]
    E -->|no| G[skip grouping]
    F --> H[construct Metadata + JsonReportGenerator]
    G --> H
    H --> I[construct ProcessMetric + MemoryMetric]
    I --> J[start ProcessMetric thread every 3s]
    I --> K[start MemoryMetric thread every 3s]
    J --> L{cpuidle requested and compiled?}
    K --> L
    L -->|yes| M[start CpuIdleMetric interval]
    L -->|no| N[skip CpuIdleMetric]
    M --> O[wait_for duration via ConditionVariable]
    N --> O
    O --> P{signal received early?}
    P -->|yes| Q[set early termination]
    P -->|no| R[normal timeout]
    Q --> S[StopCollection and join all active metrics]
    R --> S
    S --> T[SaveResults for each metric]
    T --> U{json flag?}
    U -->|yes| V[write report.json]
    U -->|no| W[skip json file]
    V --> X[render html template via inja]
    W --> X
    X --> Y[write report.html]
    Y --> Z[exit success]
```

## Evidence

- `main` orchestration and lifecycle: `main.cpp:179`.
- Metrics started with 3-second frequency: `main.cpp:243`, `main.cpp:244`.
- Wait window uses custom condition variable: `main.cpp:257`, `ConditionVariable.h:135`.
- Stop and save order: `main.cpp:268` through `main.cpp:281`.
- JSON then HTML write sequencing: `main.cpp:327` through `main.cpp:344`.
