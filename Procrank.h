/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Sky UK
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

#pragma once

#include "Log.h"
#include "Measurement.h"
#include <utility>
#include <vector>
#include <string>
#include <set>
#include "Process.h"

/**
 * Originally memcapture integrated the Android Procrank library. This is now replaced with a custom implementation of procrank
 * to read the memory values from the smaps/smaps_rollups file for increased performance.
 *
 * Android does have a procrank v2 which does make use of smaps (https://android.googlesource.com/platform/system/memory/libmeminfo/+/refs/heads/main/tools/procrank.cpp)
 * but this is buggy and at the time of writing the smaps feature is disabled so doesn't result in any performance improvement
 *
 * So this procrank class is inspired by Android's procrank v2 but simplified for our needs. Out-performs procrank v1 by 3-4x in
 * quick and dirty testing
 */
class Procrank
{
public:

    struct ProcessMemoryUsage
    {
    public:
        explicit ProcessMemoryUsage(Process p) : process(std::move(p)),
                                                 vss(0),
                                                 rss(0),
                                                 pss(0),
                                                 uss(0),
                                                 locked(0),
                                                 swap(0),
                                                 swap_pss(0),
                                                 swap_zram(0)
        {
        }

        Process process;

        uint64_t vss;
        uint64_t rss;
        uint64_t pss;
        uint64_t uss;
        uint64_t locked;

        uint64_t swap;
        uint64_t swap_pss;

        // When using zram for a swap partition, swap data will be compressed so the amount of physical
        // memory used will be less than the amount of swap in use
        uint64_t swap_zram;
    };

public:
    Procrank();

    ~Procrank();

    std::vector<ProcessMemoryUsage> GetMemoryUsage() const;

    long swapTotalKb();

private:
    template<typename T>
    bool parseInt(std::string_view s, T *out,
                  T min = std::numeric_limits<T>::min(),
                  T max = std::numeric_limits<T>::max()) const
    {
        errno = 0;
        char *end;
        long long int result = strtoll(s.data(), &end, 10);

        if (errno != 0 || s == end || *end != '\0') {
            return false;
        }
        if (result < min || max < result) {
            return false;
        }

        *out = static_cast<T>(result);
        return true;
    }

    double zramCompressionRatio();

    [[nodiscard]] std::set<pid_t> getRunningProcesses() const;

    ProcessMemoryUsage getProcessMemoryUsage(Process &process) const;

private:
    bool mSwapEnabled;
    double mZramCompressionRatio;
};
