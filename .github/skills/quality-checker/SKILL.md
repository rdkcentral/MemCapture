---
name: quality-checker
description: Run comprehensive quality checks (static analysis, memory safety, thread safety, build verification) on the MemCapture codebase. Use when validating code changes or debugging before committing.
---

# MemCapture Quality Checker

## Purpose

Execute comprehensive quality checks on the MemCapture codebase directly on the developer's machine
or Linux CI environment. Ensures the code builds cleanly, collects metrics correctly, and is free
from memory and thread safety issues.

## Usage

Invoke this skill when:
- Validating changes before committing
- Debugging build or runtime failures
- Running quality checks locally
- Verifying memory safety of new metric code
- Checking thread safety of collection loops
- Performing static analysis on new platform support

You can run all checks or select specific ones based on your needs.

## What It Does

This skill runs quality checks using tools available on the local Linux development environment:
- **CMake + g++**: Build with strict warnings
- **cppcheck**: C++17 static analysis
- **AddressSanitizer**: Memory safety (built into g++)
- **ThreadSanitizer**: Thread safety (built into g++)
- **Valgrind**: Memory leak detection

No Docker container is required. MemCapture is a self-contained CMake project.

## Available Checks

### 1. Static Analysis (cppcheck)
- **cppcheck**: Comprehensive C++17 static analyzer
- **Output**: Summary of errors and warnings per file

### 2. Memory Safety (AddressSanitizer)
- **Heap use-after-free**: Catches dangling smart pointer issues
- **Heap buffer overflow**: Catches out-of-bounds map/vector access
- **Stack use-after-scope**: Catches dangling references
- **Memory leaks**: Detects unreleased allocations
- **Output**: Runtime error report with stack trace

### 3. Thread Safety (ThreadSanitizer)
- **Data race detection**: Finds unsynchronized access to shared data
- Especially useful for `mQuit` flag and `mLinuxMemoryMeasurements` map
- **Output**: Runtime race report with access history

### 4. Build Verification
- **Strict compilation**: Builds with `-Wall -Wextra`
- **C++17 conformance**: No extensions
- **Output**: Build log and binary size

### 5. Integration Validation
- **Test capture**: 10-second run on local system
- **Report validation**: HTML and JSON output well-formed
- **Schema check**: All expected JSON top-level keys present

## Execution Process

### Step 1: Build (strict warnings)

```bash
mkdir -p build_quality && cd build_quality
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra" \
      -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
      ..
cmake --build . --parallel $(nproc) 2>&1 | tee build.log
```

### Step 2: Static Analysis

```bash
cppcheck --enable=all \
         --std=c++17 \
         --suppress=missingInclude \
         --suppress=unmatchedSuppression \
         --error-exitcode=0 \
         --xml --xml-version=2 \
         *.cpp *.h FileParsers/ 2> cppcheck-report.xml

# Print human-readable summary
cppcheck --enable=all --std=c++17 --suppress=missingInclude *.cpp *.h FileParsers/
```

### Step 3: Memory Safety (AddressSanitizer)

```bash
mkdir -p build_asan && cd build_asan
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
      -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
      ..
cmake --build . --parallel $(nproc)

ASAN_OPTIONS=detect_leaks=1 \
./MemCapture --platform AMLOGIC --duration 10 --json \
             --output-dir /tmp/asan_test/ 2>&1 | tee asan.log
```

### Step 4: Thread Safety (ThreadSanitizer)

```bash
mkdir -p build_tsan && cd build_tsan
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
      -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
      ..
cmake --build . --parallel $(nproc)

./MemCapture --platform AMLOGIC --duration 10 \
             --output-dir /tmp/tsan_test/ 2>&1 | tee tsan.log
```

### Step 5: Memory Leak (Valgrind)

```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --xml=yes \
         --xml-file=valgrind-report.xml \
         ./MemCapture --platform AMLOGIC --duration 5 \
                      --output-dir /tmp/valgrind_test/ 2>&1 | tee valgrind.log
```

### Step 6: Integration Validation

```bash
./MemCapture --platform AMLOGIC --duration 10 --json \
             --output-dir /tmp/integration_test/

# Verify outputs exist
ls -la /tmp/integration_test/report.html /tmp/integration_test/results.json

# Verify JSON schema
python3 -c "
import json, sys
with open('/tmp/integration_test/results.json') as f:
    d = json.load(f)
expected = ['metadata', 'memory', 'processes']
missing = [k for k in expected if k not in d]
print('MISSING keys:', missing) if missing else print('JSON schema OK')
"
```

## Interpreting Results

### Static Analysis (cppcheck)
- **error**: Critical issues that must be fixed
- **warning**: Potential problems to review
- **style**: Code style improvements
- **performance**: Missed optimization opportunities

### Memory Safety (AddressSanitizer)
- `heap-use-after-free`: Dangling pointer or reference — critical
- `heap-buffer-overflow`: Out-of-bounds access — critical
- `Direct leak`: Memory not freed — fix immediately
- `Indirect leak`: Typically from a lost parent structure

### Thread Safety (ThreadSanitizer)
- `DATA RACE`: Two threads access the same variable without synchronization
- Common in MemCapture: `mQuit` without `std::atomic`, map access without lock
- `Lock order inversion`: Potential deadlock in metric shutdown

### Build Verification
- All warnings listed: Review every `-Wextra` warning
- Binary size: Note if a new dependency significantly increases binary size

## User Interaction

When invoked, ask the user:

1. **Which checks to run?**
   - All checks (comprehensive)
   - Static analysis only (fast)
   - Memory safety only (AddressSanitizer or Valgrind)
   - Thread safety only (ThreadSanitizer)
   - Build verification only
   - Integration validation only

2. **Platform to test:**
   - AMLOGIC (default)
   - REALTEK, BROADCOM, MEDIATEK

3. **Report detail:**
   - Summary only (counts and critical issues)
   - Detailed (all findings)

## Example Invocations

- "Run quality checks" — all checks, AMLOGIC platform
- "Check memory safety" — AddressSanitizer + Valgrind
- "Quick static analysis" — cppcheck only
- "Verify my new Mediatek GPU code" — build + MEDIATEK integration run
- "Check thread safety of the collection loop" — ThreadSanitizer run

## Output Files Generated

- `build.log`: Compiler warnings and errors
- `cppcheck-report.xml`: Static analysis findings
- `asan.log`: AddressSanitizer runtime output
- `tsan.log`: ThreadSanitizer runtime output
- `valgrind-report.xml` + `valgrind.log`: Memory leak report

## Best Practices

1. **Run static analysis first** — fastest feedback loop
2. **Run AddressSanitizer on every new metric** — catches issues that Valgrind may miss
3. **Run ThreadSanitizer when changing collection thread logic** — race conditions are subtle
4. **Validate JSON schema after any JsonReportGenerator change** — downstream consumers depend on it
5. **Always test with the correct platform flag** — platform switch paths are not equivalent

Pull the latest test container:
```bash
docker pull ghcr.io/rdkcentral/docker-device-mgt-service-test/native-platform:latest
```

Start container with workspace mounted:
```bash
docker run -d --name native-platform \
  -v /path/to/workspace:/mnt/workspace \
  ghcr.io/rdkcentral/docker-device-mgt-service-test/native-platform:latest
```

### Step 2: Run Selected Checks

Execute the requested quality checks inside the container:

**Static Analysis:**
```bash
docker exec -i native-platform /bin/bash -c "
  cd /mnt/workspace && \
  cppcheck --enable=all \
           --inconclusive \
           --suppress=missingIncludeSystem \
           --suppress=unmatchedSuppression \
           --error-exitcode=0 \
           --xml \
           --xml-version=2 \
           . 2> cppcheck-report.xml
"
```

**Shell Script Checks:**
```bash
docker exec -i native-platform /bin/bash -c "
  cd /mnt/workspace && \
  find . -name '*.sh' -type f -exec shellcheck {} +
"
```

**Memory Safety:**
```bash
docker exec -i native-platform /bin/bash -c "
   cd /mnt/workspace/src/unittest && \
   automake --add-missing && \
   autoreconf --install && \
   ./configure && \
   make -j\$(nproc) && \
   find . -type f -executable -name '*gtest*' 2>/dev/null | while read test_bin; do
    valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --xml=yes \
             --xml-file=\"valgrind-\$(basename \$test_bin).xml\" \
             \"\$test_bin\" 2>&1 | tee \"valgrind-\$(basename \$test_bin).log\"
  done
"
```

**Thread Safety:**
```bash
docker exec -i native-platform /bin/bash -c "
   cd /mnt/workspace/src/unittest && \
   find . -type f -executable -name '*gtest*' 2>/dev/null | while read test_bin; do
    valgrind --tool=helgrind \
             --track-lockorders=yes \
             --xml=yes \
             --xml-file=\"helgrind-\$(basename \$test_bin).xml\" \
             \"\$test_bin\" 2>&1 | tee \"helgrind-\$(basename \$test_bin).log\"
  done
"
```

**Build Verification:**
```bash
docker exec -i native-platform /bin/bash -c "
   cd /path/to/build && \
   cmake --build . --parallel $(nproc) && \
   for bin in MemCapture; do
      if [ -f \"\$bin\" ]; then
         ls -lh \"\$bin\"
         file \"\$bin\"
         size \"\$bin\"
      fi
   done
"
```

### Step 3: Report Results

Parse and summarize results for the user:
- Number of issues found by category
- Critical issues requiring immediate attention
- Warnings that should be addressed
- Memory leaks with stack traces
- Race conditions or deadlock risks
- Build errors or warnings

### Step 4: Cleanup

Stop and remove the container:
```bash
docker stop native-platform
docker rm native-platform
```

## Interpreting Results

### Static Analysis (cppcheck)
- **error**: Critical issues that must be fixed
- **warning**: Potential problems to review
- **style**: Code style improvements
- **performance**: Optimization opportunities

### Memory Safety (Valgrind)
- **definitely lost**: Memory leaks requiring fixes
- **indirectly lost**: Leaks from lost parent structures
- **possibly lost**: Potential leaks to investigate
- **still reachable**: Memory held at exit (usually OK)
- **Invalid read/write**: Buffer overflow (CRITICAL)
- **Use of uninitialized value**: Must initialize before use

### Thread Safety (ThreadSanitizer / Helgrind)
- **Possible data race**: Unsynchronized access to shared data — `mQuit`, `mLinuxMemoryMeasurements`
- **Lock order violation**: Potential deadlock in metric shutdown sequence
- **Thread still running**: Collection thread not joined before destructor

### Build Verification
- **Compilation errors**: Must fix before proceeding
- **Warnings** (`-Wall -Wextra`): Review every warning, fix all in new code
- **Binary size**: Monitor if new dependency significantly increases the binary

## User Interaction

When invoked, ask the user:

1. **Which checks to run?**
   - All checks (comprehensive)
   - Static analysis only (fast)
   - Memory safety only (AddressSanitizer or Valgrind)
   - Thread safety only (ThreadSanitizer)
   - Build verification only
   - Integration validation only

2. **Platform to test:**
   - AMLOGIC (default)
   - REALTEK, BROADCOM, MEDIATEK

3. **Report detail:**
   - Summary only (counts and critical issues)
   - Detailed (all findings)
