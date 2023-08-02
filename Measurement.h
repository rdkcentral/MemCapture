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
#pragma once

#include <string>
#include "nlohmann/json.hpp"

/**
 * @brief Container for a data measurement, allowing for calculating running the min/max/average values
 *
 * Each measurement should have a unique name
 *
 */
class Measurement
{
public:
    explicit Measurement(std::string name);

public:
    void AddDataPoint(long double value);

    long double GetMin() const;
    int GetMinRounded() const;

    long double GetMax() const;
    int GetMaxRounded() const;

    long double GetAverage() const;
    int GetAverageRounded() const;

    std::string GetName() const;

    nlohmann::json ToJson() const;

private:
    std::string mName;

    int mCount;
    long double mMin;
    long double mMax;

    long double mAverage;
    long double mTotal;

};
