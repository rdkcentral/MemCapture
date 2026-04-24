---
name: memory-safety-analyzer
description: Analyze C++17 MemCapture code for memory safety issues including resource leaks, use-after-free, unclosed ifstream files, dangling iterators, and improper shared_ptr cycles. Use when reviewing metric code, file parsers, or report generation.
---

# Memory Safety Analysis for MemCapture C++17

## Purpose

Systematically analyze MemCapture C++17 code for memory safety issues that can cause crashes,
resource exhaustion, or undefined behaviour on resource-constrained RDK devices.

## Usage

Invoke this skill when:
- Reviewing new metric code (`MemoryMetric`, `ProcessMetric`, `CpuIdleMetric`)
- Auditing file parser classes (`MemInfo`, `Smaps`)
- Reviewing `JsonReportGenerator` or `Procrank` resource management
- Debugging crashes or hangs in the collection loop
- Preparing a new platform port

## Analysis Process

### Step 1: Identify All Resource Acquisitions

Search the code for:
- `std::make_shared`, `std::make_unique`, `new`
- `std::ifstream`, `std::ofstream` — must not outlive their containing scope
- `std::thread` — must be joined or detached before destruction
- `std::mutex`, `std::condition_variable` — must not be destroyed while locked
- `nlohmann::json` objects — large, must not be unnecessarily copied

For each acquisition, verify:
1. Ownership is clear (unique vs. shared)
2. Destructor or RAII wrapper handles cleanup
3. Error paths do not skip cleanup
4. No double-free or double-join possible

### Step 2: Check `std::shared_ptr` Usage

`JsonReportGenerator` is shared via `std::shared_ptr` across all metric objects:
- Verify no circular `shared_ptr` references exist
- Verify `JsonReportGenerator` outlives all metrics (it is constructed first, destroyed last in `main`)
- Do NOT store raw pointers to `JsonReportGenerator` outside `shared_ptr`

```cpp
// GOOD: shared ownership via shared_ptr
auto reportGen = std::make_shared<JsonReportGenerator>(dir, duration);
auto metric    = std::make_unique<MemoryMetric>(platform, reportGen);

// BAD: raw pointer to shared resource
JsonReportGenerator* rawPtr = reportGen.get();  // Dangerous if shared_ptr is reset
```

### Step 3: Check Collection Thread Lifecycle

Each metric spawns exactly one `std::thread`:
```cpp
// GOOD: Thread created in StartCollection, joined in StopCollection
void MemoryMetric::StartCollection(std::chrono::seconds frequency) {
    mQuit = false;
    mThread = std::thread(&MemoryMetric::CollectData, this, frequency);
}

void MemoryMetric::StopCollection() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mQuit = true;
    }
    mCv.notify_one();
    if (mThread.joinable()) mThread.join();
}

// BAD: Thread not joined — destructor called on joinable thread → std::terminate
~MemoryMetric() {
    // Thread still running here if StopCollection was not called!
}
```

Check:
1. `StopCollection()` always calls `join()` before returning
2. Destructor does not destroy `mCv` or `mMutex` while thread is still waiting
3. `mQuit` is an `std::atomic<bool>` or protected by `mMutex`

### Step 4: Check /proc File Parsing

```cpp
// GOOD: ifstream scoped inside the parsing function — closed automatically
void MemoryMetric::GetLinuxMemoryUsage() {
    MemInfo memInfo;  // Constructor opens and parses /proc/meminfo, destructor closes
    long total = memInfo.Total();
    // ...
}

// BAD: ifstream member variable not closed on error path
class BadMetric {
    std::ifstream mFile;  // If constructor throws, destructor may not close!
};
```

Also check:
- Every `std::ifstream` is checked with `if (!f.is_open())` before reading
- `std::getline` loop handles empty lines and malformed data gracefully
- Missing files (optional platform features) are logged at WARN level, not ERROR

### Step 5: Check Measurement Accumulation

```cpp
// GOOD: Measurement handles its own lifecycle
Measurement m("MemTotal");
m.AddSample(value);  // safe for first sample

// BAD: Iterator invalidation
auto& ref = mLinuxMemoryMeasurements["MemTotal"];
mLinuxMemoryMeasurements["MemFree"] = Measurement("MemFree");  // may rehash map!
ref.AddSample(100);  // ref is now dangling
```

For `std::map<std::string, Measurement>`:
- Do not hold references or iterators across insertions
- Use `emplace` with a moved value to avoid unnecessary copies

### Step 6: Static Analysis

```bash
# Build with AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
cmake --build .
./MemCapture --duration 10 --platform AMLOGIC --output-dir /tmp/test/

# Cppcheck for MemCapture source
cppcheck --enable=all --std=c++17 --suppress=missingInclude \
         MemoryMetric.cpp ProcessMetric.cpp FileParsers/

# Compiler warnings
g++ -Wall -Wextra -std=c++17 -fsanitize=address MemoryMetric.cpp
```

### Step 7: Dynamic Analysis

```bash
# Valgrind (slower but thorough)
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./MemCapture --duration 5 --platform AMLOGIC --output-dir /tmp/valgrind_test/
```

## Common Issues and Fixes

### Issue: Thread not joined on destructor

```cpp
// PROBLEM
MemoryMetric::~MemoryMetric() {
    mQuit = true;
    // Thread not notified or joined → std::terminate!
}

// FIX
MemoryMetric::~MemoryMetric() {
    StopCollection();  // Notifies and joins
}
```

### Issue: ifstream not checked before use

```cpp
// PROBLEM
std::ifstream f("/proc/brcm/bmem");
std::string line;
while (std::getline(f, line)) { ... }  // Silently does nothing if file absent

// FIX
std::ifstream f("/proc/brcm/bmem");
if (!f.is_open()) {
    LOG_WARN("BMEM file not available — skipping");
    return;
}
while (std::getline(f, line)) { ... }
```

### Issue: Map iterator invalidation

```cpp
// PROBLEM
auto& ref = mMeasurements["key1"];
mMeasurements["key2"] = Measurement("key2");  // Rehash may invalidate ref!
ref.AddSample(value);  // Undefined behaviour

// FIX: Find or insert first, then use
mMeasurements.emplace("key1", Measurement("key1"));
mMeasurements.emplace("key2", Measurement("key2"));
mMeasurements.at("key1").AddSample(value);  // Safe after all insertions
```

### Issue: Shared_ptr to self in metric callback

```cpp
// PROBLEM: Circular reference keeps JsonReportGenerator alive forever
struct MetricCallback {
    std::shared_ptr<MetricCallback> self;  // Cycle!
    std::shared_ptr<JsonReportGenerator> reporter;
};

// FIX: Use std::weak_ptr for back-references
struct MetricCallback {
    std::weak_ptr<MetricCallback> self;
    std::shared_ptr<JsonReportGenerator> reporter;
};
```

## Output Format

Provide findings as:

```
## Memory Safety Analysis — MemCapture

### Critical Issues (must fix)
1. [MemoryMetric.cpp:45] Thread not joined in destructor — std::terminate risk
2. [FileParsers/MemInfo.cpp:82] ifstream not checked after open — silent failure on RDK devices

### Warnings (should fix)
1. [MemoryMetric.cpp:210] Map reference held across potential rehash
2. [Procrank.cpp:93] Raw pointer stored to shared JsonReportGenerator

### Recommendations
1. Add `StopCollection()` call in all IMetric destructors
2. Guard all /proc file opens with `if (!f.is_open())` checks
3. Run AddressSanitizer as part of CI pipeline

### Suggested Fixes
[Provide specific code changes for each issue]
```

## Verification

After fixes:
1. AddressSanitizer clean on a 30-second test capture
2. Valgrind shows no leaks
3. All platforms build without new warnings
4. Code review by human
5. JSON and HTML report output unchanged from baseline
