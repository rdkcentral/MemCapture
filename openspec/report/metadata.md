# Metadata Injection

## Facts

- `Metadata` object is created in `main` and passed to `JsonReportGenerator` (`main.cpp:230`, `main.cpp:231`).
- Capture duration is set from elapsed steady-clock seconds before save (`main.cpp:264` to `main.cpp:266`).
- `JsonReportGenerator::getJson()` injects metadata fields: `image`, `platform`, `mac`, `timestamp`, `duration`, `swapEnabled` (`JsonReportGenerator.cpp:95` through `JsonReportGenerator.cpp:101`).

## Metadata Field Sources

### Facts - Field Sources

- `Platform()` reads `FRIENDLY_ID` from `/etc/device.properties` (`Metadata.cpp:39`).
- `Image()` reads `imagename:` from `/version.txt` (`Metadata.cpp:64`).
- `Mac()` reads `/sys/class/net/eth0/address` (`Metadata.cpp:83`).
- `ReportTimestamp()` uses local wall-clock format `%FT%T%z` (`Metadata.cpp:99`).
- `SwapEnabled()` asks `Procrank::swapTotalKb() > 0` (`Metadata.cpp:113`).

## Inferences

- Metadata can degrade to `Unknown` on development hosts, chroot builds, or images lacking expected files.

## Unknowns

- Interface name assumption `eth0` may not hold on all device variants.
