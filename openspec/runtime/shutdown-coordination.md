# Shutdown Coordination (ConditionVariable + mQuit)

## Facts

- Main thread uses custom `ConditionVariable` (`gStop`) to sleep for capture duration (`main.cpp:66`, `main.cpp:257`).
- Signal handler (`SIGTERM`, `SIGINT`) sets `gEarlyTermination=true` and broadcasts `gStop.notify_all()` (`main.cpp:170` through `main.cpp:176`).
- `ConditionVariable` implementation uses monotonic clock for timed waits to avoid wall-clock jumps (`ConditionVariable.h:31`, `ConditionVariable.h:41`, `ConditionVariable.h:135`).
- Each threaded metric stop path sets `mQuit=true`, notifies metric CV, and joins (`ProcessMetric.cpp:45`, `MemoryMetric.cpp:175`).

## Inferences

- Shutdown is cooperative and bounded by collection-loop responsiveness to notification.

## Assumptions

- No deadlock expected because each metric owns only one worker thread and joins outside lock after notification.

## Unknowns

- No watchdog timeout if a metric thread blocks on slow filesystem call during shutdown; this requires runtime stress validation.
