---
applyTo: "**/*.cpp,**/*.h,**/*.c"
---

# C++17 Coding Standards for MemCapture

MemCapture is a C++17 memory capture and analysis tool for RDK-based embedded devices.
All new code must follow these standards to ensure correctness, portability, and readability.

## Memory Management

### Prefer RAII and Smart Pointers
```cpp
// GOOD: Shared ownership of report generator
auto reportGenerator = std::make_shared<JsonReportGenerator>(outputDir, duration);

// GOOD: Unique ownership
auto metric = std::make_unique<MemoryMetric>(platform, reportGenerator);

// BAD: Raw new/delete — use smart pointers instead
MemoryMetric* m = new MemoryMetric(platform, reportGenerator);
// ... (leak risk on error path)
delete m;
```

### Move Semantics for Large Objects
```cpp
// GOOD: Move Measurement into the map — avoid copying
mLinuxMemoryMeasurements.emplace("MemTotal", std::move(measurement));

// BAD: Copy when move is available
mLinuxMemoryMeasurements["MemTotal"] = measurement;  // copies all accumulated samples
```

### /proc File Handling
```cpp
// GOOD: Check file existence before reading optional platform files
if (!std::filesystem::exists("/proc/brcm/bmem")) {
    LOG_WARN("BMEM not available on this platform");
    return;
}
std::ifstream f("/proc/brcm/bmem");
if (!f.is_open()) {
    LOG_ERROR("Failed to open /proc/brcm/bmem");
    return;
}
// process file
```

## Resource Constraints

### CPU Optimization
- Use `smaps_rollup` over `smaps` when available — it is an order of magnitude faster for busy systems
- Avoid re-scanning `/proc` directories every collection cycle — cache stable values (e.g., process name)
- Keep the collection loop sleeping on `ConditionVariable::wait_for()` to avoid busy-waiting

### Memory Optimization
- Use `std::optional<T>` instead of sentinel values for absent platform metrics
- Store per-process data in `std::map<pid_t, ProcessMeasurement>` — map keys avoid duplicate entries
- Prefer `std::string_view` for read-only string operations in parsers

## Platform Independence

### Platform Enum and Dispatch
```cpp
// GOOD: Exhaustive switch with explicit default
switch (mPlatform) {
    case Platform::AMLOGIC:
    case Platform::AMLOGIC_950D4:
        GetGpuMemoryUsageAmlogic(); break;
    case Platform::BROADCOM:
        GetGpuMemoryUsageBroadcom(); break;
    case Platform::MEDIATEK:
        GetGpuMemoryUsageMediatek(); break;
    case Platform::REALTEK:
    case Platform::REALTEK64:
        GetGpuMemoryUsageRealtek(); break;
    default:
        LOG_WARN("GPU collection not implemented for this platform");
        break;
}

// BAD: If-else chain — easy to miss new platforms
if (mPlatform == Platform::AMLOGIC) { ... }
else if (mPlatform == Platform::BROADCOM) { ... }
// Silently skips MEDIATEK!
```

### Fixed-Width Integer Types
```cpp
// GOOD: Explicit sizes for /proc values
int64_t memTotalKb = 0;
uint64_t gpuUsedBytes = 0;

// BAD: int/long — sizes vary by platform
int memTotal;
long gpuUsed;
```

### Filesystem Paths
```cpp
// GOOD: Use std::filesystem for path construction
std::filesystem::path outputFile = outputDir / "report.html";

// BAD: String concatenation
std::string outputFile = outputDir + "/" + "report.html";  // no separator normalization
```

## Error Handling

### Return Value Convention
- Return early with a log message on /proc read failures; do not throw exceptions in metric code
- Use `std::optional<T>` to signal "value not available" without exceptions

```cpp
// GOOD: Graceful degradation for optional platform metrics
std::optional<long> ReadCmaBorrowed() {
    std::ifstream f("/proc/meminfo");
    if (!f.is_open()) return std::nullopt;
    // parse...
    return value;
}
```

### Logging
Use `Log.h` macros throughout — never `printf` or `std::cout` in metric code:
```cpp
// GOOD
LOG_INFO("Starting memory collection, platform=%d, freq=%lds", (int)mPlatform, frequency.count());
LOG_WARN("GPU sysfs node not found — skipping GPU metrics");
LOG_ERROR("Failed to open /proc/meminfo");

// BAD
printf("Starting collection\n");
std::cout << "Error reading /proc\n";
```

## Thread Safety and Concurrency

### IMetric Thread Contract
Each `IMetric` subclass owns exactly one collection thread:
```cpp
// GOOD: StartCollection spawns the thread; StopCollection joins it
void MemoryMetric::StartCollection(std::chrono::seconds frequency) {
    mQuit = false;
    mThread = std::thread(&MemoryMetric::CollectData, this, frequency);
}

void MemoryMetric::StopCollection() {
    mCv.notify_one();  // wake the sleeping collection loop
    if (mThread.joinable()) mThread.join();
}
```

### ConditionVariable Usage
```cpp
// GOOD: Use ConditionVariable::wait_for() to sleep between samples
// It wakes early if mQuit is set, enabling clean shutdown
mCv.wait_for(lock, frequency, [this] { return mQuit.load(); });
```

### JsonReportGenerator Concurrency
`JsonReportGenerator` is not internally thread-safe. `SaveResults()` is called
**after** `StopCollection()` for each metric — from the main thread only.
Do not call `SaveResults()` concurrently from multiple metric threads.
```

### Thread Safety Documentation

Always document thread safety expectations:

```c
// GOOD: Clear thread safety documentation

/**
 * Collect and record a memory metric sample
 * @param measurement Measurement accumulator to update
 * @return 0 on success, negative on error
 * 
 * Thread Safety: Called only from the owning metric's collection
 *                thread. Not safe to call concurrently.
 */
int collect_memory_sample(Measurement& measurement) {
    // Updates measurement min/max/mean
}

/**
 * Initialize event processor
 * @return 0 on success, negative on error
 * 
 * Thread Safety: NOT thread-safe. Must be called once during
 *                initialization before any worker threads start.
 */
int init_event_processor(void) {
    // No locking - initialization only
}

/**
 * Get current statistics
 * @param stats Output buffer for statistics
 * 
 * Thread Safety: Caller must hold stats_lock before calling.
 *                Use get_stats_safe() for automatic locking.
 */
void get_stats_unlocked(stats_t* stats) {
    // Assumes caller holds lock
}
```

### Memory Fragmentation Prevention

Configure thread pools to prevent fragmentation:

```c
// GOOD: Thread pool with pre-allocated threads
#define THREAD_POOL_SIZE 4
#define WORK_QUEUE_SIZE 256

typedef struct {
    pthread_t threads[THREAD_POOL_SIZE];
    pthread_attr_t thread_attr;
    // ... work queue ...
} thread_pool_t;

int init_thread_pool(thread_pool_t* pool) {
    // Configure thread attributes once
    pthread_attr_init(&pool->thread_attr);
    pthread_attr_setstacksize(&pool->thread_attr, THREAD_STACK_SIZE);
    pthread_attr_setdetachstate(&pool->thread_attr, PTHREAD_CREATE_JOINABLE);
    
    // Create fixed number of threads (no dynamic allocation)
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        int ret = pthread_create(&pool->threads[i], &pool->thread_attr,
                                 worker_thread, pool);
        if (ret != 0) {
            // Cleanup already created threads
            cleanup_partial_pool(pool, i);
            return -1;
        }
    }
    
    return 0;
}

// BAD: Creating threads dynamically (causes fragmentation)
void bad_handle_request(request_t* req) {
    pthread_t thread;
    pthread_create(&thread, NULL, handle_one_request, req);
    pthread_detach(thread);  // New thread for each request!
}
```

### Testing Thread Safety

```c
// GOOD: Test for race conditions
#include <gtest/gtest.h>

TEST(ThreadSafety, ConcurrentIncrement) {
    thread_safe_counter_t counter = {0};
    init_counter(&counter);
    
    const int NUM_THREADS = 10;
    const int INCREMENTS_PER_THREAD = 1000;
    pthread_t threads[NUM_THREADS];
    
    // Create multiple threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, 
                      increment_n_times, &counter);
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verify no race conditions
    EXPECT_EQ(counter.counter, NUM_THREADS * INCREMENTS_PER_THREAD);
    
    cleanup_counter(&counter);
}
```

### Static Analysis for Concurrency

```bash
# Use thread sanitizer to detect race conditions
gcc -g -fsanitize=thread source.c -o program
./program

# Use helgrind (valgrind) to detect synchronization issues
valgrind --tool=helgrind ./program

# Check for deadlocks
valgrind --tool=helgrind --track-lockorders=yes ./program
```

## Code Style

### Naming Conventions
- Functions: `camelCase` methods, `snake_case` free functions (e.g., `StartCollection`, `get_platform_name`)
- Types: `PascalCase` classes (e.g., `MemoryMetric`, `JsonReportGenerator`)
- Macros/Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_BUFFER_SIZE`)
- Global variables: `g_` prefix (avoid when possible)
- Static variables: `s_` prefix

### File Organization
- One .c file per module
- Corresponding .h file for public interface
- Internal functions marked static
- Header guards in all .h files

```cpp
// GOOD: header guard
#ifndef MEMORY_METRIC_H
#define MEMORY_METRIC_H

// ... declarations ...

#endif /* MEMORY_METRIC_H */
```

## Testing Requirements

### Unit Tests
- Test all public functions
- Test error paths and edge cases
- Use mocks for external dependencies
- Verify resource cleanup (no leaks)
- Run tests under valgrind

### Memory Testing
```bash
# Run with memory checking
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./test_binary

# Static analysis
cppcheck --enable=all --inconclusive source/
```

## Anti-Patterns to Avoid

```c
// BAD: Magic numbers
if (size > 1024) { ... }

// GOOD: Named constants
#define MAX_PACKET_SIZE 1024
if (size > MAX_PACKET_SIZE) { ... }

// BAD: Unchecked allocation
char* buf = malloc(size);
strcpy(buf, input);

// GOOD: Checked with cleanup
char* buf = malloc(size);
if (!buf) return ERR_NO_MEMORY;
strncpy(buf, input, size - 1);
buf[size - 1] = '\0';

// BAD: Memory leak in error path
FILE* f = fopen(path, "r");
if (condition) return -1;  // Leaked f
fclose(f);

// GOOD: Cleanup on all paths
FILE* f = fopen(path, "r");
if (!f) return -1;
if (condition) {
    fclose(f);
    return -1;
}
fclose(f);
return 0;
```

## References

- Project targets C++17 with RDK device deployment
- See `IMetric.h` for the public metric interface
- Review `MemoryMetric.cpp` and `ProcessMetric.cpp` for implementation patterns
- Check `FileParsers/` for `/proc` file parsing examples
