# Annotated report.json Schema (Code-Grounded)

All keys below are emitted from `JsonReportGenerator` and `main` flow.

## Top-Level Keys

### Facts - Top-Level

- `processes` (array): initialized empty; populated by `addProcesses` (`JsonReportGenerator.cpp:30`, `JsonReportGenerator.cpp:106`).
- `metadata` (object): populated in `getJson()` (`JsonReportGenerator.cpp:31`, `JsonReportGenerator.cpp:92`).
- `cpuIdleStats` (object or null): initialized null; populated by `addCpuIdleMetrics` when enabled (`JsonReportGenerator.cpp:32`, `JsonReportGenerator.cpp:202`).
- `grandTotal` (object):
  - `linuxUsage` (number MB)
  - `calculatedUsage` (number MB)
  (`JsonReportGenerator.cpp:34`, `JsonReportGenerator.cpp:35`, `JsonReportGenerator.cpp:186`, `JsonReportGenerator.cpp:192`)
- `data` (array): appended by `addDataset` when non-empty (`JsonReportGenerator.cpp:88`).
- `pssByGroup` (array or null): set in `addProcesses` depending on group manager presence (`JsonReportGenerator.cpp:151`, `JsonReportGenerator.cpp:181`).

## metadata Object

### Facts - metadata

Fields emitted in `getJson()`:

- `image`
- `platform`
- `mac`
- `timestamp`
- `duration` (seconds)
- `swapEnabled`
(`JsonReportGenerator.cpp:95` through `JsonReportGenerator.cpp:101`)

## processes[] Item

### Facts - processes item

Each process object contains:

- Identity and labels: `pid`, `ppid`, `name`, `cmdline`, `systemdService`, `container`, `group`
- Measurements: `rss`, `pss`, `uss`, `vss`, `swap`, `swapPss`, `swapZram`, `locked`
- Units: process measurement values are in `kB` (sourced from smaps/smaps_rollup parsing and carried through `Procrank` + `Measurement` serialization without unit conversion), while `grandTotal.linuxUsage` and `grandTotal.calculatedUsage` are stored in `MB` (`FileParsers/Smaps.cpp:157` through `FileParsers/Smaps.cpp:180`, `Procrank.cpp:170` through `Procrank.cpp:177`, `Measurement.cpp:92` through `Measurement.cpp:98`, `JsonReportGenerator.cpp:186` through `JsonReportGenerator.cpp:199`).

Each measurement object shape is `{ min, max, average }` from `Measurement::ToJson()`.
(See `JsonReportGenerator.cpp:112` through `JsonReportGenerator.cpp:133`, `Measurement.cpp:92`.)

## data[] Dataset Item

### Facts - data dataset

Dataset structure from `addDataset`:

- `name` (string)
- `data` (array of row objects)
- `_columnOrder` (array of column labels for stable template ordering)
(`JsonReportGenerator.cpp:47` through `JsonReportGenerator.cpp:50`)

Row encoding rules:

- scalar string pair values become `row[key] = value`.
- `Measurement` values become nested object:
  - `row[measurementName].Min`
  - `row[measurementName].Max`
  - `row[measurementName].Average`
- `_columnOrder` emits flattened labels for measurement columns using `<measurementName> (Min)`, `<measurementName> (Max)`, `<measurementName> (Average)`.
(`JsonReportGenerator.cpp:61` through `JsonReportGenerator.cpp:76`)

Shape distinction for JSON consumers:

- `processes[]` measurement objects use lowercase keys from `Measurement::ToJson()`: `{ min, max, average }`.
- `data[]` dataset row measurements use capitalized keys: `{ Min, Max, Average }`, and `_columnOrder` uses labeled headers (`<name> (Min|Max|Average)`).

## pssByGroup[] Item

### Facts - pssByGroup item

When grouping enabled, each item has:

- `groupName`
- `pss`
(`JsonReportGenerator.cpp:174` through `JsonReportGenerator.cpp:177`)

## cpuIdleStats Object

### Facts - cpuIdleStats

When present, includes:

- `cpu[i].idle.sum`
- `cpu[i].idle.percent`
- `overall.idle.sum`
- `overall.load.sum`
- `overall.load.count`
- `overall.load.percent`
- `load.lt1ms`, `load.gt1ms`, `load.gt5ms`, `load.gt10ms`, `load.gt20ms`, `load.gt30ms`, `load.gt40ms`, `load.gt50ms`, `load.gt75ms`, `load.gt100ms`
(`JsonReportGenerator.cpp:211` through `JsonReportGenerator.cpp:243`)

## Dataset Names Emitted by Current Code

### Facts - Dataset Names

Potential dataset names from `MemoryMetric::SaveResults`:

- `Linux Memory`
- `Swap Memory`
- `GPU Memory`
- `CMA Regions`
- `CMA Summary`
- `Containers`
- `Memory Bandwidth`
- `Memory Fragmentation - Zone <zone>`
- `BMEM`
(`MemoryMetric.cpp:228` through `MemoryMetric.cpp:360`)

## File Write Sequence

### Facts - File Write Sequence

- JSON is written first (if enabled), then HTML render/write (`main.cpp:327` through `main.cpp:344`).
