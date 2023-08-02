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

#include "Smaps.h"

#include <filesystem>
#include <fstream>
#include <climits>
#include <cstring>

Smaps::Smaps(pid_t pid) : mPid(pid), mRss(0), mPss(0), mSwap(0), mSwapPss(0), mLocked(0), mPrivateClean(0), mPrivateDirty(0),
                          mSize(0)
{
    char buffer[PATH_MAX];
    snprintf(buffer, sizeof(buffer), "/proc/%d/smaps_rollup", mPid);

    if (std::filesystem::exists(buffer)) {
        parseSmapsRollup();
    } else {
        parseSmaps();
    }
}

void Smaps::parseSmaps()
{
    char filePath[PATH_MAX];
    snprintf(filePath, sizeof(filePath), "/proc/%d/smaps", mPid);

    std::ifstream smapsFile(filePath);
    if (!smapsFile) {
        // Process might have died, don't log anything
        return;
    }

    std::string line;
    while (std::getline(smapsFile, line)) {
        auto entry = parseSmapsLine(line);

        switch (entry.first) {
            case SmapsField::Pss:
                mPss += entry.second;
                break;
            case SmapsField::Rss:
                mRss += entry.second;
                break;
            case SmapsField::Swap:
                mSwap += entry.second;
                break;
            case SmapsField::SwapPss:
                mSwapPss += entry.second;
                break;
            case SmapsField::Locked:
                mLocked += entry.second;
                break;
            case SmapsField::PrivateClean:
                mPrivateClean += entry.second;
                break;
            case SmapsField::PrivateDirty:
                mPrivateDirty += entry.second;
                break;
            case SmapsField::Size:
                mSize += entry.second;
                break;
            case SmapsField::Ignore:
            default:
                break;
        }
    }
}

void Smaps::parseSmapsRollup()
{
    char filePath[PATH_MAX];
    snprintf(filePath, sizeof(filePath), "/proc/%d/smaps_rollup", mPid);

    std::ifstream smapsFile(filePath);
    if (!smapsFile) {
        // Process might have died, don't log anything
        return;
    }

    std::string line;
    while (std::getline(smapsFile, line)) {
        auto entry = parseSmapsLine(line);

        switch (entry.first) {
            case SmapsField::Pss:
                mPss = entry.second;
                break;
            case SmapsField::Rss:
                mRss = entry.second;
                break;
            case SmapsField::Swap:
                mSwap = entry.second;
                break;
            case SmapsField::SwapPss:
                mSwapPss = entry.second;
                break;
            case SmapsField::Locked:
                mLocked = entry.second;
                break;
            case SmapsField::PrivateClean:
                mPrivateClean = entry.second;
                break;
            case SmapsField::PrivateDirty:
                mPrivateDirty = entry.second;
                break;
            case SmapsField::Size:
                mSize = entry.second;
                break;
            case SmapsField::Ignore:
            default:
                break;
        }
    }
}

std::pair<Smaps::SmapsField, long> Smaps::parseSmapsLine(std::string_view line)
{
    const char *end = line.data();

    // https://lore.kernel.org/patchwork/patch/1088579/ introduced tabs. Handle this case as well.
    // Find the end of the key
    while (*end && !isspace(*end)) {
        end++;
    }

    // For non-rollup files, there is a lot of data so be as fast as possible and try to avoid costly strings
    // Check if the last non-whitespace value was a :
    if (*end && end > line.data() && *(end - 1) == ':') {
        // Find the start of the actual value
        const char *c = end;
        while (isspace(*c)) {
            c++;
        }

        // Parse
        switch (line[0]) {
            case 'P':
                if (strncmp(line.data(), "Pss:", 4) == 0) {
                    return std::make_pair(Smaps::SmapsField::Pss, strtol(c, nullptr, 10));
                } else if (strncmp(line.data(), "Private_Clean:", 14) == 0) {
                    return std::make_pair(Smaps::SmapsField::PrivateClean, strtol(c, nullptr, 10));
                } else if (strncmp(line.data(), "Private_Dirty:", 14) == 0) {
                    return std::make_pair(Smaps::SmapsField::PrivateDirty, strtol(c, nullptr, 10));
                }
                break;
            case 'S':
                if (strncmp(line.data(), "Swap:", 5) == 0) {
                    return std::make_pair(Smaps::SmapsField::Swap, strtol(c, nullptr, 10));
                } else if (strncmp(line.data(), "SwapPss:", 8) == 0) {
                    return std::make_pair(Smaps::SmapsField::SwapPss, strtol(c, nullptr, 10));
                } else if (strncmp(line.data(), "Size:", 5) == 0) {
                    return std::make_pair(Smaps::SmapsField::Size, strtol(c, nullptr, 10));
                }
                break;
            case 'R':
                if (strncmp(line.data(), "Rss:", 4) == 0) {
                    return std::make_pair(Smaps::SmapsField::Rss, strtol(c, nullptr, 10));
                }
                break;
            case 'L':
                if (strncmp(line.data(), "Locked:", 7) == 0) {
                    return std::make_pair(Smaps::SmapsField::Locked, strtol(c, nullptr, 10));
                }
                break;
        }
    }

    return std::make_pair(Smaps::SmapsField::Ignore, 0);
}