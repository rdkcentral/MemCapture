# Measurement Accumulation Model

## Facts

- `Measurement` tracks running `min`, `max`, `average`, and total/count state (`Measurement.h`, `Measurement.cpp:40`).
- `AddDataPoint` updates min/max and computes average as total/count (`Measurement.cpp:40` through `Measurement.cpp:56`).
- `ToJson` serializes shape `{ min, max, average }` with rounded integers (`Measurement.cpp:92`).
- Process metrics store one `Measurement` per memory dimension in `processMeasurement` (`ProcessMeasurement.h`).
- Memory metric stores per-category/per-region `Measurement` instances in maps (`MemoryMetric.h`).

## Inferences

- Statistical state is capture-window aggregate; no time series per sample is retained.

## Assumptions

- Potential numeric overflow concern in code comment (long captures) is accepted for current duration profiles.

## Unknowns

- Maximum expected capture durations in production are not documented in source.
