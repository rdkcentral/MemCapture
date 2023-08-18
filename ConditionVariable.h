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

#include "Log.h"
#include <pthread.h>
#include <time.h>

#include <chrono>
#include <mutex>
#include <condition_variable>

/**
 * @brief A custom condition variable that uses the monotonic clock for all timed waits
 *
 * This is necessary due to a long-standing bug in GCC where wait_for uses the system clock, causing the condition
 * variable to wake early if the system clock jumps (especially an issue during bootup on an embedded platform without
 * a battery-backup RTC when NTP sync occurs)
 *
 */
class ConditionVariable
{
public:
    ConditionVariable()
    {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
#if !defined(HAS_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)
        pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif
        pthread_cond_init(&mCond, &attr);

        pthread_condattr_destroy(&attr);
    }

    ConditionVariable(const ConditionVariable &) = delete;

    ConditionVariable(const ConditionVariable &&) = delete;

    ~ConditionVariable()
    {
        int err = pthread_cond_destroy(&mCond);
        if (err != 0)
            LOG_ERROR("Condition variable failed to be destroyed with error %d", errno);
    }

public:
    void notify_one()
    {
        int err = pthread_cond_signal(&mCond);
        if (err != 0) {
            LOG_ERROR("Condition variable failed to be signaled with error %d", errno);
        }
    }

    void notify_all()
    {
        int err = pthread_cond_broadcast(&mCond);
        if (err != 0) {
            LOG_ERROR("Condition variable failed to broadcast with error %d", errno);
        }
    }

private:
    struct timespec calcTimeoutAbs(const std::chrono::nanoseconds &rel_time)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        ts.tv_sec += std::chrono::duration_cast<std::chrono::seconds>(rel_time).count();
        ts.tv_nsec += (rel_time % std::chrono::seconds(1)).count();

        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_nsec -= 1000000000L;
            ts.tv_sec += 1;
        } else if (ts.tv_nsec < 0L) {
            // This can happend when rel_time.tv_nsec is big negative, and ts.tv_nsec
            // is small positive
            ts.tv_nsec += 1000000000L;
            ts.tv_sec -= 1;
        }

        return ts;
    }

    struct timespec calcTimeoutRel(const std::chrono::nanoseconds &rel_time)
    {
        struct timespec ts;

        ts.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(rel_time).count();
        ts.tv_nsec = (rel_time % std::chrono::seconds(1)).count();

        return ts;
    }

public:
    void wait(std::unique_lock<std::mutex> &lock)
    {
        int err = pthread_cond_wait(&mCond, lock.mutex()->native_handle());
        if (err != 0) {
            LOG_ERROR("Condition variable failed to wait with error %d", errno);
        }
    }

    template<class Predicate>
    void wait(std::unique_lock<std::mutex> &lock, Predicate pred)
    {
        while (!pred()) {
            int err = pthread_cond_wait(&mCond, lock.mutex()->native_handle());
            if (err != 0) {
                LOG_ERROR("Condition variable failed to wait with error %d", errno);
            }
        }
    }


    template<class Rep, class Period>
    std::cv_status wait_for(std::unique_lock<std::mutex> &lock,
                            const std::chrono::duration<Rep, Period> &rel_time)
    {
        if (rel_time.count() < 0) {
            LOG_DEBUG("Negative wait period, timeout occured");
            return std::cv_status::timeout;
        }
#if defined(HAS_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)
        const struct timespec ts = calcTimeoutRel(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time));
        int err = pthread_cond_timedwait_relative_np(&mCond, lock.mutex()->native_handle(), &ts);
#else
        const struct timespec ts = calcTimeoutAbs(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time));
        int err = pthread_cond_timedwait(&mCond, lock.mutex()->native_handle(), &ts);
#endif
        if (err == 0) {
            return std::cv_status::no_timeout;
        }
        if (err == ETIMEDOUT) {
            return std::cv_status::timeout;
        } else {
            LOG_ERROR("Condition variable timed wait failed with error %d", errno);
            return std::cv_status::timeout;
        }
    }

    template<class Rep, class Period, class Predicate>
    bool wait_for(std::unique_lock<std::mutex> &lock,
                  const std::chrono::duration<Rep, Period> &rel_time,
                  Predicate pred)
    {
        if (rel_time.count() < 0) {
            LOG_DEBUG("Negative wait period, timeout occured");
            return pred();
        }
#if defined(HAS_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)
        const struct timespec ts = calcTimeoutRel(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time));
#else
        const struct timespec ts = calcTimeoutAbs(std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time));
#endif

        while (!pred()) {
#if defined(HAS_PTHREAD_COND_TIMEDWAIT_RELATIVE_NP)
            int err = pthread_cond_timedwait_relative_np(&mCond, lock.mutex()->native_handle(), &ts);
#else
            int err = pthread_cond_timedwait(&mCond, lock.mutex()->native_handle(), &ts);
#endif
            if (err == ETIMEDOUT) {
                return pred();
            } else if (err != 0) {
                LOG_ERROR("Condition variable error in wait_for '%d'", err);
            }
        }

        return true;
    }


    std::cv_status wait_until(std::unique_lock<std::mutex> &lock,
                              const std::chrono::time_point<std::chrono::steady_clock> &timeout_time)
    {
        auto rel_time = (timeout_time - std::chrono::steady_clock::now());
        return wait_for(lock, rel_time);
    }

    template<class Predicate>
    bool wait_until(std::unique_lock<std::mutex> &lock,
                    const std::chrono::time_point<std::chrono::steady_clock> &timeout_time,
                    Predicate pred)
    {
        auto rel_time = (timeout_time - std::chrono::steady_clock::now());
        return wait_for(lock, rel_time, pred);
    }

public:
    typedef pthread_cond_t *native_handle_type;

    native_handle_type native_handle()
    {
        return &mCond;
    }

private:
    pthread_cond_t mCond;

};