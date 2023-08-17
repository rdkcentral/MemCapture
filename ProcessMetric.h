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

#include "IMetric.h"
#include <thread>
#include <condition_variable>
#include <map>
#include <mutex>
#include <utility>
#include "GroupManager.h"
#include "JsonReportGenerator.h"
#include "Procrank.h"
#include "ProcessMeasurement.h"

class ProcessMetric : public IMetric
{
public:
    ProcessMetric(std::shared_ptr<JsonReportGenerator> reportGenerator);

    ~ProcessMetric();

    void StartCollection(std::chrono::seconds frequency) override;

    void StopCollection() override;

    void SaveResults() override;


private:
    void CollectData(std::chrono::seconds frequency);

    void DeduplicateData();

private:
    std::thread mCollectionThread;
    bool mQuit;
    std::condition_variable mCv;
    std::mutex mLock;

    std::vector<processMeasurement> mMeasurements;

    const std::shared_ptr<JsonReportGenerator> mReportGenerator;
};
