# File Parser Subsystem (MemInfo + Smaps)

## MemInfo Parser

### Facts - MemInfo

- Reads `/proc/meminfo` line-by-line and parses `MemTotal`, `MemFree`, `MemAvailable`, `Buffers`, `Cached`, `Slab`, `SReclaimable`, `SUnreclaim`, `SwapTotal`, `SwapFree`, `CmaTotal` (`FileParsers/MemInfo.cpp:31`).
- Calculates `MemUsed` as `MemTotal - (MemFree + Buffers + Cached + SReclaimable)` (`FileParsers/MemInfo.cpp:72`).
- Exposes methods consumed by `MemoryMetric::GetLinuxMemoryUsage` (`MemoryMetric.cpp:381` onward).
- `MemInfo` exposes `CmaFree()` in the interface, but `parseMemInfo()` does not parse `CmaFree` (only `CmaTotal`) in current code (`FileParsers/MemInfo.h:94`, `FileParsers/MemInfo.cpp:42` through `FileParsers/MemInfo.cpp:64`).

### Inferences - MemInfo

- Reported Linux Used follows a reclaim-aware approximation rather than raw kernel `MemTotal-MemFree`.

## Smaps Parser

### Facts - Smaps

- Chooses `smaps_rollup` when available for performance; otherwise sums full `smaps` entries (`FileParsers/Smaps.cpp:34`).
- `parseSmapsLine` maps key/value lines to enum fields and numeric values (`FileParsers/Smaps.cpp:134`).
- Missing files are treated as transient process death and silently ignored (`FileParsers/Smaps.cpp:46`, `FileParsers/Smaps.cpp:93`).

### Relationship to Measurement

#### Facts

- Smaps does not aggregate min/max/average itself; it provides snapshot values to Procrank, which are later accumulated by `Measurement` in `ProcessMetric`.
