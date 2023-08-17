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

/**
 * @brief Utility wrapper over the /proc/meminfo file to pull data from it easily
 */
class MemInfo
{
public:
    MemInfo();

    long MemTotalKb() const
    {
        return mTotal;
    }

    long MemFreeKb() const
    {
        return mFree;
    }

    long MemAvailableKb() const
    {
        return mAvailable;
    }

    long MemUsedKb() const
    {
        return mUsed;
    }

    long BuffersKb() const
    {
        return mBuffers;
    }

    long CachedKb() const
    {
        return mCached;
    }

    long SlabKb() const
    {
        return mSlab;
    }

    long SlabReclaimable() const
    {
        return mSReclaimable;
    }

    long SlabUnreclaimable() const
    {
        return mSUnreclaimable;
    }

    long SwapTotal() const
    {
        return mSwapTotal;
    }

    long SwapFree() const
    {
        return mSwapFree;
    }

    long SwapUsed() const {
        return mSwapTotal - mSwapFree;
    }

    long CmaTotal() const
    {
        return mCmaTotal;
    }

    long CmaFree() const
    {
        return mCmaFree;
    }

private:
    void parseMemInfo();


private:
    long mTotal;
    long mFree;
    long mAvailable;
    long mUsed;
    long mBuffers;
    long mCached;
    long mSlab;
    long mSReclaimable;
    long mSUnreclaimable;
    long mSwapTotal;
    long mSwapFree;
    long mCmaTotal;
    long mCmaFree;
};
