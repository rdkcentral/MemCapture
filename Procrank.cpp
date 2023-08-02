/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Stephen Foulds
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "Procrank.h"
#include "FileParsers/MemInfo.h"
#include "FileParsers/Smaps.h"

#include <climits>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <inttypes.h>
#include <set>

Procrank::Procrank() : mSwapEnabled(swapTotalKb() > 0), mZramCompressionRatio(zramCompressionRatio())
{

}

Procrank::~Procrank()
{

}

/**
 * Get the memory usage for all the processes currently running
 * @return
 */
std::vector<Procrank::ProcessMemoryUsage> Procrank::GetMemoryUsage() const
{
    // Get running processes
    std::set<pid_t> pids = getRunningProcesses();

    if (pids.empty()) {
        LOG_WARN("No PIDs found");
        return {};
    }

    // Get the memory usage for each PID
    std::vector<Procrank::ProcessMemoryUsage> memoryUsage;
    for (auto &&pid: pids) {
        Process process(pid);
        if (process.name().empty()) {
            continue;
        }

        auto usage = getProcessMemoryUsage(process);
        memoryUsage.emplace_back(usage);
    }

    return memoryUsage;
}

long Procrank::swapTotalKb()
{
    MemInfo memInfo;
    return memInfo.SwapTotal();
}

/**
 * Work out the current zram compression ratio
 * @return
 */
double Procrank::zramCompressionRatio()
{
    if (!mSwapEnabled) {
        return 0;
    }

    // First, work out how much ZRAM memory we have
    char buffer[PATH_MAX];
    uint64_t zramTotal = 0;

    constexpr uint32_t maxZramDevices = 256;
    for (uint32_t i = 0; i < maxZramDevices; i++) {
        snprintf(buffer, PATH_MAX, "/sys/block/zram%u", i);
        if (!std::filesystem::exists(buffer)) {
            // We assume zram devices appear in range 0-255 and appear always in sequence
            // under /sys/block. So, stop looking for them once we find one is missing.
            break;
        }

        std::filesystem::path mmstat(buffer);
        mmstat /= "mm_stat";

        uint64_t deviceMemoryTotal = 0;

        if (std::filesystem::exists(mmstat)) {
            std::ifstream mmstatFile(mmstat);
            if (mmstatFile) {
                std::string line;
                std::getline(mmstatFile, line);
                if (sscanf(line.c_str(), "%*u %*u %" SCNu64, &deviceMemoryTotal) != 1) {
                    LOG_ERROR("Malformed mm_stat file %s", mmstat.string().c_str());
                }
                zramTotal += deviceMemoryTotal;
            }
        }
    }

    if (zramTotal == 0) {
        return 0;
    }

    uint64_t zramTotalKb = zramTotal / 1024;

    // Now work out the compression ratio
    MemInfo systemMemInfo;
    double compression = static_cast<double>(zramTotalKb) / systemMemInfo.SwapUsed();

    LOG_DEBUG("Zram compression is %f", compression);
    return compression;
}

/**
 * Return the pids of all the currently running processes
 * @return
 */
std::set<pid_t> Procrank::getRunningProcesses() const
{
    std::set<pid_t> pids;
    std::filesystem::directory_iterator procDir("/proc");

    std::string directoryName;
    pid_t pid;
    for (const auto &entry: procDir) {
        if (entry.is_directory()) {
            // Get the PID from dir name
            directoryName = entry.path().filename().string();

            if (parseInt(directoryName, &pid)) {
                pids.insert(static_cast<pid_t>(pid));
            }
        }
    }

    return pids;
}


/**
 * Get the memory usage of a given process
 * @param process
 * @return
 */
Procrank::ProcessMemoryUsage Procrank::getProcessMemoryUsage(Process &process) const
{
    ProcessMemoryUsage memoryUsage(process);

    Smaps smapFile(memoryUsage.process.pid());
    memoryUsage.pss = smapFile.Pss();
    memoryUsage.rss = smapFile.Rss();
    memoryUsage.swap = smapFile.Swap();
    memoryUsage.swap_pss = smapFile.SwapPss();
    memoryUsage.locked = smapFile.Locked();
    memoryUsage.vss = smapFile.Vss();
    memoryUsage.uss = smapFile.Uss();
    memoryUsage.swap_zram = smapFile.SwapPss() * mZramCompressionRatio;

    return memoryUsage;
}
