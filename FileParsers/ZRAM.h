#pragma once

#include <string>
#include <vector>

// Structure to store ZRAM device statistics
struct ZRAMDeviceStats {
    std::string device_name;
    unsigned long orig_data_size;  // Original data size in MB
    unsigned long compr_data_size; // Compressed data size in MB
    unsigned long mem_used_total;  // Total memory used in MB
};

class ZRAM {
public:
    ZRAM();

    bool hasZRAM() { return m_zramDevices.size() > 0; }

    std::vector<ZRAMDeviceStats> GetDeviceStats() const;

private:
    static constexpr const char* SYS_ZRAM_PATH      = "/sys/block/";
    static constexpr const char* ZRAM_DEVICE_PREFIX = "zram";
    static constexpr int MAX_ZRAM_DEVICES           = 4;

    // Collects ZRAM device paths
    void FindZRAMDevices();

    // Read stats from mm_stat file for a specific device
    bool ReadMMStat(const std::string& device, ZRAMDeviceStats& stats) const;

    // Convert bytes to megabytes
    unsigned long BytesToMB(unsigned long bytes) const;

    std::vector<std::string> m_zramDevices;
};