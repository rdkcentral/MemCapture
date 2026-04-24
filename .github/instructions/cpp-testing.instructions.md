---
applyTo: "test/**/*.cpp,test/**/*.h"
---

# C++ Testing Standards for MemCapture (Google Test)

## Test Framework

Use Google Test (gtest) and Google Mock (gmock) for all C++ test code targeting MemCapture components.

## Test Organization

### File Structure
- One test file per source component: `MemoryMetric.cpp` → `test/MemoryMetricTest.cpp`
- Mirror the source tree structure under `test/`
- Use test fixtures for components that require setup/teardown (e.g., temp directories, mock `/proc` files)

```cpp
// GOOD: Test file structure for MemCapture
// filepath: test/MemInfoTest.cpp

#include "FileParsers/MemInfo.h"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

class MemInfoTest : public ::testing::Test {
protected:
    std::filesystem::path tempDir;

    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "memcapture_test";
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    void WriteMockMemInfo(const std::string& content) {
        std::ofstream f(tempDir / "meminfo");
        f << content;
    }
};

TEST_F(MemInfoTest, ParsesTotalAndFree) {
    WriteMockMemInfo(
        "MemTotal:       8192000 kB\n"
        "MemFree:        2048000 kB\n"
        "MemAvailable:   4096000 kB\n"
    );
    // MemInfo reads /proc/meminfo — use a mock path override or
    // verify via integration with a known /proc/meminfo on test host
    MemInfo info;
    EXPECT_GT(info.Total(), 0L);
}
```

## Testing Patterns

### Mock /proc Files with Temp Directories
Since MemCapture reads from `/proc`, tests should write mock data to a temp directory
and inject the path via constructor or a test-only method:

```cpp
// GOOD: Inject mock path for testability
class MemInfo {
public:
    explicit MemInfo(std::filesystem::path memInfoPath = "/proc/meminfo");
    // ...
};

TEST(MemInfoTest, CmaValuesFromMockFile) {
    auto path = std::filesystem::temp_directory_path() / "mock_meminfo";
    std::ofstream f(path);
    f << "CmaTotal:         131072 kB\n"
      << "CmaFree:           65536 kB\n";
    f.close();

    MemInfo info(path);
    EXPECT_EQ(info.CmaTotal(), 131072L);
    EXPECT_EQ(info.CmaFree(), 65536L);
    std::filesystem::remove(path);
}
```

### Testing Measurement Accumulation
```cpp
TEST(MeasurementTest, AccumulatesMinMaxMean) {
    Measurement m("TestValue");
    m.AddSample(10);
    m.AddSample(20);
    m.AddSample(30);

    EXPECT_EQ(m.Min(), 10);
    EXPECT_EQ(m.Max(), 30);
    EXPECT_DOUBLE_EQ(m.Mean(), 20.0);
}

TEST(MeasurementTest, FirstSampleSetsMinAndMax) {
    Measurement m("TestValue");
    m.AddSample(42);
    EXPECT_EQ(m.Min(), 42);
    EXPECT_EQ(m.Max(), 42);
}
```

### Testing Group Manager
```cpp
TEST(GroupManagerTest, MatchesProcessByName) {
    nlohmann::json groupJson = R"({
        "processes": [
            { "group": "Logging", "processes": ["syslog-ng", "journald"] }
        ]
    })"_json;

    GroupManager mgr(groupJson);
    auto group = mgr.getGroup(GroupManager::groupType::PROCESS, "syslog-ng");
    ASSERT_TRUE(group.has_value());
    EXPECT_EQ(group.value(), "Logging");
}

TEST(GroupManagerTest, ReturnsNulloptForUnknownProcess) {
    nlohmann::json groupJson = R"({ "processes": [] })"_json;
    GroupManager mgr(groupJson);
    auto group = mgr.getGroup(GroupManager::groupType::PROCESS, "unknown");
    EXPECT_FALSE(group.has_value());
}
```

### Memory Safety Testing
- Use AddressSanitizer (`-fsanitize=address`) during test builds
- Verify no leaks by checking all `std::ifstream` objects are destroyed at test end
- Use RAII in test fixtures for temp files

```cpp
// GOOD: RAII temp file in test
class TempFile {
    std::filesystem::path path_;
public:
    explicit TempFile(const std::string& content) {
        path_ = std::filesystem::temp_directory_path()
                / ("test_" + std::to_string(std::rand()));
        std::ofstream f(path_);
        f << content;
    }
    ~TempFile() { std::filesystem::remove(path_); }
    const std::filesystem::path& path() const { return path_; }
};
```

## Test Quality Standards

### Coverage Requirements
- All `MemInfo` and `Smaps` parser methods must have tests with mock `/proc` data
- `Measurement` min/max/mean calculations must be tested for 0 samples, 1 sample, and N samples
- `GroupManager` must be tested for exact match, regex match, and no match
- `JsonReportGenerator` JSON schema must be validated against expected keys in at least one test

### Test Naming
```cpp
// Pattern: TEST(ComponentName, BehaviorBeingTested)
TEST(MemInfo, ParsesTotalMemory) { ... }
TEST(MemInfo, HandlesAbsentCmaFields) { ... }
TEST(Measurement, MinMaxMeanWithMultipleSamples) { ... }
TEST(GroupManager, MatchesContainerByRegex) { ... }
TEST(JsonReportGenerator, OutputContainsMetadataKey) { ... }
```

### Assertions
- Use `ASSERT_*` when the test cannot continue meaningfully after failure
- Use `EXPECT_*` when multiple independent checks are valuable
- Add failure context for non-obvious assertions

```cpp
ASSERT_TRUE(group.has_value()) << "Expected group match for process 'syslog-ng'";
EXPECT_EQ(info.CmaTotal(), 131072L) << "CmaTotal mismatch in mock meminfo";
```

## Building and Running Tests

### CMake Test Target
```cmake
enable_testing()
find_package(GTest REQUIRED)

add_executable(MemCaptureTests
    test/MemInfoTest.cpp
    test/MeasurementTest.cpp
    test/GroupManagerTest.cpp
)

target_link_libraries(MemCaptureTests
    GTest::GTest GTest::Main
    nlohmann_json::nlohmann_json
)

add_test(NAME MemCaptureTests COMMAND MemCaptureTests)
```

### Running with AddressSanitizer
```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
cmake --build . --target MemCaptureTests
./MemCaptureTests
```

### Test Output for CI
```bash
./MemCaptureTests --gtest_output=xml:test_results.xml
```
