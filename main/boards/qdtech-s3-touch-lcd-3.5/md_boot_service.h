#pragma once

#include "sdkconfig.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE) && \
    CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE

#include <cstdint>
#include <string>

#include "esp_err.h"

enum class MdLaunchMode : uint8_t {
    Fresh,
    Resume,
};

struct MdLaunchRequest {
    // Accepted forms are "roms/md/game.bin" and
    // "/sdcard/roms/md/game.bin". The handoff always stores the relative form.
    std::string rom_path;
    MdLaunchMode mode = MdLaunchMode::Fresh;
    uint8_t save_slot = 0;
};

class MdBootService {
public:
    static MdBootService& GetInstance();

    // Validates the running/main and emulator partitions, validates the ROM,
    // atomically writes the SD-card handoff, then selects mdemu for next boot.
    // This method does not restart the board and creates no background task.
    esp_err_t PrepareAndSelect(const MdLaunchRequest& request);

    static const char* HandoffPath();

private:
    MdBootService() = default;
};

#endif  // CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE
