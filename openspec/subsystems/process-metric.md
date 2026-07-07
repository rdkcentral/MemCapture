# ProcessMetric, Procrank, and Smaps

## Responsibilities

### Facts - Responsibilities

- `ProcessMetric` periodically snapshots all running processes through `Procrank::GetMemoryUsage()` (`ProcessMetric.cpp:80`, `Procrank.cpp:48`).
- For each process, it accumulates `Pss`, `Rss`, `Uss`, `Vss`, `Swap`, `SwapPss`, `SwapZram`, `Locked` in `processMeasurement` (`ProcessMetric.cpp:94` onward, `ProcessMeasurement.h`).
- On save, it deduplicates dead repeated command invocations and sends results to report generator (`ProcessMetric.cpp:58`, `ProcessMetric.cpp:145`).

## Procrank Relationship

### Facts - Procrank

- Procrank enumerates numeric `/proc` entries and constructs `Process` objects (`Procrank.cpp:138`, `Procrank.cpp:60`).
- Per process memory is derived from `Smaps` parser (`Procrank.cpp:165`).
- `swap_zram` is computed from `swap_pss * zramCompressionRatio` (`Procrank.cpp:174`).

### Inferences - Procrank

- This is a userspace procrank replacement optimized for repeated snapshotting inside a fixed capture window.

## Smaps Relationship

### Facts - Smaps

- `Smaps` prefers `/proc/<pid>/smaps_rollup` when available; otherwise it parses full `/proc/<pid>/smaps` (`FileParsers/Smaps.cpp:34`).
- Parsed fields include `Pss`, `Rss`, `Swap`, `SwapPss`, `Locked`, `Private_Clean`, `Private_Dirty`, `Size` (`FileParsers/Smaps.cpp:134`).
- `Uss` is `Private_Clean + Private_Dirty`; `Vss` is `Size` (`FileParsers/Smaps.h`).

## Group and Container Attribution

### Facts - Group Attribution

- `Process` caches command line/name/ppid and cgroup-derived container/systemd labels (`Process.cpp:29` through `Process.cpp:34`).
- Group assignment prioritizes container match then process name then cmdline regex match through `GroupManager` (`Process.cpp:148` onward).

## Unknowns

- Accuracy of dead-process deduplication under PID reuse in very long captures should be validated with stress tests.
