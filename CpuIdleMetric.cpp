//
// Created by Stephen F on 14/11/23.
//

#ifdef ENABLE_CPU_IDLE_METRICS

#include "CpuIdleMetric.h"
#include "Log.h"
#include "Procrank.h"
#include <sys/prctl.h>


CpuIdleMetric::CpuIdleMetric(std::shared_ptr<JsonReportGenerator> reportGenerator) :
        mReportGenerator(std::move(reportGenerator)),
        mIdleMetrics({})
{

}


void CpuIdleMetric::StartCollection([[maybe_unused]] const std::chrono::seconds frequency)
{
    // Check if collectd is running in the background. If it is, it might interfere with the stat collection since
    // the idle metrics are globally scoped
    LOG_INFO("Starting CPU idle metric collection");

    Procrank procrank;
    auto runningProcesses = procrank.GetMemoryUsage();

    auto collectRunning = std::any_of(runningProcesses.begin(), runningProcesses.end(),
                                      [](const Procrank::ProcessMemoryUsage &proc)
                                      {
                                          return proc.process.name().find("collectd") != std::string::npos;
                                      });

    if (collectRunning) {
        LOG_WARN("*** WARNING:: Collectd is running in the background. This may cause incorrect CPU idle readings ***");
    }

    // Start by resetting the stats
    auto ret = prctl(PR_GET_IDLE_METRICS, &mIdleMetrics, IDLE_METRICS_VERSION_V2, 0, 0);
    if (ret != 0) {
        LOG_ERROR("Failed to reset idle stats with error %d (%s)", ret, strerror(errno));
    }

    // Nothing to do now until we stop collection
}

void CpuIdleMetric::StopCollection()
{
    // Get the latest stats
    LOG_INFO("Stopping CPU idle metric collection");
    auto ret = prctl(PR_GET_IDLE_METRICS, &mIdleMetrics, IDLE_METRICS_VERSION_V2, 0, 0);
    if (ret != 0)
    {
        LOG_ERROR("Failed to get idle stats with error %d (%s)", ret, strerror(errno));
    }
}

void CpuIdleMetric::SaveResults()
{
    mReportGenerator->addCpuIdleMetrics(mIdleMetrics);
}

#endif