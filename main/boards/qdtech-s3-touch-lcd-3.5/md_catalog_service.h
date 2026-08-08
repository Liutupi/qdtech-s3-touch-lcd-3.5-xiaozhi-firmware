#pragma once

#include "sdkconfig.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE) && \
    CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE

#include <cstddef>

#include "esp_err.h"

struct MdCatalogEntry {
    char relative_path[192]{};
    char title[96]{};
    char category[32]{};
};

class MdCatalogService {
public:
    static constexpr size_t kMaxEntries = 128;

    MdCatalogService() = default;
    ~MdCatalogService();

    MdCatalogService(const MdCatalogService&) = delete;
    MdCatalogService& operator=(const MdCatalogService&) = delete;

    // Loads an optional catalog.tsv first, then falls back to a bounded scan
    // of /sdcard/roms/md. The entry array exists only in PSRAM while the page
    // is open; this method creates no task or timer.
    esp_err_t Load();
    void Clear();

    size_t Count() const { return count_; }
    bool Truncated() const { return truncated_; }
    const MdCatalogEntry* Entry(size_t index) const;

private:
    bool LoadTsv();
    void ScanDirectory(const char* absolute_dir, const char* relative_dir,
                       const char* category, unsigned depth);
    bool AddEntry(const char* relative_path, const char* title,
                  const char* category);

    MdCatalogEntry* entries_ = nullptr;
    size_t count_ = 0;
    bool truncated_ = false;
};

#endif  // CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE
