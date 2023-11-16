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

#ifndef MEMCAPTURE_IMETRIC_H
#define MEMCAPTURE_IMETRIC_H

#include <chrono>

/**
 * @brief Represent a category of metrics - e.g. memory usage, performance data etc
 */
class IMetric
{
public:
    virtual ~IMetric() = default;

    /**
     * @brief Start collecting data every X seconds and store the results in memory
     *
     * It is expected this will start a new thread and return
     *
     * @param[in]   frequency   How often to collect the data
     */
    virtual void StartCollection(std::chrono::seconds frequency) = 0;

    /**
     * @brief Stop any running data collection
     */
    virtual void StopCollection() = 0;

    /**
     * Print the results of the data collection to stdout
     *
     * Expected to format results ina table
     */
    virtual void SaveResults() = 0;
};


#endif //MEMCAPTURE_IMETRIC_H
