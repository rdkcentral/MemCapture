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


#include "JsonReportGenerator.h"
#include "Log.h"

#include <utility>

JsonReportGenerator::JsonReportGenerator(std::shared_ptr<Metadata> metadata,
                                         std::optional<std::shared_ptr<GroupManager>> groupManager)
        : mMetadata(std::move(metadata)), mGroupManager(std::move(groupManager)), mJson()
{
    mJson["processes"] = nlohmann::json::array();
    mJson["metadata"] = {};
    mJson["cpuIdleStats"] = nullptr;

    mJson["grandTotal"]["linuxUsage"] = 0.0;
    mJson["grandTotal"]["calculatedUsage"] = 0.0;
}

void JsonReportGenerator::addDataset(const std::string &name, const std::vector<dataItems> &data)
{
    if (data.empty()) {
        // no data, so no-op
        return;
    }

    nlohmann::json dataSet;

    dataSet["name"] = name;
    dataSet["data"] = nlohmann::json::array();
    dataSet["_columnOrder"] = nlohmann::json::array();

    bool setColumnOrder = false;

    // Inja uses the unordered JSON format, and changing to ordered_json breaks things, see
    // https://github.com/pantor/inja/pull/214. To work around this, add an array that defines the order
    // of the columns. The _columnOrder array will also be responsible for generating the table headings

    for (const auto& item : data) {
        nlohmann::json tmp;

        for (const auto &value: item) {
            std::visit(overload{
                    [&](const std::pair<std::string, std::string> &v)
                    {
                        tmp[v.first] = v.second;

                        if (!setColumnOrder) {
                            dataSet["_columnOrder"].emplace_back(v.first);
                        }
                    },
                    [&](const Measurement &v)
                    {
                        tmp[v.GetName()]["Min"] = v.GetMinRounded();
                        tmp[v.GetName()]["Max"] = v.GetMaxRounded();
                        tmp[v.GetName()]["Average"] = v.GetAverageRounded();

                        if (!setColumnOrder) {
                            dataSet["_columnOrder"].emplace_back(v.GetName() + " (Min)");
                            dataSet["_columnOrder"].emplace_back(v.GetName() + " (Max)");
                            dataSet["_columnOrder"].emplace_back(v.GetName() + " (Average)");
                        }
                    }
            }, value);
        }
        dataSet["data"].emplace_back(tmp);
        setColumnOrder = true;
    }

    mJson["data"].emplace_back(dataSet);
}


nlohmann::json JsonReportGenerator::getJson()
{
    mJson["metadata"] = {
            {"image",       mMetadata->Image()},
            {"platform",    mMetadata->Platform()},
            {"mac",         mMetadata->Mac()},
            {"timestamp",   mMetadata->ReportTimestamp()},
            {"duration",    mMetadata->Duration()},
            {"swapEnabled", mMetadata->SwapEnabled()}
    };

    return mJson;
}

void JsonReportGenerator::addProcesses(std::vector<processMeasurement> &processes)
{
    // Sort by PSS desc
    std::sort(processes.begin(), processes.end(), [](const processMeasurement &a, const processMeasurement &b)
    {
        return a.Pss.GetAverageRounded() > b.Pss.GetAverageRounded();
    });

    for (const auto &process: processes) {
        nlohmann::json processJson;

        processJson["pid"] = process.ProcessInfo.pid();
        processJson["ppid"] = process.ProcessInfo.ppid();
        processJson["name"] = process.ProcessInfo.name();
        processJson["cmdline"] = process.ProcessInfo.cmdline();

        processJson["systemdService"] = process.ProcessInfo.systemdService().has_value()
                                        ? process.ProcessInfo.systemdService().value() : "";
        processJson["container"] = process.ProcessInfo.container().has_value() ? process.ProcessInfo.container().value()
                                                                               : "";

        if (mGroupManager.has_value()) {
            processJson["group"] = process.ProcessInfo.group(mGroupManager.value()).has_value()
                                   ? process.ProcessInfo.group(
                            mGroupManager.value()).value() : "";
        } else {
            processJson["group"] = "";
        }

        processJson["rss"] = process.Rss.ToJson();
        processJson["pss"] = process.Pss.ToJson();
        processJson["uss"] = process.Uss.ToJson();
        processJson["vss"] = process.Vss.ToJson();
        processJson["swap"] = process.Swap.ToJson();
        processJson["swapPss"] = process.SwapPss.ToJson();
        processJson["swapZram"] = process.SwapZram.ToJson();
        processJson["locked"] = process.Locked.ToJson();

        mJson["processes"].emplace_back(processJson);
    }


    // Calculate PSS memory per group
    if (mGroupManager.has_value()) {
        std::map<std::string, long double> pssPerGroup;
        mJson["pssByGroup"] = nlohmann::json::array();

        for (const auto &process: processes) {
            auto group = process.ProcessInfo.group(mGroupManager.value());
            if (group.has_value()) {
                pssPerGroup[group.value()] += process.Pss.GetAverage();
            }
        }

        // Sort the map by PSS desc so the pie chart appears nicely
        std::vector<std::pair<std::string, long double>> pairs;
        pairs.reserve(pssPerGroup.size());
        for (auto &itr: pssPerGroup) {
            pairs.emplace_back(itr);
        }

        std::sort(pairs.begin(), pairs.end(),
                  [](std::pair<std::string, long double> &a, std::pair<std::string, long double> &b)
                  {
                      return a.second > b.second;
                  });

        for (const auto &group: pairs) {
            nlohmann::json tmp;
            tmp["groupName"] = group.first;
            tmp["pss"] = std::round((int) group.second);

            mJson["pssByGroup"].emplace_back(tmp);
        }
    } else {
        mJson["pssByGroup"] = nullptr;
    }

}

void JsonReportGenerator::setAverageLinuxMemoryUsage(int valueKb)
{
    // Store in MB
    mJson["grandTotal"]["linuxUsage"] = valueKb / 1024.0L;
}

void JsonReportGenerator::addToAccumulatedMemoryUsage(long double valueKb)
{
    // Store in MB
    long double usage = mJson["grandTotal"]["calculatedUsage"];
    usage += valueKb / 1024.0L;

    mJson["grandTotal"]["calculatedUsage"] = usage;
}

#ifdef ENABLE_CPU_IDLE_METRICS
void JsonReportGenerator::addCpuIdleMetrics(const IDLE_METRICS_V2 &metrics)
{
    nlohmann::json idleMetricsJson;

    unsigned long long totalRuntimeNs = metrics.metricEndTime - metrics.metricStartTime;

    auto &loadStats = idleMetricsJson["loadStats"];

    // Per CPU stats
    unsigned long long idleTime = 0;
    for (int i = 0; i < T962X3_NUM_CPUS; i++) {
        // Convert ns to s
        loadStats["cpu"][i]["idle"]["sum"] = metrics.idle[i].sum_idle_time / 1000000.0;
        loadStats["cpu"][i]["idle"]["percent"] = (float)((float)metrics.idle[i].sum_idle_time / (float)(totalRuntimeNs / 1000.0)) * 100;
        idleTime += metrics.idle[i].sum_idle_time;
    }

    // Summary
    // ms
    loadStats["overall"]["idle"]["sum"] = idleTime / 1000000.0;
    loadStats["overall"]["load"]["sum"] = metrics.sum_all_cpus_running_time / 1000000.0;
    loadStats["overall"]["load"]["count"] = metrics.count;
    loadStats["overall"]["load"]["percent"] = (float)((float)metrics.sum_all_cpus_running_time / (float)totalRuntimeNs) * 100;


    // Load times
    unsigned long run_time_lt_1ms_count = metrics.count - (metrics.run_time_gt_1ms + metrics.run_time_gt_5ms +
                                                            metrics.run_time_gt_10ms + metrics.run_time_gt_20ms +
                                                            metrics.run_time_gt_30ms + metrics.run_time_gt_40ms +
                                                            metrics.run_time_gt_50ms + metrics.run_time_gt_75ms +
                                                            metrics.run_time_gt_100ms);

    // Store the amount of times the CPU was under load for the specified duration
    loadStats["load"]["lt1ms"] = run_time_lt_1ms_count;          // < 1ms
    loadStats["load"]["gt1ms"] = metrics.run_time_gt_1ms;       // >= 1ms && < 5ms
    loadStats["load"]["gt5ms"] = metrics.run_time_gt_5ms;       // >= 5ms && < 10ms
    loadStats["load"]["gt10ms"] = metrics.run_time_gt_10ms;     // >= 10ms && < 20 ms
    loadStats["load"]["gt20ms"] = metrics.run_time_gt_20ms;     // >= 20ms && < 30 ms
    loadStats["load"]["gt30ms"] = metrics.run_time_gt_30ms;     // >= 30ms && < 40 ms
    loadStats["load"]["gt40ms"] = metrics.run_time_gt_40ms;     // >= 40ms && < 50 ms
    loadStats["load"]["gt50ms"] = metrics.run_time_gt_50ms;     // >= 50ms && < 75 ms
    loadStats["load"]["gt75ms"] = metrics.run_time_gt_75ms;     // >= 75ms && < 100 ms
    loadStats["load"]["gt100ms"] = metrics.run_time_gt_100ms;    // >= 100ms

    mJson["cpuIdleStats"] = loadStats;
}
#endif
