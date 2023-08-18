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

#include <sys/types.h>
#include <unistd.h>
#include <string_view>

/**
 * If smaps_rollup is available, will use that. Otherwise will use smaps and sum everything manually.
 */
class Smaps
{
public:
    Smaps(pid_t pid);

    long Rss() const
    {
        return mRss;
    }

    long Pss() const
    {
        return mPss;
    }

    long Swap() const
    {
        return mSwap;
    }

    long SwapPss() const
    {
        return mSwapPss;
    }

    long Locked() const
    {
        return mLocked;
    }

    long Uss() const
    {
        return mPrivateClean + mPrivateDirty;
    }

    long Vss() const
    {
        return mSize;
    }

private:
    enum class SmapsField
    {
        Pss,
        Rss,
        Swap,
        SwapPss,
        Locked,
        PrivateClean,
        PrivateDirty,
        Size,
        Ignore
    };

private:
    void parseSmaps();

    void parseSmapsRollup();

    std::pair<SmapsField, long> parseSmapsLine(std::string_view line);

private:
    pid_t mPid;

    long mRss;
    long mPss;
    long mSwap;
    long mSwapPss;
    long mLocked;
    long mPrivateClean;
    long mPrivateDirty;
    long mSize;
};
