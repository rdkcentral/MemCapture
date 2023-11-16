//
// Created by Stephen F on 14/11/23.
//

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