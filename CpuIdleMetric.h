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

#ifdef ENABLE_CPU_IDLE_METRICS

#include "IMetric.h"
#include "JsonReportGenerator.h"
#include <chrono>
#include <memory>
#include <thread>
#include <condition_variable>
#include <mutex>
#include "Platform.h"
#include <sys/prctl.h>

/**
 * @brief On supported platforms, retrieve the CPU idle metrics. Requires kernel patch.
 *
 * Only enabled when MemCapture built with ENABLE_CPU_IDLE_METRICS set
 */
class CpuIdleMetric : public IMetric
{
public:
    CpuIdleMetric(std::shared_ptr<JsonReportGenerator> reportGenerator);

    ~CpuIdleMetric() override = default;

    void StartCollection(std::chrono::seconds frequency) override;

    void StopCollection() override;

    void SaveResults() override;

private:
    const std::shared_ptr<JsonReportGenerator> mReportGenerator;

    IDLE_METRICS_V2 mIdleMetrics;
};

#endif