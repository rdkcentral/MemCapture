## HLD Generation Guidelines for MemCapture

- **Project Goal:** Add new metrics, new platform support, or new report output formats to MemCapture.
- **Target Platforms:** Amlogic, Amlogic 950D4, Realtek, Realtek64, Broadcom, Mediatek — resource-constrained Linux devices.
- **Constraints:** New code must be efficient, lightweight, and platform-neutral; must not measurably impact the system being measured.

## Design Strategy

1. **Requirements Gathering**
    - For each new metric or platform, create a Markdown `.md` document describing:
        - What data is collected and from which `/proc` or sysfs source
        - Which platforms support this metric (and which do not)
        - Inputs to the metric (frequency, platform, optional flags)
        - Output: which JSON keys are added to `results.json`
        - Constraints: file access latency, device memory budget
        - Edge cases: file absent, permission denied, unexpected format

2. **High Level Design (HLD)**
    - For each new component, create a separate HLD `.md` file including:
        - Where it fits in the `IMetric` architecture
        - Platform dispatch strategy (new switch case vs. separate subclass)
        - Data flow: `/proc` or sysfs → `Measurement` → `JsonReportGenerator` → `results.json`
        - Threading: does collection run in the metric's own thread?
        - Memory: which `Measurement` objects or `std::map` entries are needed

3. **Flowchart Creation**
    - Develop flowcharts to represent the collection and reporting workflow.
    - Use `mermaid` syntax:

    ```mermaid
    flowchart TD
        A[StartCollection called] --> B[Spawn collection thread]
        B --> C{mQuit?}
        C -->|No| D[Read /proc or sysfs]
        D --> E[AddSample to Measurement]
        E --> F[Wait for frequency interval]
        F --> C
        C -->|Yes| G[Thread exits]
        G --> H[StopCollection joins thread]
        H --> I[SaveResults writes JSON]
    ```

4. **Sequence Diagrams**
    - Create sequence diagrams showing interactions between `main`, `IMetric` subclass, `JsonReportGenerator`, and system files.
    - Use `mermaid` syntax.

5. **LLD Preparation**
    - Prepare an LLD document outlining:
        - New `IMetric` subclass design or new method in `MemoryMetric`
        - New fields in `JsonReportGenerator` output
        - New `Platform` enum values and switch cases required
        - New `Measurement` objects and their keys in the JSON schema
        - Error handling for absent `/proc` files or unsupported platform

6. **Fine Tuning**
    - Do not create implementation roadmap markdown files.
    - Do not suggest timelines or planning details for execution.
    - Always verify the JSON schema impact: list every key that is added, changed, or removed.
