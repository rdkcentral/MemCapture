---
name: platform-portability-checker
description: Verify MemCapture C++17 code is correct and portable across all supported RDK device platforms (AMLOGIC, AMLOGIC_950D4, REALTEK, REALTEK64, BROADCOM, MEDIATEK). Use when reviewing platform-specific metric code, adding a new platform target, or auditing sysfs/proc path assumptions.
---

# Platform Portability Checker for MemCapture

## Purpose

Ensure MemCapture code correctly handles all supported hardware platforms without omissions,
hardcoded paths, or silent fallbacks that hide missing metrics.

## When to Use

- Adding a new platform to the `Platform` enum
- Adding a new GPU or bandwidth metric with platform-specific paths
- Reviewing a PR that changes switch statements in `MemoryMetric.cpp`
- Investigating missing metrics in `results.json` for a specific device
- Auditing all sysfs/proc file access for correctness

## Supported Platforms

| Enum Value        | String arg    | Notes                                  |
|-------------------|---------------|----------------------------------------|
| `AMLOGIC`         | `AMLOGIC`     | Default. Meson SoC.                    |
| `AMLOGIC_950D4`   | `AMLOGIC_950D4`| Amlogic T950D4 variant                |
| `REALTEK`         | `REALTEK`     | 32-bit Realtek SoC                     |
| `REALTEK64`       | `REALTEK64`   | 64-bit Realtek SoC                     |
| `BROADCOM`        | `BROADCOM`    | BCM SoC; uses `/proc/brcm/`            |
| `MEDIATEK`        | `MEDIATEK`    | MediaTek SoC                           |

## Platform-Specific Paths

| Metric            | Platform     | Path                                             |
|-------------------|--------------|--------------------------------------------------|
| GPU memory        | AMLOGIC/AMLOGIC_950D4 | `/sys/kernel/debug/mali*/`              |
| GPU memory        | BROADCOM     | `/proc/brcm/` (Nexus heap nodes)                 |
| GPU memory        | MEDIATEK     | `/sys/kernel/debug/gpu/`                         |
| GPU memory        | REALTEK/REALTEK64 | platform-specific sysfs                     |
| CMA regions       | All          | `/proc/meminfo` (`CmaTotal`, `CmaFree`)          |
| BMEM              | BROADCOM     | `/proc/brcm/bmem`                                |
| Bandwidth         | AMLOGIC      | `/sys/class/aml_ddr/bandwidth`                   |

## Portability Checklist

### 1. Exhaustive Platform Switch

Every `switch (mPlatform)` in `MemoryMetric.cpp` **must** handle all 6 platform variants.

```cpp
// GOOD: All cases covered, default warns
switch (mPlatform) {
    case Platform::AMLOGIC:
    case Platform::AMLOGIC_950D4:
        return GetGpuMemoryUsageAmlogic();
    case Platform::BROADCOM:
        return GetGpuMemoryUsageBroadcom();
    case Platform::MEDIATEK:
        return GetGpuMemoryUsageMediatek();
    case Platform::REALTEK:
    case Platform::REALTEK64:
        return GetGpuMemoryUsageRealtek();
    default:
        LOG_WARN("GetGpuMemoryUsage: unknown platform %d", static_cast<int>(mPlatform));
        return std::nullopt;
}

// BAD: Missing AMLOGIC_950D4 — silently returns nothing for that SoC
switch (mPlatform) {
    case Platform::AMLOGIC:
        return GetGpuMemoryUsageAmlogic();
    // ...
}
```

### 2. sysfs/proc Path Existence Guards

Every read of a platform-specific path must be guarded with `std::filesystem::exists()`.

```cpp
#include <filesystem>

// GOOD: Guard before open
std::filesystem::path gpuPath{"/sys/kernel/debug/mali0/total_pages"};
if (!std::filesystem::exists(gpuPath)) {
    LOG_WARN("GPU path not found: %s", gpuPath.c_str());
    return std::nullopt;
}
std::ifstream gpuFile{gpuPath};

// BAD: Silent open failure — returns 0 or garbage
std::ifstream gpuFile{"/sys/kernel/debug/mali0/total_pages"};
int pages = 0;
gpuFile >> pages;   // No check — may be at EOF from failed open
```

### 3. Use std::optional for Absent Metrics

When a platform does not support a metric, return `std::nullopt` rather than 0 or -1.

```cpp
// GOOD: Caller can skip absent metrics
std::optional<uint64_t> MemoryMetric::GetGpuMemoryUsage() {
    if (mPlatform != Platform::AMLOGIC) return std::nullopt;
    // ...
    return pages * pageSize;
}

// BAD: 0 is ambiguous — is GPU 0 bytes or absent?
int64_t MemoryMetric::GetGpuMemoryUsage() {
    if (mPlatform != Platform::AMLOGIC) return 0;
    // ...
}
```

### 4. Fixed-Width Integers for /proc Values

Values from `/proc/meminfo` and sysfs can exceed 4 GB on devices with large RAM.

```cpp
// GOOD: 64-bit for memory quantities
uint64_t memTotal = 0;

// BAD: 32-bit overflows at 4 GB
uint32_t memTotal = 0;
int memTotal = 0;
```

### 5. std::filesystem for Path Construction

Never concatenate path strings manually.

```cpp
// GOOD
std::filesystem::path cpuIdlePath = std::filesystem::path{"/sys/devices/system/cpu"}
    / ("cpu" + std::to_string(cpuNum))
    / "cpuidle" / "state0" / "time";

// BAD: Fragile string concatenation, not portable across path separators
std::string cpuIdlePath = "/sys/devices/system/cpu/cpu" +
    std::to_string(cpuNum) + "/cpuidle/state0/time";
```

### 6. New Platform Checklist

When adding a new platform, verify ALL of the following:

- [ ] New value added to `Platform` enum in `Platform.h`
- [ ] New string mapped in `main.cpp` argument parser
- [ ] All `switch (mPlatform)` statements in `MemoryMetric.cpp` updated (add case or fall-through)
- [ ] New `GetGpuMemoryUsage<Name>()` method added if GPU path differs
- [ ] Platform-specific path verified with `std::filesystem::exists()` guard
- [ ] `std::optional<uint64_t>` returned for metrics not available on new platform
- [ ] Integration tested: run `MemCapture --platform <NEW> --duration 10` and inspect JSON

## Analysis Steps

### Step 1: Enumerate All Switch Statements

```bash
grep -n "switch (mPlatform)\|switch(mPlatform)" MemoryMetric.cpp
```

For each switch, check the case list against the 6 supported platforms above.

### Step 2: Check for Unguarded File Opens

```bash
grep -n "ifstream\|fopen\|open(" MemoryMetric.cpp CpuIdleMetric.cpp Procrank.cpp
```

Each open should be preceded by `std::filesystem::exists()` or followed by an `is_open()` check that returns `std::nullopt` / logs a warning.

### Step 3: Check Integer Types on /proc Values

```bash
grep -n "int \|uint32_t\|long " MemoryMetric.cpp FileParsers/MemInfo.cpp FileParsers/Smaps.cpp
```

Any plain `int` or `uint32_t` storing bytes-from-proc should be flagged.

### Step 4: Run All Platforms in Simulation

On a build host (no target device), validate that MemCapture at least starts without crash for each platform (even if metrics return nullopt):

```bash
for PLATFORM in AMLOGIC AMLOGIC_950D4 REALTEK REALTEK64 BROADCOM MEDIATEK; do
  echo "=== $PLATFORM ==="
  timeout 5 ./MemCapture --platform $PLATFORM --duration 2 --output-dir /tmp/test_$PLATFORM/ \
    2>&1 | head -20 || true
done
```

## Output Format

```
## Platform Portability Report — MemCapture

### Missing Cases in switch(mPlatform)
1. [MemoryMetric.cpp:234] GetGpuMemoryUsage — missing AMLOGIC_950D4 and MEDIATEK

### Unguarded Path Accesses
1. [MemoryMetric.cpp:310] opens /proc/brcm/bmem without filesystem::exists() guard

### Integer Type Issues
1. [FileParsers/MemInfo.cpp:45] uint32_t for MemTotal — overflows at 4 GB

### New Platform Checklist Status
- Platform.h enum: ✅
- main.cpp parser: ✅
- All switch cases: ❌ (missing in GetBandwidth)
- GPU method: ✅
- Path guard: ❌ (no exists() check in GetGpuMemoryUsageFoo)
- optional return: ✅
- Integration test: ⚠️ not verified

### Suggested Fixes
[Specific code changes for each issue]
```

## Verification

After fixes:
1. `grep -c "case Platform::" MemoryMetric.cpp` — count matches 6 × number of switches
2. All `ifstream` openings have `is_open()` checks or `exists()` guards
3. No `uint32_t` or `int` used for memory quantities parsed from `/proc`
4. All 6 platforms produce valid (possibly empty) `results.json` without crashing
