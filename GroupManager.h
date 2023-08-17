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

#include "nlohmann/json.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include "Group.h"


/**
 * Store the groups loaded from the provided JSON file at MemCapture launch and determine which group processes belong to
 * based on their name
 */
class GroupManager
{
public:
    enum class groupType
    {
        PROCESS,
        CONTAINER
    };

    explicit GroupManager(nlohmann::json groupList);

    std::optional<std::string> getGroup(groupType type, const std::string& name);

private:
    std::vector<Group> mProcessGroups;
    std::vector<Group> mContainerGroups;
};
