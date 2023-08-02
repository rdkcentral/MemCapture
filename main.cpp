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

#include <unistd.h>
#include <getopt.h>
#include <csignal>
#include <fstream>
#include <optional>


#include "Platform.h"
#include "Log.h"
#include "ProcessMetric.h"
#include "MemoryMetric.h"
#include "Metadata.h"
#include "GroupManager.h"

#include "inja/inja.hpp"

#ifdef USE_BREAKPAD
#include "breakpad_wrapper.h"
#endif

#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX g_
#include <incbin.h>

#include "JsonReportGenerator.h"

INCBIN(templateHtml, "./templates/template.html");

static int gDuration = 30;
static Platform gPlatform = Platform::AMLOGIC;

// Default to save in current directory if not specified
static std::filesystem::path gOutputDirectory = std::filesystem::current_path() / "MemCaptureReport";

static bool gJson = false;

bool gEnableGroups = false;
static std::filesystem::path gGroupsFile;

std::condition_variable gStop;
std::mutex gLock;
bool gEarlyTermination = false;

static void displayUsage()
{
    printf("Usage: MemCapture <option(s)>\n");
    printf("    Utility to capture memory statistics\n\n");
    printf("    -h, --help          Print this help and exit\n");
    printf("    -o, --output-dir    Directory to save results in\n");
    printf("    -j, --json          Save data as JSON in addition to HTML report\n");
    printf("    -d, --duration      Amount of time (in seconds) to capture data for. Default 30 seconds\n");
    printf("    -p, --platform      Platform we're running on. Supported options = ['AMLOGIC', 'REALTEK', 'BROADCOM']. Defaults to Amlogic\n");
    printf("    -g, --groups        Path to JSON file containing the group mappings (optional)\n");
}

static void parseArgs(const int argc, char **argv)
{
    struct option longopts[] = {
            {"help",       no_argument,       nullptr, (int) 'h'},
            {"duration",   required_argument, nullptr, (int) 'd'},
            {"platform",   required_argument, nullptr, (int) 'p'},
            {"output-dir", required_argument, nullptr, (int) 'o'},
            {"json",       no_argument, nullptr, (int) 'j'},
            {"groups",     required_argument, nullptr, (int) 'g'},
            {nullptr, 0,                      nullptr, 0}
    };

    opterr = 0;

    int option;
    int longindex;

    while ((option = getopt_long(argc, argv, "hd:p:o:jg:", longopts, &longindex)) != -1) {
        switch (option) {
            case 'h':
                displayUsage();
                exit(EXIT_SUCCESS);
                break;
            case 'd':
                gDuration = std::atoi(optarg);
                if (gDuration < 0) {
                    fprintf(stderr, "Error: duration (s) must be > 0\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'p': {
                std::string platform(optarg);

                if (platform == "AMLOGIC") {
                    gPlatform = Platform::AMLOGIC;
                } else if (platform == "REALTEK") {
                    gPlatform = Platform::REALTEK;
                } else if (platform == "BROADCOM") {
                    gPlatform = Platform::BROADCOM;
                } else {
                    fprintf(stderr, "Warning: Unsupported platform %s\n", platform.c_str());
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'o': {
                gOutputDirectory = std::filesystem::path(optarg);
                break;
            }
            case 'j': {
                gJson = true;
                break;
            }
            case 'g': {
                gEnableGroups = true;
                gGroupsFile = std::filesystem::path(optarg);
                break;
            }
            case '?':
                if (optopt == 'c')
                    fprintf(stderr, "Warning: Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Warning: Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Warning: Unknown option character `\\x%x'.\n", optopt);

                exit(EXIT_FAILURE);
                break;
            default:
                exit(EXIT_FAILURE);
                break;
        }
    }
}

void signalHandler(int signal)
{
    // On SIGTERM, we should stop capturing and save the report
    LOG_INFO("Signal %d (%s) received. Stopping and saving report!", signal, strsignal(signal));
    gEarlyTermination = true;
    gStop.notify_all();
    LOG_INFO("Waiting for in-progress data collection to complete");
}


int main(int argc, char *argv[])
{
    parseArgs(argc, argv);

    // Get start time
    auto start = std::chrono::steady_clock::now();

    // Configure signals to stop and clean up
#ifdef USE_BREAKPAD
    // Breakpad will handle SIGILL, SIGABRT, SIGFPE and SIGSEGV
    LOG_INFO("Breakpad support enabled");
    breakpad_ExceptionHandler();
#endif

    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    // Lower our priority to avoid getting in the way
    if (nice(10) < 0) {
        LOG_WARN("Failed to set nice value");
    }

    try {
        std::filesystem::create_directories(gOutputDirectory);
    } catch (std::filesystem::filesystem_error &e) {
        LOG_ERROR("Failed to create directory %s to save results in: '%s'", gOutputDirectory.string().c_str(),
                  e.what());
        return EXIT_FAILURE;
    }

    LOG_INFO("** About to start memory capture for %d seconds **", gDuration);
    LOG_INFO("Will save report to %s", gOutputDirectory.string().c_str());

    // Load groups JSON if provided
    std::optional<std::shared_ptr<GroupManager>> groupManager = std::nullopt;
    if (gEnableGroups) {
        LOG_INFO("Loading groups from %s", std::filesystem::absolute(gGroupsFile).string().c_str());
        std::ifstream groupsFile(gGroupsFile);
        if (!groupsFile) {
            LOG_ERROR("Invalid groups file %s", gGroupsFile.string().c_str());
            return EXIT_FAILURE;
        } else {
            try {
                auto groupsJson = nlohmann::json::parse(groupsFile);
                groupManager = std::make_shared<GroupManager>(groupsJson);
            } catch (nlohmann::json::exception &e) {
                LOG_ERROR("Failed to parse groups JSON with error %s", e.what());
                return EXIT_FAILURE;
            }
        }
    }

    auto metadata = std::make_shared<Metadata>();
    auto reportGenerator = std::make_shared<JsonReportGenerator>(metadata, groupManager);

    // Create all our metrics
    ProcessMetric processMetric(reportGenerator);
    MemoryMetric memoryMetric(gPlatform, reportGenerator);

    // Start data collection
    processMetric.StartCollection(std::chrono::seconds(3));
    memoryMetric.StartCollection(std::chrono::seconds(3));

    // Block main thread for the collection duration or until SIGTERM
    std::unique_lock<std::mutex> locker(gLock);
    gStop.wait_for(locker, std::chrono::seconds(gDuration));

    if (!gEarlyTermination) {
        LOG_INFO("Stopping after %d seconds - completed full capture", gDuration);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    metadata->SetDuration(duration);

    // Done! Stop data collection
    processMetric.StopCollection();
    memoryMetric.StopCollection();

    // Save results
    processMetric.SaveResults();
    memoryMetric.SaveResults();

    // Build report
    inja::Environment env;
    // Make the output a bit tidier
    env.set_trim_blocks(true);
    env.set_lstrip_blocks(true);
    auto htmlTemplateString = std::string(g_templateHtml_data, g_templateHtml_data + g_templateHtml_size);
    std::string result = env.render(htmlTemplateString, reportGenerator->getJson());

    std::filesystem::path htmlFilepath = gOutputDirectory / "report.html";
    std::ofstream outputHtml(htmlFilepath, std::ios::trunc | std::ios::binary);
    outputHtml << result;

    LOG_INFO("Saved report to %s", htmlFilepath.string().c_str());

    if (gJson) {
        std::filesystem::path jsonFilepath = gOutputDirectory / "report.json";
        std::ofstream outputJson(jsonFilepath, std::ios::trunc | std::ios::binary);
        outputJson << reportGenerator->getJson().dump(4);

        LOG_INFO("Saved JSON data to %s", jsonFilepath.string().c_str());
    }

    return EXIT_SUCCESS;
}
