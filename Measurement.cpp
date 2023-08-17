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

#include "Measurement.h"
#include <limits>
#include <utility>
#include <cmath>

Measurement::Measurement(std::string name)
        : mName(std::move(name)),
          mCount(0),
          mMin(std::numeric_limits<double>::max()),
          mMax(std::numeric_limits<double>::min()),
          mAverage(0),
          mTotal(0)
{

}

/**
 * @brief Add a new data point and update the min/max/average values
 * @param value Data point to add
 */
void Measurement::AddDataPoint(long double value)
{
    if (value < mMin) {
        mMin = value;
    }

    if (value > mMax) {
        mMax = value;
    }

    // TODO:: This is simplistic and has the potential for overflowing for long data collection sessions.
    mTotal += value;
    mCount++;

    mAverage = mTotal / mCount;
}

long double Measurement::GetMin() const
{
    return mMin;
}

int Measurement::GetMinRounded() const
{
    return (int) std::round(mMin);
}

long double Measurement::GetMax() const
{
    return mMax;
}

int Measurement::GetMaxRounded() const
{
    return (int) std::round(mMax);
}

long double Measurement::GetAverage() const
{
    return mAverage;
}

int Measurement::GetAverageRounded() const
{
    return (int) std::round(mAverage);
}

std::string Measurement::GetName() const
{
    return mName;
}

nlohmann::json Measurement::ToJson() const
{
    return {
            {"min",     GetMinRounded()},
            {"max",     GetMaxRounded()},
            {"average", GetAverageRounded()}
    };
}