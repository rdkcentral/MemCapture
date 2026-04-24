---
name: triage-logs
description: >
  Triage MemCapture behavioural issues by correlating the HTML/JSON report output and stderr
  logs with the MemCapture source tree. Covers metric collection failures, missing data in
  reports, platform-specific sysfs/proc read errors, process grouping problems, and report
  generation issues. The user states the issue; this skill guides systematic root-cause analysis.
---

# MemCapture Output & Log Triage Skill

## Purpose

Systematically correlate MemCapture output (HTML report, `results.json`, stderr) with the
source code to identify likely root causes and propose fixes for any behavioural anomaly.

---

## Usage

Invoke this skill when:
- MemCapture exits with a non-zero code or crashes
- The HTML report is missing expected sections (memory table, process table, CPU idle)
- `results.json` is missing expected keys or contains zero/unexpected values
- A metric shows 0 or `null` for all samples on a specific platform
- Process grouping is not working as expected
- MemCapture is consuming excessive CPU or memory on the target device

The user's stated issue drives the investigation. Do not assume a fixed failure mode.

---

## Step 1: Orient to the Output Bundle

Typical files to inspect first:

```text
<output-dir>/
    report.html          <- HTML report (open in browser)
    results.json         <- Raw data (use jq or python to inspect)
stderr from launch       <- Capture with: ./MemCapture ... 2>stderr.log
```

Collect relevant context:
```bash
# Inspect JSON top-level keys
python3 -c "import json; d=json.load(open('results.json')); print(list(d.keys()))"

# Check for zero-value metrics
python3 -c "
import json
d = json.load(open('results.json'))
for k, v in d.get('memory', {}).items():
    if isinstance(v, dict) and v.get('mean', -1) == 0:
        print(f'ZERO: {k}')
"

# Check stderr for /proc or sysfs errors
grep -i "error\|warn\|fail\|not found\|unable" stderr.log
```

---

## Step 2: Map Issue to Source Component

| Symptom | Source Files |
|---------|-------------|
| MemCapture crashes at startup | `main.cpp` (arg parsing, signal handler) |
| Linux memory table empty | `MemoryMetric.cpp::GetLinuxMemoryUsage()`, `FileParsers/MemInfo.cpp` |
| CMA data missing | `FileParsers/MemInfo.cpp` (CmaTotal/CmaFree) |
| GPU memory all zeros | `MemoryMetric.cpp::GetGpuMemoryUsage<Platform>()` |
| Container memory missing | `MemoryMetric.cpp::GetContainerMemoryUsage()` |
| Memory bandwidth missing | `MemoryMetric.cpp::GetMemoryBandwidth()` |
| Fragmentation data missing | `MemoryMetric.cpp::CalculateFragmentation()`, `/proc/buddyinfo` |
| Process table empty | `ProcessMetric.cpp`, `Procrank.cpp`, `FileParsers/Smaps.cpp` |
| Process grouping wrong | `GroupManager.cpp`, groups JSON file |
| CPU idle table missing | `CpuIdleMetric.cpp` (requires `-c` flag and kernel support) |
| HTML report not generated | `JsonReportGenerator.cpp`, `templates/template.html` (incbin) |
| results.json not generated | `JsonReportGenerator.cpp` (requires `-j` flag) |
| Metadata shows "unknown" | `Metadata.cpp` (RDK-E / RDK-V device metadata when available; expected on non-RDK hosts) |

---

## Step 3: Identify the Anomaly

### MemCapture Does Not Start or Exits Immediately

```bash
# Check for missing required args
./MemCapture --help

# Check stderr for arg parse errors
./MemCapture --platform INVALID 2>&1

# Check for output directory creation failures
./MemCapture --output-dir /read-only-dir/ 2>&1
```

Look for:
- Unrecognised platform string (only: AMLOGIC, AMLOGIC_950D4, REALTEK, REALTEK64, BROADCOM, MEDIATEK)
- Output directory not writable
- Template binary (incbin) corruption — rare; requires rebuild

### Linux Memory Table Missing or All Zeros

```bash
# Verify /proc/meminfo is readable
cat /proc/meminfo | head -5

# Check MemInfo parsing
grep -n "parseMemInfo\|MemTotal\|MemFree" FileParsers/MemInfo.cpp
```

Look for:
- `/proc/meminfo` line format mismatch (custom kernel)
- `MemInfo` constructor exception not caught (check stderr)

### GPU Memory All Zeros on a Specific Platform

```bash
# Check which platform method is called
grep -n "GetGpuMemoryUsage" MemoryMetric.cpp

# Verify the sysfs node exists on the device
ls /sys/kernel/debug/mali*/  # Amlogic
ls /proc/brcm/               # Broadcom
```

Look for:
- Sysfs node path changed in a new kernel version
- Missing `std::filesystem::exists()` guard — method returns silently
- New platform added to `Platform` enum but switch case missing in `GetGpuMemoryUsage()`

### Process Table Empty

```bash
# Check if /proc/<pid>/smaps_rollup or smaps is readable
ls /proc/1/smaps_rollup 2>/dev/null || ls /proc/1/smaps

# Check Procrank for errors
grep -n "smaps\|smaps_rollup\|opendir\|readdir" Procrank.cpp
```

Look for:
- SELinux or permission policy blocking `/proc/<pid>/smaps` reads
- `smaps_rollup` not available on older kernels (fallback to `smaps` should occur)
- PID directory disappears between `/proc` scan and `smaps` open (ephemeral processes)

### Process Grouping Not Working

```bash
# Verify groups JSON file syntax
python3 -c "import json; json.load(open('groups.json')); print('JSON valid')"

# Check GroupManager regex matching
grep -n "getGroup\|regex\|group" GroupManager.cpp
```

Look for:
- Regex syntax error in groups JSON (`"processes"` field)
- `gEnableGroups` flag not set (requires `-g <path>` argument)
- Process name includes path prefix not matched by the regex

### CPU Idle Metrics Missing

```bash
# Check kernel support for idle stats
ls /sys/devices/system/cpu/cpu0/cpuidle/ 2>/dev/null

# Verify -c flag was passed
./MemCapture --cpuidle --platform AMLOGIC ...
```

Look for:
- Kernel built without `CONFIG_CPU_IDLE` support
- `-c` / `--cpuidle` flag not passed at launch
- `CpuIdleMetric::StartCollection()` returning early without log

---

## Step 4: Correlate with Source Code

### Collection Thread Lifecycle Issues

If MemCapture hangs at the end of the capture duration:
- `StopCollection()` did not notify the condition variable
- Collection thread is blocked on a slow `std::ifstream` read (e.g., unresponsive sysfs)
- `mThread.join()` is waiting for a thread that never exits

```bash
# Check if MemCapture is stuck
ps aux | grep MemCapture
strace -p <pid> -e trace=read,write,futex 2>&1 | head -50
```

### JSON Report Schema Issues

If `results.json` is missing keys:
- Check `JsonReportGenerator::SaveResults()` for the expected key names
- Verify `StopCollection()` and `SaveResults()` were called for every metric

```bash
# List all keys output by the report generator
grep -n 'operator\[\|\.push_back\|emplace' JsonReportGenerator.cpp
```

---

## Step 5: Reproduce Locally

### Reproduce on Development Host (Linux)

```bash
# Build and run a short capture
mkdir -p build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake ..
cmake --build . --parallel $(nproc)

./MemCapture --platform AMLOGIC --duration 10 --json \
             --output-dir /tmp/triage_test/ 2>stderr.log

# Inspect output
python3 -c "import json; d=json.load(open('/tmp/triage_test/results.json')); print(list(d.keys()))"
cat stderr.log
```

### Reproduce Platform-Specific Failure

Test on the target device by copying the binary and running:
```bash
./MemCapture --platform BROADCOM --duration 10 --json \
             --output-dir /tmp/memcapture_triage/ 2>stderr.log
```

---

## Step 6: Propose Fix and Test

### Fix Template — Missing Platform Sysfs Guard

```cpp
// BEFORE: Silent failure if sysfs node is absent
void MemoryMetric::GetGpuMemoryUsageBroadcom() {
    std::ifstream f("/proc/brcm/gpu_memory");
    // ... parses nothing if file is absent
}

// AFTER: Guard with existence check and warning
void MemoryMetric::GetGpuMemoryUsageBroadcom() {
    const std::filesystem::path gpuPath = "/proc/brcm/gpu_memory";
    if (!std::filesystem::exists(gpuPath)) {
        LOG_WARN("Broadcom GPU sysfs node not found — skipping GPU metrics");
        return;
    }
    std::ifstream f(gpuPath);
    if (!f.is_open()) {
        LOG_ERROR("Failed to open %s", gpuPath.c_str());
        return;
    }
    // ...
}
```

### Fix Template — Missing Platform Switch Case

```cpp
// BEFORE: New platform silently produces no GPU data
void MemoryMetric::GetGpuMemoryUsage() {
    switch (mPlatform) {
        case Platform::AMLOGIC: GetGpuMemoryUsageAmlogic(); break;
        // MEDIATEK missing!
    }
}

// AFTER: Add missing case
case Platform::MEDIATEK:
    GetGpuMemoryUsageMediatek(); break;
```

---

## Output Format

Present findings in this structure:

```markdown
## Triage Summary

**Issue:** <user's stated problem>
**Evidence:** <key log lines or JSON values>
**Root Cause:** <likely cause based on code analysis>
**Impact:** <what is missing or wrong in the report>

## Code Location

**File:** <source file path>
**Function:** <function name>
**Line:** <approximate line number>

## Reproduction

[bash command to reproduce]

## Proposed Fix

[code diff or description]

## Test / Validation

[how to verify the fix — diff results.json, re-run MemCapture]
```



