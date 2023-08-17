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

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HAVE_GNU_STRERROR_R 1

#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define LEVEL_DEBUG 3
#define LEVEL_INFO 2
#define LEVEL_WARN 1
#define LEVEL_ERROR 0

#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...) \
    __LOG(LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#define LOG_INFO(fmt, ...) \
    __LOG(LEVEL_INFO, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    __LOG(LEVEL_WARN, fmt, ##__VA_ARGS__)

#define LOG_SYS_WARN(err, fmt, ...) \
    __LOG_SYS(err, LEVEL_WARN, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    __LOG(LEVEL_ERROR, fmt, ##__VA_ARGS__)

#define LOG_SYS_ERROR(err, fmt, ...) \
    __LOG_SYS(err, LEVEL_ERROR, fmt, ##__VA_ARGS__)

#define __LOG(level, fmt, ...)                                                                                                    \
    do                                                                                                                            \
    {                                                                                                                             \
        fprintf(stderr, "%s[%s:%d](%s): " fmt "\n", getLogLevel(level), __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
    } while (0)

#define __LOG_SYS(err, level, fmt, ...)                                                                                                                             \
    do                                                                                                                                                              \
    {                                                                                                                                                               \
        fprintf(stderr, "%s[%s:%d](%s): " fmt " (%d - %s)\n", getLogLevel(level), __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__, err, getErrString(err)); \
    } while (0)

inline const char *getLogLevel(int level)
{
    switch (level) {
        case LEVEL_DEBUG:
            return "[DBG]";
        case LEVEL_INFO:
            return "[NFO]";
        case LEVEL_WARN:
            return "[WRN]";
        case LEVEL_ERROR:
            return "[ERR]";
        default:
            return "";
    }
}

inline const char *getErrString(int err)
{
    char errbuf[64];
    const char *errmsg = nullptr;
#ifdef HAVE_GNU_STRERROR_R
    errmsg = strerror_r(err, errbuf, sizeof(errbuf));
#else
    if (strerror_r(err, errbuf, sizeof(errbuf)) != 0)
        errmsg = "Unknown error";
    else
        errmsg = errbuf;
#endif

    return errmsg;
}

#ifdef __cplusplus
}
#endif