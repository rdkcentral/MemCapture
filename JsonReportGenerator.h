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

#include <variant>

#include "nlohmann/json.hpp"

#include "ProcessMeasurement.h"
#include "GroupManager.h"
#include "Metadata.h"

template<class... Ts>
struct overload : Ts ...
{
    using Ts::operator()...;
};
template<class... Ts> overload(Ts...) -> overload<Ts...>;

class JsonReportGenerator
{
public:
    using row = std::vector<std::variant<std::string, Measurement>>;

    using dataItems = std::vector<std::variant<std::pair<std::string, std::string>, Measurement>>;

    JsonReportGenerator(std::shared_ptr<Metadata> metadata, std::optional<std::shared_ptr<GroupManager>> groupManager);

    void addDataset(const std::string& name, const std::vector<dataItems>& data);

    void addProcesses(std::vector<processMeasurement> &processes);

    void setAverageLinuxMemoryUsage(int valueKb);

    void addToAccumulatedMemoryUsage(long double valueKb);

    nlohmann::json getJson();

private:
    const std::shared_ptr<Metadata> mMetadata;
    const std::optional<std::shared_ptr<GroupManager>> mGroupManager;

    nlohmann::json mJson;

    std::vector<Process> mProcesses;
};
