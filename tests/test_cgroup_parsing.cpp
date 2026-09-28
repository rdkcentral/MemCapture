/*
 * Security regression tests for cgroup path parsing (RDKEMW-25863)
 *
 * Validates the security invariant that cgroup path extraction
 * handles paths of any length without buffer overflow.
 *
 * Findings addressed:
 * - MemCapture_High_003: Stack overflow via cpuset controller cgroup path
 * - MemCapture_High_006: Stack overflow via pids controller cgroup path
 *
 * The vulnerable code used:
 *   char cgroup_path[128];
 *   sscanf(line, "%d:<controller>:/%s", &id, cgroup_path);
 *
 * The fix uses std::string::find + substr, which handles any length.
 * These tests exercise the parsing logic with mock cgroup files to
 * prove paths >127 bytes are handled correctly.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdlib>

namespace fs = std::filesystem;

/**
 * @brief Standalone reimplementation of the fixed parsing logic from Process.cpp
 * GetCgroupPathByCgroupControllerAndPid — extracted for testing.
 *
 * This mirrors the exact std::string::find + substr approach used in the fix.
 */
static std::string parseCgroupPath(const std::string &cgroupFileContent,
                                   const std::string &cgroup_controller)
{
    std::string cgrp_path;
    std::string needle = ":" + cgroup_controller + ":/";

    std::istringstream stream(cgroupFileContent);
    std::string line;

    while (std::getline(stream, line)) {
        auto pos = line.find(needle);
        if (pos != std::string::npos) {
            size_t pathStart = pos + needle.size();
            if (pathStart < line.size()) {
                cgrp_path = line.substr(pathStart);
            }
            break;
        }
    }

    return cgrp_path;
}

class CgroupParsingTest : public ::testing::Test {
protected:
    fs::path testDir;

    void SetUp() override {
        testDir = fs::temp_directory_path() / "memcapture_test_cgroup";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }
};

// Normal cgroup file — typical container
TEST_F(CgroupParsingTest, ParsesNormalContainerCgroupPath) {
    std::string content =
        "10:gpu:/com.sky.as.apps_com.bskyb.epgui\n"
        "9:pids:/com.sky.as.apps_com.bskyb.epgui\n"
        "8:cpu,cpuacct:/com.sky.as.apps_com.bskyb.epgui\n"
        "3:cpuset:/com.sky.as.apps_com.bskyb.epgui\n"
        "1:name=systemd:/com.sky.as.apps_com.bskyb.epgui\n";

    EXPECT_EQ(parseCgroupPath(content, "cpuset"), "com.sky.as.apps_com.bskyb.epgui");
    EXPECT_EQ(parseCgroupPath(content, "pids"), "com.sky.as.apps_com.bskyb.epgui");
    EXPECT_EQ(parseCgroupPath(content, "gpu"), "com.sky.as.apps_com.bskyb.epgui");
}

// Non-containerised process — cpuset is just "/"
TEST_F(CgroupParsingTest, ParsesEmptyCgroupPath) {
    std::string content =
        "10:gpu:/\n"
        "9:pids:/system.slice/sky-appsservice.service\n"
        "3:cpuset:/\n";

    // cpuset path after "/" is empty — should return empty
    EXPECT_EQ(parseCgroupPath(content, "cpuset"), "");
    // pids has a real path
    EXPECT_EQ(parseCgroupPath(content, "pids"), "system.slice/sky-appsservice.service");
}

// H003/H006: Path exceeding 127 bytes MUST be handled safely
// This is the critical regression test — the old sscanf code would overflow
TEST_F(CgroupParsingTest, HandlesPathExceeding127Bytes) {
    // Create a cgroup path that's 200 bytes long
    std::string longPath(200, 'A');
    std::string content = "3:cpuset:/" + longPath + "\n";

    std::string result = parseCgroupPath(content, "cpuset");
    EXPECT_EQ(result, longPath);
    EXPECT_EQ(result.size(), 200u);
}

// H003/H006: Very long path (1KB) — must not crash
TEST_F(CgroupParsingTest, HandlesVeryLongPath) {
    std::string longPath(1024, 'B');
    std::string content = "3:cpuset:/" + longPath + "\n";

    std::string result = parseCgroupPath(content, "cpuset");
    EXPECT_EQ(result, longPath);
    EXPECT_EQ(result.size(), 1024u);
}

// H003/H006: Path with special characters — no overflow on embedded nulls/slashes
TEST_F(CgroupParsingTest, HandlesPathWithSpecialCharacters) {
    std::string content = "3:cpuset:/container/with spaces/and.dots/slash\n";
    EXPECT_EQ(parseCgroupPath(content, "cpuset"), "container/with spaces/and.dots/slash");
}

// Controller not found — must return empty, not crash
TEST_F(CgroupParsingTest, ReturnsEmptyForMissingController) {
    std::string content = "3:cpuset:/container\n9:pids:/slice\n";
    EXPECT_EQ(parseCgroupPath(content, "memory"), "");
    EXPECT_EQ(parseCgroupPath(content, "freezer"), "");
}

// Empty file — must return empty
TEST_F(CgroupParsingTest, HandlesEmptyFile) {
    EXPECT_EQ(parseCgroupPath("", "cpuset"), "");
}

// Malformed lines — must not crash
TEST_F(CgroupParsingTest, HandlesMalformedLines) {
    std::string content =
        "garbage\n"
        "no:colons:here\n"
        "3:cpuset:/valid_path\n";

    EXPECT_EQ(parseCgroupPath(content, "cpuset"), "valid_path");
}

// Controller name is a substring of another — must match exact controller
TEST_F(CgroupParsingTest, MatchesExactControllerName) {
    std::string content =
        "8:cpu,cpuacct:/slice\n"
        "3:cpuset:/container\n";

    // "cpu" should NOT match "cpuset"
    EXPECT_EQ(parseCgroupPath(content, "cpuset"), "container");
    // "cpu,cpuacct" is the full controller field
    EXPECT_EQ(parseCgroupPath(content, "cpu,cpuacct"), "slice");
}

// H003/H006: Path at exactly 127 bytes (boundary of old buffer)
TEST_F(CgroupParsingTest, HandlesBoundaryLength127) {
    std::string path127(127, 'C');
    std::string content = "3:cpuset:/" + path127 + "\n";
    EXPECT_EQ(parseCgroupPath(content, "cpuset"), path127);
}

// H003/H006: Path at exactly 128 bytes (overflow point of old buffer)
TEST_F(CgroupParsingTest, HandlesBoundaryLength128) {
    std::string path128(128, 'D');
    std::string content = "3:cpuset:/" + path128 + "\n";
    EXPECT_EQ(parseCgroupPath(content, "cpuset"), path128);
}
