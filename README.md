# MemCapture

Memory capture and analysis tool for RDK

## Build

### Dependencies
MemCapture requires the following 3rd party libraries:

* Nlohmann JSON
* Inja

For desktop builds, these can be installed using vcpkg. 

The incbin library is also included as a header in the `3rdparty` directory.

#### VCPKG Usage
First, install VCPKG: https://vcpkg.io/en/getting-started.

Then build the project using the below commands:
```shell
$ mkdir build && cd ./build
$ cmake -DCMAKE_BUILD_TYPE=Release  -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake ..
$ make -j$(nproc)
```

#### Yocto
For Yocto builds, ensure the nlohmann/json and inja libraries are added as recipe dependencies.

## Run

```
Usage: MemCapture <option(s)>
    Utility to capture memory statistics

    -h, --help          Print this help and exit
    -o, --output-dir    Directory to save results in
    -j, --json          Save data as JSON in addition to HTML report
    -d, --duration      Amount of time (in seconds) to capture data for. Default 30 seconds
    -p, --platform      Platform we're running on. Supported options = ['AMLOGIC', 'REALTEK', 'BROADCOM']. Defaults to Amlogic
    -g, --groups        Path to JSON file containing the group mappings (optional)
```

Example:

```shell
$ ./MemCapture --platform AMLOGIC --duration 30 --groups ./groups.json --output-dir /tmp/memcapture_results/
```

Averages are calculated over the specified duration.

### Process Grouping

To ease analysis, MemCapture supports grouping processes into categories. This is done by providing MemCapture with a
JSON file containing the groups and regexes defining which process(es) should belong to that group.

For example if the provided JSON file contained the below:

```json
{
  "processes": [
    {
      "group": "Logging",
      "processes": [
        "syslog-ng",
        "systemd-journald"
      ]
    }
  ]
}
```

The `syslog-ng` and `systemd-journald` processes would belong to the `Logging` group and show up in the process list
with that group name.

An example file (`groups.example.json`) is provided in the repo.

### Results

By default, MemCapture will produce a file called `report.html` in the selected output directory. If no directory is provided, it
will create a `MemCaptureReport` subdirectory in the current working directory to save the files.

This report can be opened in any web browser and contains a summary view of all the metrics collected.

If the `-j` argument is provided to MemCapture, then an additional `results.json` file will be created. This contains the
raw data from MemCapture and is designed for importing into a backend system for analysis/reporting.

### Notes

Tool currently supports three platforms - `AMLOGIC` (default), `REALTEK` and `BROADCOM`. Not all stats are available on
all platforms
