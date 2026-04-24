---
name: thread-safety-analyzer
description: Analyze MemCapture C++17 code for thread safety issues including race conditions on shared metrics maps, deadlocks in metric shutdown, improper std::atomic usage for mQuit flags, and collection thread lifecycle problems. Use when reviewing concurrent metric code or debugging threading issues.
---

# Thread Safety Analysis for MemCapture C++17

## Purpose

Systematically analyze MemCapture's multi-threaded metric collection code for thread safety
issues that can cause race conditions, deadlocks, or data corruption during capture.

## Usage

Invoke this skill when:
- Reviewing new `IMetric` subclass implementations
- Debugging a hang at end of capture (stuck `StopCollection()`)
- Investigating data corruption in `results.json` (garbled values)
- Adding a new field to `mLinuxMemoryMeasurements` map
- Changing the `ConditionVariable` or `mQuit` flag logic
- Reviewing `JsonReportGenerator` access patterns

## MemCapture Threading Architecture

```
main thread
│
├── MemoryMetric thread  (mThread)
│     ├── reads /proc/meminfo, /proc/brcm/, sysfs GPU nodes
│     ├── writes mLinuxMemoryMeasurements (map<string, Measurement>)
│     └── waits on mCv (ConditionVariable)
│
├── ProcessMetric thread  (mThread)
│     ├── reads /proc/<pid>/smaps via Procrank
│     ├── writes mProcessMeasurements
│     └── waits on its own ConditionVariable
│
└── CpuIdleMetric thread  (mThread)
      ├── reads /sys/devices/system/cpu/cpu0/cpuidle/
      ├── writes mCpuIdleMeasurements
      └── waits on its own ConditionVariable

After all StopCollection() calls (sequential, from main thread):
└── SaveResults() called for each metric — single-threaded, safe
```

## Analysis Process

### Step 1: Identify All Shared Data

Search for data accessed from both the main thread and a collection thread:
- `mQuit` flag — must be `std::atomic<bool>` or protected by `mMutex`
- `mLinuxMemoryMeasurements`, `mProcessMeasurements` — owned by collection thread only
- `JsonReportGenerator` — accessed only by `SaveResults()` (main thread, post-stop)

For each shared variable, verify:
1. Collection thread writes are never concurrent with `SaveResults()` reads
2. `mQuit` set by main thread is immediately visible to collection thread
3. No raw pointer aliases exist to collection-thread-owned data

### Step 2: Verify mQuit Flag Safety

```cpp
// GOOD: std::atomic<bool> — no mutex needed for simple flag
class MemoryMetric : public IMetric {
    std::atomic<bool> mQuit{false};
    std::thread mThread;
    std::mutex mMutex;
    std::condition_variable mCv;
    // ...
};

void MemoryMetric::StopCollection() {
    mQuit = true;          // atomic store — visible to collection thread
    mCv.notify_one();      // wake collection thread
    if (mThread.joinable())
        mThread.join();    // wait for clean exit
}

void MemoryMetric::CollectData(std::chrono::seconds freq) {
    std::unique_lock<std::mutex> lock(mMutex);
    while (!mQuit) {
        mCv.wait_for(lock, freq);  // handles spurious wakeups
        if (mQuit) break;
        lock.unlock();
        // ... collect ...
        lock.lock();
    }
}

// BAD: Non-atomic bool — write from main thread may not be visible
bool mQuit = false;  // Race condition!
```

### Step 3: Verify Collection Thread Lifecycle

```cpp
// CHECK 1: Destructor must not destroy mCv/mMutex while thread is waiting
~MemoryMetric() {
    StopCollection();  // MUST join before members are destroyed
}

// CHECK 2: Thread detach is forbidden — we must join to know it's done
void MemoryMetric::StartCollection(std::chrono::seconds freq) {
    mQuit = false;
    mThread = std::thread(&MemoryMetric::CollectData, this, freq);
    // NEVER: mThread.detach();  — would leave thread using destroyed members
}

// CHECK 3: Double-start is prevented
void MemoryMetric::StartCollection(std::chrono::seconds freq) {
    if (mThread.joinable()) {
        LOG_WARN("Collection already running — ignoring StartCollection");
        return;
    }
    // ...
}
```

### Step 4: Verify Measurement Map Thread Safety

`mLinuxMemoryMeasurements` is a `std::map<std::string, Measurement>`:
- **Only the collection thread writes to it** (via `AddSample`)
- **Only the main thread reads from it** (via `SaveResults()`, after join)
- No concurrent access = no mutex needed

**BUT:** Violations to watch for:

```cpp
// BAD: Main thread reads while collection thread may still be running
void BadUsage() {
    metric.StartCollection(freq);
    // ... immediately read results ...
    auto& m = metric.GetMeasurement("MemTotal");  // RACE with collection thread!
}

// GOOD: Always stop before accessing measurements
metric.StopCollection();   // join() returns only after thread exits
metric.SaveResults();      // safe: no concurrent writes possible
```

### Step 5: Check ConditionVariable Usage

```cpp
// GOOD: wait_for with predicate handles spurious wakeups
mCv.wait_for(lock, freq, [this] { return mQuit.load(); });

// GOOD: Manual loop also works
while (!mQuit) {
    auto status = mCv.wait_for(lock, freq);
    if (mQuit) break;
    // collect data
}

// BAD: Single wait without loop — spurious wakeup runs collection early
mCv.wait_for(lock, freq);  // may return early! No predicate check.
if (mQuit) return;         // missing: not re-checking condition
```

### Step 6: Check JsonReportGenerator Concurrency

`JsonReportGenerator` is **not internally thread-safe**. The design contract is:
- It is constructed once (main thread, before any `StartCollection()`)
- It is written to only in `SaveResults()` (main thread, after all `StopCollection()`)
- Collection threads **must not** call any `JsonReportGenerator` methods

```cpp
// BAD: Calling SaveResults from inside the collection thread
void MemoryMetric::CollectData(std::chrono::seconds freq) {
    while (!mQuit) {
        mCv.wait_for(lock, freq);
        Collect();
        mReportGenerator->SaveResults();  // WRONG — not from collection thread!
    }
}

// GOOD: SaveResults called only from main thread after join
metric.StopCollection();
metric.SaveResults();   // safe
```

## Running Thread Safety Analysis

### ThreadSanitizer (fastest, catches most races)

```bash
mkdir build_tsan && cd build_tsan
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
      -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      ..
cmake --build . --parallel $(nproc)

# Run a capture; TSan will print any data races to stderr
./MemCapture --platform AMLOGIC --duration 30 \
             --output-dir /tmp/tsan_test/ 2>&1 | tee tsan.log
```

### Helgrind (deeper lock-order analysis)

```bash
valgrind --tool=helgrind \
         --track-lockorders=yes \
         ./MemCapture --platform AMLOGIC --duration 10 \
                      --output-dir /tmp/helgrind_test/ 2>&1 | tee helgrind.log
```

## Common Issues and Fixes

### Issue: mQuit is a plain bool

```cpp
// PROBLEM
bool mQuit = false;  // Not atomic — write from main thread may not be visible

// FIX
std::atomic<bool> mQuit{false};  // Atomic — guaranteed visibility
```

### Issue: Collection thread not joined in destructor

```cpp
// PROBLEM
~MemoryMetric() {
    mQuit = true;  // Thread never notified or joined
}   // mCv destroyed while thread is waiting → crash

// FIX
~MemoryMetric() {
    StopCollection();  // notify + join before any member destruction
}
```

### Issue: Map reference held across collection cycle

```cpp
// PROBLEM
auto& ref = mLinuxMemoryMeasurements["MemTotal"];
mLinuxMemoryMeasurements.emplace("MemFree", Measurement("MemFree"));  // may rehash
ref.AddSample(value);  // DANGLING — ref invalidated by rehash

// FIX: Insert all keys at startup (constructor), then only AddSample
mLinuxMemoryMeasurements.emplace("MemTotal", Measurement("MemTotal"));
mLinuxMemoryMeasurements.emplace("MemFree", Measurement("MemFree"));
// In collection loop:
mLinuxMemoryMeasurements.at("MemTotal").AddSample(value);  // safe — no rehash
```

## Output Format

```
## Thread Safety Analysis — MemCapture

### Critical Issues (must fix)
1. [MemoryMetric.h:42] mQuit is plain bool — use std::atomic<bool>
2. [CpuIdleMetric.cpp:88] SaveResults() called from collection thread — race condition

### Warnings (should fix)
1. [ProcessMetric.cpp:120] Map reference held across emplace — potential invalidation

### Verified Safe
- mLinuxMemoryMeasurements: only written by collection thread, read by SaveResults() after join ✅
- JsonReportGenerator: constructed before threads start, written only from main thread ✅

### Suggested Fixes
[Specific code changes for each issue]
```

## Verification

After fixes:
1. ThreadSanitizer reports no data races on a 60-second capture
2. Helgrind reports no lock ordering violations
3. All platforms build without warnings (`-Wall -Wextra`)
4. JSON report output matches baseline after refactor
