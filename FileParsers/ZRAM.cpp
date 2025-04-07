#include "ZRAM.h"
#include "Log.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

ZRAM::ZRAM() {
    FindZRAMDevices();
}

void ZRAM::FindZRAMDevices() {
    m_zramDevices.clear();
    
    DIR* dir = opendir(SYS_ZRAM_PATH);
    if (!dir) {
        LOG_SYS_WARN(errno, "Failed to open directory: %s", SYS_ZRAM_PATH);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr && m_zramDevices.size() < MAX_ZRAM_DEVICES) {
        if (strncmp(entry->d_name, ZRAM_DEVICE_PREFIX, strlen(ZRAM_DEVICE_PREFIX)) == 0) {
            m_zramDevices.push_back(entry->d_name);
            LOG_INFO("Tracking ZRAM device: %s", entry->d_name);
        }
    }

    closedir(dir);
}

std::vector<ZRAMDeviceStats> ZRAM::GetDeviceStats() const {
    std::vector<ZRAMDeviceStats> deviceStats;
    
    for (const auto& device : m_zramDevices) {
        ZRAMDeviceStats stats;
        stats.device_name = device;
        
        if (ReadMMStat(device, stats)) {
            deviceStats.push_back(stats);
            
            LOG_DEBUG("ZRAM device: %s, Data: %lu MB, Compressed: %lu MB, Memory Used: %lu MB",
                     stats.device_name.c_str(), 
                     stats.orig_data_size, 
                     stats.compr_data_size, 
                     stats.mem_used_total);
        }
    }
    
    return deviceStats;
}

bool ZRAM::ReadMMStat(const std::string& device, ZRAMDeviceStats& stats) const {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s/mm_stat", SYS_ZRAM_PATH, device.c_str());
    
    FILE* file = fopen(path, "r");
    if (!file) {
        LOG_SYS_ERROR(errno, "Failed to open file: %s", path);
        return false;
    }
    
    unsigned long orig_data_size, compr_data_size, mem_used_total;
    
    if (fscanf(file, "%lu %lu %lu", &orig_data_size, &compr_data_size, &mem_used_total) != 3) {
        LOG_ERROR("Failed to read mm_stat metrics from file: %s", path);
        fclose(file);
        return false;
    }
    
    fclose(file);
    
    // Convert to MB
    stats.orig_data_size  = BytesToMB(orig_data_size);
    stats.compr_data_size = BytesToMB(compr_data_size);
    stats.mem_used_total  = BytesToMB(mem_used_total);
    
    return true;
}

unsigned long ZRAM::BytesToMB(unsigned long bytes) const {
    return bytes / (1024 * 1024);
}