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

#include "MemoryMetric.h"
#include "FileParsers/MemInfo.h"
#include <thread>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <cmath>

MemoryMetric::MemoryMetric(Platform platform, std::shared_ptr<JsonReportGenerator> reportGenerator)
        : mQuit(false),
          mCv(),
          mLinuxMemoryMeasurements{},
          mCmaFree("CmaFree"),
          mCmaBorrowed("CmaBorrowedKernel"),
          mMemoryBandwidth({0, 0, 0, 0}),
          mMemoryFragmentation{},
          mPlatform(platform),
          mReportGenerator(std::move(reportGenerator))
{

    // Some metrics are returned as a number of pages instead of bytes, so get page size to be able to calculate
    // human-readable values
    mPageSize = sysconf(_SC_PAGESIZE);

    // Create a map of CMA regions that converts the directories in /sys/kernel/debug/cma/ to a human-readable name
    // based on the kernel DTS file
    // *** This will likely need updating for your particular device ***
    if (platform == Platform::AMLOGIC) {
        mCmaNames = {
                std::make_pair("cma-0", "secmon_reserved"),
                std::make_pair("cma-1", "logo_reserved"),
                std::make_pair("cma-2", "codec_mm_cma"),
                std::make_pair("cma-3", "ion_cma_reserved"),
                std::make_pair("cma-4", "vdin1_cma_reserved"),
                std::make_pair("cma-5", "demod_cma_reserved"),
                std::make_pair("cma-6", "kernel_reserved")
        };
    } else if (platform == Platform::REALTEK) {
        mCmaNames = {
                std::make_pair("cma-0", "cma-0"),
                std::make_pair("cma-1", "cma-1"),
                std::make_pair("cma-2", "cma-2"),
                std::make_pair("cma-3", "cma-3"),
                std::make_pair("cma-4", "cma-4"),
                std::make_pair("cma-5", "cma-5"),
                std::make_pair("cma-6", "cma-6"),
                std::make_pair("cma-7", "cma-7"),
                std::make_pair("cma-8", "cma-8"),
        };
    } else if (platform == Platform::BROADCOM) {
        mCmaNames = {
                std::make_pair("cma-WiFi@4C0000", "cma-WiFi@4C0000"),
                std::make_pair("cma-reserved", "cma-reserved")
        };
    }

    // Create static measurements for linux memory usage - store in KB
    // TODO:: This is gross, make tider
    Measurement total("Total");
    mLinuxMemoryMeasurements.insert(std::make_pair(total.GetName(), total));

    Measurement used("Used");
    mLinuxMemoryMeasurements.insert(std::make_pair(used.GetName(), used));

    Measurement buffered("Buffered");
    mLinuxMemoryMeasurements.insert(std::make_pair(buffered.GetName(), buffered));

    Measurement cached("Cached");
    mLinuxMemoryMeasurements.insert(std::make_pair(cached.GetName(), cached));

    Measurement free("Free");
    mLinuxMemoryMeasurements.insert(std::make_pair(free.GetName(), free));

    Measurement available("Available");
    mLinuxMemoryMeasurements.insert(std::make_pair(available.GetName(), available));

    Measurement slabTotal("Slab Total");
    mLinuxMemoryMeasurements.insert(std::make_pair(slabTotal.GetName(), slabTotal));

    Measurement slabReclaimable("Slab Reclaimable");
    mLinuxMemoryMeasurements.insert(std::make_pair(slabReclaimable.GetName(), slabReclaimable));

    Measurement slabUnreclaimable("Slab Unreclaimable");
    mLinuxMemoryMeasurements.insert(
            std::make_pair(slabUnreclaimable.GetName(), slabUnreclaimable));

    Measurement swapUsed("Swap Used");
    mLinuxMemoryMeasurements.insert(
            std::make_pair(swapUsed.GetName(), swapUsed));

    switch (platform) {
        case Platform::AMLOGIC: {
            // Amlogic allows reporting memory bandwidth
            mMemoryBandwidthSupported = true;
            // Enable memory bandwidth monitoring
            std::ofstream ddrMode("/sys/class/aml_ddr/mode", std::ios::binary);
            ddrMode << "1";

            // Amlogic reports GPU memory allocations
            mGPUMemorySupported = true;
            break;
        }

        case Platform::REALTEK:
            // Realtek does not report memory bandwidth
            mMemoryBandwidthSupported = false;
            // Realtek reports GPU memory allocations
            mGPUMemorySupported = true;
            break;

        case Platform::BROADCOM:
            // Line of enquiry open with Broadcom as to whether there is a way to get this info.
            // TODO: Complete investigation.
            mMemoryBandwidthSupported = false;
            mGPUMemorySupported = true;
            break;
    }

}

MemoryMetric::~MemoryMetric()
{
    if (!mQuit) {
        StopCollection();
    }

    // Disable memory bandwidth monitoring
    std::ofstream ddrMode("/sys/class/aml_ddr/mode", std::ios::binary);
    ddrMode << "0";
}

void MemoryMetric::StartCollection(const std::chrono::seconds frequency)
{
    mQuit = false;
    mCollectionThread = std::thread(&MemoryMetric::CollectData, this, frequency);
}

void MemoryMetric::StopCollection()
{
    std::unique_lock<std::mutex> locker(mLock);
    mQuit = true;
    mCv.notify_all();
    locker.unlock();

    if (mCollectionThread.joinable()) {
        LOG_INFO("Waiting for MemoryMetric collection thread to terminate");
        mCollectionThread.join();
    }
}

void MemoryMetric::CollectData(std::chrono::seconds frequency)
{
    std::unique_lock<std::mutex> lock(mLock);

    do {
        auto start = std::chrono::high_resolution_clock::now();

        GetLinuxMemoryUsage();
        GetCmaMemoryUsage();
        GetGpuMemoryUsage();
        GetContainerMemoryUsage();
        GetMemoryBandwidth();
        CalculateFragmentation();

        if (mPlatform == Platform::BROADCOM) {
            GetBroadcomBmemUsage();
        }

        auto end = std::chrono::high_resolution_clock::now();
        LOG_INFO("MemoryMetric completed in %lld ms",
                 (long long) std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

        // Wait for period before doing collection again, or until cancelled
        mCv.wait_for(lock, frequency);
    } while (!mQuit);

    LOG_INFO("Collection thread quit");
}

void MemoryMetric::SaveResults()
{
    std::vector<std::string> columns{};
    std::vector<JsonReportGenerator::row> rows{};

    // *** Linux Memory Usage ***
    columns = {"Value", "Min (KB)", "Max (KB)", "Average (KB)"};

    for (const auto &result: mLinuxMemoryMeasurements) {
        JsonReportGenerator::row row = {
                result.first,
                result.second
        };
        rows.emplace_back(row);
    }
    mReportGenerator->addDataset("Linux Memory", columns, rows);

    // Set the average Used memory value
    auto it = mLinuxMemoryMeasurements.find("Used");
    if (it != mLinuxMemoryMeasurements.end()) {
        mReportGenerator->setAverageLinuxMemoryUsage(it->second.GetAverageRounded());
    }
    rows.clear();

    // *** GPU Memory Usage ***
    if (mGPUMemorySupported) {
        columns = {"PID", "Process", "Container", "Cmdline", "Min (KB)", "Max (KB)", "Average (KB)"};

        for (const auto &result: mGpuMeasurements) {
            JsonReportGenerator::row row = {
                    std::to_string(result.first),
                    result.second.ProcessInfo.name(),
                    result.second.ProcessInfo.container().has_value() ? result.second.ProcessInfo.container().value()
                                                                      : "-",
                    result.second.ProcessInfo.cmdline(),
                    result.second.Used
            };
            rows.emplace_back(row);
        }
        mReportGenerator->addDataset("GPU Memory", columns, rows);

        // Add all GPU memory to accumulated total
        long double gpuSum = 0;
        std::for_each(mGpuMeasurements.begin(), mGpuMeasurements.end(), [&](const std::pair<pid_t, gpuMeasurement> &m)
        {
            gpuSum += m.second.Used.GetAverage();
        });
        mReportGenerator->addToAccumulatedMemoryUsage(gpuSum);

        rows.clear();
    }

    // *** CMA Memory Usage and breakdown ***
    columns = {"Region", "Size_KB", "Used Min (KB)", "Used Max (KB)", "Used Average (KB)", "Unused Min (KB)",
               "Unused Max (KB)", "Unused Average (KB)"};
    for (const auto &result: mCmaMeasurements) {
        JsonReportGenerator::row row = {
                result.first,
                std::to_string(result.second.sizeKb),
                result.second.Used,
                result.second.Unused
        };
        rows.emplace_back(row);
    }
    mReportGenerator->addDataset("CMA Regions", columns, rows);

    // Add all CMA memory to accumulated total
    long double cmaSum = 0;
    std::for_each(mCmaMeasurements.begin(), mCmaMeasurements.end(), [&](const std::pair<std::string, cmaMeasurement> &m)
    {
        cmaSum += m.second.Used.GetAverage();
    });
    mReportGenerator->addToAccumulatedMemoryUsage(cmaSum);

    rows.clear();


    // *** CMA Summary ***
    columns = {"", "Min_KB", "Max_KB", "Average_KB"};

    rows.emplace_back(
            JsonReportGenerator::row{
                    "CMA Free",
                    mCmaFree,
            }
    );

    rows.emplace_back(
            JsonReportGenerator::row{
                    "CMA Borrowed by Kernel",
                    mCmaBorrowed
            }
    );
    mReportGenerator->addDataset("CMA Summary", columns, rows);
    rows.clear();

    // *** Per-container memory usage ***
    columns = {"Container", "Used_Min_KB", "Used_Max_KB", "Used_Average_KB"};

    for (const auto &result: mContainerMeasurements) {
        rows.emplace_back(JsonReportGenerator::row{
                result.first,
                result.second
        });
    }
    mReportGenerator->addDataset("Containers", columns, rows);
    rows.clear();


    // *** Memory bandwidth (if supported) ***
    if (mMemoryBandwidthSupported) {
        columns = {"", "Bandwidth_KB/s", "Usage_%"};

        rows.emplace_back(JsonReportGenerator::row{"Max",
                                                   std::to_string(mMemoryBandwidth.maxKBps),
                                                   std::to_string(mMemoryBandwidth.maxUsagePercent)});

        rows.emplace_back(JsonReportGenerator::row{"Average",
                                                   std::to_string(mMemoryBandwidth.averageKBps),
                                                   std::to_string(mMemoryBandwidth.averageUsagePercent)});

        mReportGenerator->addDataset("Memory Bandwidth", columns, rows);
        rows.clear();
    }

    // *** Memory fragmentation - break down per zone ***
    for (const auto &memoryZone: mMemoryFragmentation) {
        std::string reportName = "Memory Fragmentation - Zone " + memoryZone.first;
        columns = {"Order", "Min_Free_Pages", "Max_Free_Pages", "Average_Free_Pages", "Min_Fragmentation_%",
                   "Max_Fragmentation_%", "Average_Fragmentation_%"};

        int i = 0;
        for (const auto &measurement: memoryZone.second) {
            rows.emplace_back(JsonReportGenerator::row{
                    std::to_string(i),
                    measurement.FreePages,
                    measurement.Fragmentation
            });
            i++;
        }
        mReportGenerator->addDataset(reportName, columns, rows);
        rows.clear();
    }

    // *** Broadcom BMEM (if applicable) ***
    if (mPlatform == Platform::BROADCOM) {
        columns = {"Region", "Min_Usage_KB", "Max_Usage_KB", "Average_Usage_KB"};

        for (const auto &measurement: mBroadcomBmemMeasurements) {
            rows.emplace_back(JsonReportGenerator::row{
                    measurement.GetName(),
                    measurement});
        }
        mReportGenerator->addDataset("BMEM", columns, rows);

        // Add all BMEM memory to accumulated total
        long double bmemSum = 0;
        std::for_each(mBroadcomBmemMeasurements.begin(), mBroadcomBmemMeasurements.end(), [&](const Measurement &m)
        {
            bmemSum += m.GetAverage();
        });
        mReportGenerator->addToAccumulatedMemoryUsage(bmemSum);
    }
}

void MemoryMetric::GetLinuxMemoryUsage()
{
    //LOG_INFO("Getting memory usage");

    MemInfo memInfoFile;
    mLinuxMemoryMeasurements.at("Total").AddDataPoint(memInfoFile.MemTotalKb());
    mLinuxMemoryMeasurements.at("Used").AddDataPoint(memInfoFile.MemUsedKb());
    mLinuxMemoryMeasurements.at("Buffered").AddDataPoint(memInfoFile.BuffersKb());
    mLinuxMemoryMeasurements.at("Cached").AddDataPoint(memInfoFile.CachedKb());
    mLinuxMemoryMeasurements.at("Free").AddDataPoint(memInfoFile.MemFreeKb());
    mLinuxMemoryMeasurements.at("Available").AddDataPoint(memInfoFile.MemAvailableKb());
    mLinuxMemoryMeasurements.at("Slab Total").AddDataPoint(memInfoFile.SlabKb());
    mLinuxMemoryMeasurements.at("Slab Reclaimable").AddDataPoint(memInfoFile.SlabReclaimable());
    mLinuxMemoryMeasurements.at("Slab Unreclaimable").AddDataPoint(memInfoFile.SlabUnreclaimable());
    mLinuxMemoryMeasurements.at("Swap Used").AddDataPoint(memInfoFile.SwapUsed());
}

void MemoryMetric::GetCmaMemoryUsage()
{
    //LOG_INFO("Getting CMA memory usage");

    long double countKb = 0;
    long double usedKb = 0;
    long double unusedKb = 0;

    long double cmaTotalKb = 0;
    long double cmaTotalUsed = 0;

    // Start by getting CMA breakdown
    try {
        for (const auto &dirEntry: std::filesystem::directory_iterator(
                "/sys/kernel/debug/cma")) {

            // Read CMA metrics
            // Total size of the CMA region
            auto countFile = std::ifstream(dirEntry.path() / "count");
            countFile >> countKb;
            countKb = (countKb * mPageSize) / (long double) 1024;

            // Amount of pages used
            auto usedPagesFile = std::ifstream(dirEntry.path() / "used");
            usedPagesFile >> usedKb;
            usedKb = (usedKb * mPageSize) / (long double) 1024;

            // Calculate how much of that region is unused
            unusedKb = countKb - usedKb;

            // Calculate some totals
            cmaTotalKb += countKb;
            cmaTotalUsed += usedKb;

            std::string cmaName;
            try {
                cmaName = mCmaNames.at(dirEntry.path().filename());
            }
            catch (const std::exception &ex) {
                LOG_ERROR("Could not find CMA name for directory %s", dirEntry.path().filename().string().c_str());
                break;
            }

            // Add to measurements
            auto itr = mCmaMeasurements.find(cmaName);

            if (itr != mCmaMeasurements.end()) {
                // If we have previous measurements for this region, add new data points
                auto &measurement = itr->second;

                measurement.sizeKb = countKb;
                measurement.Used.AddDataPoint(usedKb);
                measurement.Unused.AddDataPoint(unusedKb);
            } else {
                // New CMA region, create measurements
                auto used = Measurement("Used");
                used.AddDataPoint(usedKb);

                auto unused = Measurement("Unused");
                unused.AddDataPoint(unusedKb);

                auto measurement = cmaMeasurement(countKb, used, unused);
                mCmaMeasurements.insert(std::make_pair(cmaName, measurement));
            }
        }

        // Work out how much CMA is borrowed by the kernel (this can occur under memory pressure scenarios where
        // there is not enough memory elsewhere for userspace processes)
        MemInfo memInfoFile;
        mCmaFree.AddDataPoint(memInfoFile.CmaFree());

        long double totalUnused = cmaTotalKb - cmaTotalUsed;
        long double borrowed = totalUnused - memInfoFile.CmaFree();
        mCmaBorrowed.AddDataPoint(borrowed);
    } catch (std::filesystem::filesystem_error &error) {
        LOG_WARN("Failed to open CMA debug file with error %s", error.what());
    }
}

void MemoryMetric::GetGpuMemoryUsage()
{
    if (mGPUMemorySupported) {
        //LOG_INFO("Getting GPU memory usage");

        switch (mPlatform) {
            case (Platform::AMLOGIC): {
                GetGpuMemoryUsageAmlogic();
                break;
            }
            case (Platform::REALTEK): {
                GetGpuMemoryUsageRealtek();
                break;
            }
            case (Platform::BROADCOM): {
                GetGpuMemoryUsageBroadcom();
                break;
            }
        }
    }
}

void MemoryMetric::GetContainerMemoryUsage()
{
    //LOG_INFO("Getting Container memory usage");

    long double memoryUsageKb = 0;
    // List of system containers which we are not interested in.
    std::string ignore_list[] = {"init.scope", "system.slice"};

    std::string memoryCgroupDir = "/sys/fs/cgroup/memory";

    if (!std::filesystem::exists(memoryCgroupDir)) {
        return;
    }

    // Simplest way is to report memory usage by each cgroup, although this can result in some results that don't
    // correspond to a container if something else created that cgroup
    for (const auto &dirEntry: std::filesystem::directory_iterator(
            "/sys/fs/cgroup/memory")) {
        if (!dirEntry.is_directory()) {
            continue;
        }

        auto containerName = dirEntry.path().filename().string();
        if (std::find(std::begin(ignore_list), std::end(ignore_list), containerName.c_str()) == std::end(ignore_list)) {
            auto memoryUsageFile = std::ifstream(dirEntry.path() / "memory.usage_in_bytes");
            memoryUsageFile >> memoryUsageKb;
            memoryUsageKb /= (long double) 1024.0;

            auto itr = mContainerMeasurements.find(containerName);

            if (itr != mContainerMeasurements.end()) {
                auto &measurement = itr->second;
                measurement.AddDataPoint(memoryUsageKb);
            } else {
                Measurement measurement(containerName);
                measurement.AddDataPoint(memoryUsageKb);
                mContainerMeasurements.insert(std::make_pair(containerName, measurement));
            }
        }
    }
}

void MemoryMetric::GetMemoryBandwidth()
{
    // Only supported on Amlogic
    if (mMemoryBandwidthSupported) {
        //LOG_INFO("Getting memory bandwidth usage");

        if (mPlatform == Platform::AMLOGIC) {
            std::ifstream memBandwidthFile("/sys/class/aml_ddr/usage_stat");

            if (!memBandwidthFile) {
                LOG_WARN("Cannot get DDR usage");
                return;
            }

            std::string line;
            long kbps = 0;
            double percent = 0;

            // Know the data we need is in the first two lines, save effort by only reading those lines
            int i = 0;
            while (std::getline(memBandwidthFile, line) && i < 2) {
                if (sscanf(line.c_str(), "MAX bandwidth:  %ld KB/s, usage: %lf%%, tick:%*d us", &kbps, &percent) != 0) {
                    mMemoryBandwidth.maxKBps = kbps;
                    mMemoryBandwidth.maxUsagePercent = percent;
                } else if (
                        sscanf(line.c_str(), "AVG bandwidth:  %ld KB/s, usage: %lf%%, samples:%*d", &kbps, &percent) !=
                        0) {
                    mMemoryBandwidth.averageKBps = kbps;
                    mMemoryBandwidth.averageUsagePercent = percent;
                }
                i++;
            }
        }

    }
}

void MemoryMetric::GetBroadcomBmemUsage()
{
    // LOG_INFO("Getting BMEM Usage");

    std::ifstream broadcomCoreInfo("/proc/brcm/core");

    if (!broadcomCoreInfo) {
        LOG_WARN("Could not open /proc/brcm/core");
        return;
    }

    std::string line;

    char regionName[128];
    int regionSize;
    int regionUsage;
    while (std::getline(broadcomCoreInfo, line)) {
        if (sscanf(line.c_str(), "%*d  %*s %*d %*s   %d %*s %d%% %*d%% %s", &regionSize, &regionUsage,
                   regionName) == 3) {
            // Calculate how many MB we're using since Bcom in their infinite wisdom only give us a percentage
            // Use KB for consistency with everything else
            double usageKb = (regionSize * (regionUsage / 100.0)) * 1024;

            auto itr = std::find_if(mBroadcomBmemMeasurements.begin(), mBroadcomBmemMeasurements.end(),
                                    [&](const Measurement &m)
                                    {
                                        return m.GetName() == std::string(regionName);
                                    });

            if (itr == mBroadcomBmemMeasurements.end()) {
                // New region
                Measurement measurement(regionName);
                measurement.AddDataPoint(usageKb);
                mBroadcomBmemMeasurements.emplace_back(measurement);
            } else {
                auto &measurement = *itr;
                measurement.AddDataPoint(usageKb);
            }
        }
    }
}


void MemoryMetric::CalculateFragmentation()
{
    //LOG_INFO("Getting memory fragmentation");

    std::ifstream buddyInfo("/proc/buddyinfo");

    if (!buddyInfo) {
        LOG_WARN("Could not open buddyinfo");
        return;
    }

    std::string line;
    std::string segment;
    // Get fragmentation for all zones
    while (std::getline(buddyInfo, line)) {
        std::stringstream lineStream(line);
        std::vector<std::string> segments;
        // Split line on space
        while (std::getline(lineStream, segment, ' ')) {
            if (!segment.empty()) {
                segments.emplace_back(segment);
            }
        }

        std::string zoneName = segments[3];
        std::map<int, int> freePages;
        std::map<int, double> fragmentationPercent;

        size_t columnCount = 0;
        if (mPlatform == Platform::AMLOGIC) {
            columnCount = 15;
        } else if (mPlatform == Platform::REALTEK) {
            columnCount = 17;
        } else if (mPlatform == Platform::BROADCOM) {
            columnCount = 15;
        }

        if (segments.size() != columnCount) {
            LOG_WARN("Failed to parse buddyinfo - invalid number of columns (got %zd, expected %zd)", segments.size(),
                     columnCount);
        } else {
            // Calculate fragmentation % for this node
            int totalFreePages = 0;

            //  Get all free page values, and work out total free pages
            for (int i = 4; i < (int) columnCount; i++) {
                int order = i - 4;

                int freeCount = std::stoi(segments[i]);
                totalFreePages += std::pow(2, order) * freeCount;
                freePages[order] = freeCount;
            }

            // Now find out the fragmentation percentages (see https://github.com/dsanders11/scripts/blob/master/Linux_Memory_Fragmentation.pdf and
            // http://thomas.enix.org/pub/rmll2005/rmll2005-gorman.pdf)
            double fragPercentage;
            for (int i = 0; i < (int) freePages.size(); i++) {
                fragPercentage = 0;

                // Seems inefficient...
                for (int j = i; j < (int) freePages.size(); j++) {
                    fragPercentage += (std::pow(2, j)) * freePages[j];
                }
                fragPercentage = (totalFreePages - fragPercentage) / totalFreePages;
                fragmentationPercent[i] = fragPercentage;
            }

            // Update measurements
            auto itr = mMemoryFragmentation.find(zoneName);
            if (itr != mMemoryFragmentation.end()) {
                auto &measurements = itr->second;

                for (int i = 0; i < (int) freePages.size(); i++) {
                    measurements[i].FreePages.AddDataPoint(freePages[i]);
                    measurements[i].Fragmentation.AddDataPoint(fragmentationPercent[i] * 100);
                }
            } else {
                std::vector<memoryFragmentation> measurements = {};
                for (int i = 0; i < (int) freePages.size(); i++) {
                    Measurement fp("FreePages");
                    fp.AddDataPoint(freePages[i]);

                    Measurement frag("Fragmentation");
                    frag.AddDataPoint(fragmentationPercent[i]);
                    memoryFragmentation fragMeasurement(fp, frag);
                    measurements.emplace_back(fragMeasurement);
                }

                mMemoryFragmentation.insert(std::make_pair(zoneName, measurements));
            }
        }
    }
}

/**
 * Broadcom GPU memory allocations.
 * Available from a series of directories under /sys/kernel/debug/dri/0/.
 * Each directory has a 'client' file which needs to be parsed.
 *
 * Example paths:
 *
 * root@xione-sercomm:~# find /sys/kernel/debug/dri/0/ -name client
 * /sys/kernel/debug/dri/0/13449-00000000f601794d/client
 * /sys/kernel/debug/dri/0/13030-00000000cf255c5d/client
 * /sys/kernel/debug/dri/0/12326-00000000426cbc26/client
 * /sys/kernel/debug/dri/0/12298-00000000954ee8cf/client
 * /sys/kernel/debug/dri/0/8804-000000004fe3dec5/client
 * /sys/kernel/debug/dri/0/8632-0000000055df6881/client
 * /sys/kernel/debug/dri/0/7566-000000003bfb5b6e/client
 * root@xione-sercomm:~#
 *
 * Each directory under /sys/kernel/debug/dri/0/ is of the form '<tid>-<64bit hex>'.
 *
 * tid is the thread id of the thread that allocated the gpu mem, the allocation being detailed in the 'client' file under that directory.
 * Not sure what the 64 bit hex is. An address?
 *
 * Example content of a 'client' file:
 *
 * root@xione-sercomm:~# cat /sys/kernel/debug/dri/0/13449-00000000f601794d/client
 *             command objects    Virtual  SHM pages Huge Pages
 *     SkyBrowserLaunc       2     4096KB        0KB        4MB
 * root@xione-sercomm:~#
 *
 * Need to correlate this TID to the main PID of the process to make analysis easier
 *
 * Note that the process name does not include full path so this is instead retrieved from Procrank using the pid extracted from the directory name.
*/
void MemoryMetric::GetGpuMemoryUsageBroadcom()
{
    std::string line;
    pid_t tid;

    for (const auto &entry: std::filesystem::directory_iterator("/sys/kernel/debug/dri/0/")) {
        const auto entryStr = entry.path().filename().string();
        if (entry.is_directory()) {
            // Scan as far as we need to.
            if (sscanf(entryStr.c_str(), "%d-", &tid) != 1) {
                // Not interested in this directory.
                continue;
            }

            std::string pathStr = std::string("/sys/kernel/debug/dri/0/") + entryStr + "/client";
            std::ifstream gpuMem(pathStr.c_str());
            if (!gpuMem) {
                LOG_WARN("Could not open gpu_memory file %s", pathStr.c_str());
                continue;
            }

            while (std::getline(gpuMem, line)) {
                char processName[32];
                unsigned int objectsNum;
                unsigned long virtualMemNum;
                char virtualMemNumUnit[3];
                unsigned long virtualMemNumBytes;

                // Scan as far as we need to.
                if (sscanf(line.c_str(), " %s %d %ld%2c", processName, &objectsNum, &virtualMemNum,
                           virtualMemNumUnit) == 4) {

                    virtualMemNumUnit[2] = 0;

                    std::string virtualMemNumUnitStr(virtualMemNumUnit);

                    if (virtualMemNumUnitStr == "KB") {
                        virtualMemNumBytes = virtualMemNum * 1024;
                    } else if (virtualMemNumUnitStr == "MB") {
                        virtualMemNumBytes = virtualMemNum * 1024 * 1024;
                    } else if (virtualMemNumUnitStr == "GB") {
                        virtualMemNumBytes = virtualMemNum * 1024 * 1024 * 1024;
                    } else {
                        LOG_WARN("Could not parse this line: \'%s\'", line.c_str());
                        continue;
                    }

                    // Convert TID to parent PID (TGID) to make things easier to correlate later on
                    pid_t pid = tidToParentPid(tid);

                    auto itr = mGpuMeasurements.find(pid);

                    if (itr != mGpuMeasurements.end()) {
                        // Already got a measurement for this PID
                        auto &measurement = itr->second;
                        measurement.Used.AddDataPoint(virtualMemNumBytes / (long double) 1024.0);
                    } else {
                        Process process(pid);
                        Measurement used(process.name());
                        used.AddDataPoint(virtualMemNumBytes / (long double) 1024.0);

                        auto measurement = gpuMeasurement(process, used);
                        mGpuMeasurements.insert(std::make_pair(pid, measurement));
                    }
                }
            }
        }
    }
}

/* Amlogic GPU memory allocations
 *
 * Sizes are in pages, so convert to bytes
 *
 * root@sky-llama-panel:~# cat /sys/kernel/debug/mali0/gpu_memory
    mali0            total used_pages      25939
    ----------------------------------------------------
    kctx             pid              used_pages
    ----------------------------------------------------
    f1dbf000      14880       4558
    f1c19000      14438        135
    f1bb1000      14292      16359
    f18c0000      10899       4887
*/
void MemoryMetric::GetGpuMemoryUsageAmlogic()
{
    std::ifstream gpuMem("/sys/kernel/debug/mali0/gpu_memory");

    if (!gpuMem) {
        LOG_WARN("Could not open gpu_memory file");
        return;
    }

    std::string line;
    long gpuPages;
    pid_t pid;

    while (std::getline(gpuMem, line)) {
        if (sscanf(line.c_str(), "%*x %d %ld", &pid, &gpuPages) != 0) {
            unsigned long gpuBytes = gpuPages * mPageSize;

            auto itr = mGpuMeasurements.find(pid);

            if (itr != mGpuMeasurements.end()) {
                // Already got a measurement for this PID
                auto &measurement = itr->second;
                measurement.Used.AddDataPoint(gpuBytes / (long double) 1024.0);
            } else {
                Process process(pid);

                Measurement used(process.name());
                used.AddDataPoint(gpuBytes / (long double) 1024.0);

                auto measurement = gpuMeasurement(process, used);
                mGpuMeasurements.insert(std::make_pair(pid, measurement));
            }
        }
    }
}


/* Realtek GPU memory allocations
 *
 * Uses a similar format to Amlogic but rendered slightly differently
 *
 * Sizes are in pages, so convert to bytes
 * root@skyxione:/sys/kernel/debug/mali0# cat gpu_memory
 *
 * mali0                  45605
 * kctx-0xfa847000      14102      15898
 * kctx-0xf7953000         42      15833
 * kctx-0xff0b0000       3316       9134
 * kctx-0xfec18000      20929       8344
 * kctx-0xfb9df000        135       6235
 * kctx-0xfb12e000       7081       4962
*/
void MemoryMetric::GetGpuMemoryUsageRealtek()
{
    std::ifstream gpuMem("/sys/kernel/debug/mali0/gpu_memory");

    if (!gpuMem) {
        LOG_WARN("Could not open gpu_memory file");
        return;
    }

    std::string line;
    long gpuPages;
    pid_t pid;

    while (std::getline(gpuMem, line)) {
        if (sscanf(line.c_str(), "  kctx-0x%*x %ld %d", &gpuPages, &pid) != 0) {
            unsigned long gpuBytes = gpuPages * mPageSize;

            auto itr = mGpuMeasurements.find(pid);

            if (itr != mGpuMeasurements.end()) {
                // Already got a measurement for this PID
                auto &measurement = itr->second;
                measurement.Used.AddDataPoint(gpuBytes / (long double) 1024.0);
            } else {
                Process process(pid);

                Measurement used(process.name());
                used.AddDataPoint(gpuBytes / (long double) 1024.0);

                auto measurement = gpuMeasurement(process, used);
                mGpuMeasurements.insert(std::make_pair(pid, measurement));
            }
        }
    }
}

/**
 * Given a thread ID, return the main PID (TGID) the thread belongs to
 * @return PID
 */
pid_t MemoryMetric::tidToParentPid(pid_t tid)
{
    std::string statusFilePath = "/proc/" + std::to_string(tid) + "/status";

    std::ifstream statusFile(statusFilePath);

    if (!statusFile) {
        LOG_WARN("Failed to open file %s", statusFilePath.c_str());
        return -1;
    }

    std::string line;
    pid_t pid;

    while (std::getline(statusFile, line)) {
        if (sscanf(line.c_str(), "Tgid:\t%d", &pid) == 1) {
            return pid;
        }
    }

    // Failed to find Tgid in file, weird?
    return -1;
}