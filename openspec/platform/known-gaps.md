# Known Gaps, Assumptions, and Manual Validation Needs

## Facts (directly observed)

- README runtime section lists only three platforms (`AMLOGIC`, `REALTEK`, `BROADCOM`), while code supports six (`AMLOGIC_950D4`, `REALTEK64`, `MEDIATEK` additionally) (`README.md:40`, `main.cpp:116` to `main.cpp:128`).
- README says optional JSON filename `results.json`; code writes `report.json` (`README.md:80`, `main.cpp:330`).
- `MemInfo` exposes `CmaFree()` but parser never sets `mCmaFree`; only `CmaTotal` is parsed (`FileParsers/MemInfo.cpp:31` onward, `MemInfo.h`).
- `main` duration validation checks `< 0` while error text says must be `> 0`, allowing `0` duration captures (`main.cpp:108`).

## Inferences

- CMA borrowed-by-kernel calculation may be inaccurate because `CmaFree()` appears to stay default unless set elsewhere.
- Documentation drift is present between README and current implementation behavior.

## Assumptions to Validate

- GPU debugfs nodes and formats remain stable per platform across all deployed kernel branches.
- Cgroup controller layout (`memory`, `cpuacct`, `gpu`, `cpuset`, `pids`) exists and matches expected paths on all target images.
- CPU idle PRCTL ABI is available and compatible where build flag is enabled.

## Unknowns Requiring Manual Validation

- Yocto recipe and package naming in production build layers.
- Required runtime privileges/capabilities for reading debugfs/procfs nodes on locked-down images.
- Operational contract with telemetry pipeline (who invokes capture, where artifacts are uploaded, retention policy).
- Device matrix verification for all six platform enums and associated collector paths.

## Suggested Validation Checklist

1. Run one-shot captures on at least one device per enum platform and verify dataset presence.
2. Compare `Linux Memory` totals against `/proc/meminfo` and kernel tools on-device.
3. Validate `report.json` schema consumption by backend tooling.
4. Verify graceful early-termination path using SIGTERM during active capture.
5. Confirm CDN reachability or provide offline assets for HTML rendering in restricted environments.
