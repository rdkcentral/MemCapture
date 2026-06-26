## Implementation Guidelines

- **Project Goal:** Capture and analyse memory usage on RDK-based embedded devices (Amlogic, Realtek, Broadcom, Mediatek) and produce HTML/JSON reports.
- **Language:** C++17
- **Build System:** CMake 3.10+ with vcpkg (desktop) or Yocto recipes (device)
- **Key Dependencies:** nlohmann_json, pantor::inja, incbin (bundled), optional Breakpad
- **Constraints:** Runs on resource-constrained Linux devices (ARMv7/ARMv8); must not significantly impact the system it is measuring.

## Implementation Strategy

1. **Setup Development Environment**
    - Install vcpkg and run `cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake ..` for desktop builds.
    - For cross-compilation, use the target device's Yocto SDK sysroot; do not use vcpkg on device builds.

2. **Adding a New Metric**
    - Implement the `IMetric` interface: `StartCollection(std::chrono::seconds)`, `StopCollection()`, `SaveResults()`.
    - Own exactly one `std::thread` for collection; use `ConditionVariable` / `mQuit` flag for clean shutdown.
    - Accumulate samples in `Measurement` objects (min/max/mean across the capture duration).
    - Pass `std::shared_ptr<JsonReportGenerator>` in the constructor — do not access it from the collection thread.
    - Call `SaveResults()` only after `StopCollection()` returns, from the main thread.

3. **Adding a New Platform**
    - Add the new enum value to `Platform.h`.
    - Add the new string mapping in `main.cpp` (argument parsing).
    - Add a new `GetGpuMemoryUsage<PlatformName>()` method in `MemoryMetric`.
    - Add the new case to every `switch (mPlatform)` block in `MemoryMetric.cpp`.
    - Guard all platform-specific sysfs/proc paths with `std::filesystem::exists()` before reading.

4. **Code Review and Integration**
    - Ensure platform switch statements are exhaustive (all cases handled, or explicit default with log).
    - Verify `IMetric::StopCollection()` always joins the collection thread before returning.
    - Confirm JSON report keys match the existing schema — do not rename or remove existing keys.

5. **Documentation**
    - Update `README.md` usage section with new CLI flags or platform names.
    - Document new JSON keys in code comments on `JsonReportGenerator`.
    - Maintain `CHANGELOG.md` following the existing format.

6. **Testing**
    - Write unit tests for parser classes (`MemInfo`, `Smaps`) using mock `/proc` files.
    - Run a 30-second test capture on a real or simulated device and diff `report.json` against a baseline.
    - Build with `-fsanitize=address` and verify no AddressSanitizer errors.