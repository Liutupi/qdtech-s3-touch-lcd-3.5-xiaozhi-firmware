#include "md_boot_service.h"

#if defined(CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE) && \
    CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

namespace {

constexpr char kTag[] = "MdBootService";
constexpr char kSdMount[] = "/sdcard";
constexpr char kRomPrefix[] = "roms/md/";
constexpr char kHandoffDirectory[] = "/sdcard/retro-go/config";
constexpr char kHandoffPath[] = "/sdcard/retro-go/config/md_handoff.json";
constexpr char kHandoffNewPath[] = "/sdcard/retro-go/config/md_handoff.json.new";
constexpr char kHandoffBackupPath[] = "/sdcard/retro-go/config/md_handoff.json.bak";
constexpr char kEmulatorLabel[] = "mdemu";
constexpr uint32_t kEmulatorAddress = 0xF00000;
constexpr uint32_t kEmulatorSize = 0x100000;
constexpr uint8_t kMaxSaveSlot = 3;
// Retro-Go uses RG_PATH_MAX=255. Its savestate path replaces "/sd/roms/"
// with "/sd/retro-go/saves/" and adds the slot plus ".sav", so 235 is
// the largest relative ROM path that remains safe in every derived path.
constexpr size_t kMaxRelativeRomPath = 235;

bool IsDirectory(const char* path) {
    struct stat info = {};
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

esp_err_t EnsureDirectory(const char* path) {
    if (IsDirectory(path)) {
        return ESP_OK;
    }
    if (mkdir(path, 0775) == 0 || errno == EEXIST) {
        return IsDirectory(path) ? ESP_OK : ESP_FAIL;
    }
    ESP_LOGE(kTag, "mkdir failed path=%s errno=%d", path, errno);
    return ESP_FAIL;
}

bool HasSupportedExtension(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }
    std::string extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".md" || extension == ".gen" || extension == ".bin" ||
           extension == ".smd" || extension == ".zip";
}

esp_err_t NormalizeAndValidateRom(const std::string& requested,
                                  std::string* relative_path) {
    if (!relative_path || requested.empty()) {
        return ESP_ERR_INVALID_ARG;
    }

    std::string path = requested;
    constexpr char kMountedPrefix[] = "/sdcard/";
    if (path.rfind(kMountedPrefix, 0) == 0) {
        path.erase(0, sizeof(kMountedPrefix) - 1);
    }

    if (path.size() > kMaxRelativeRomPath || path.rfind(kRomPrefix, 0) != 0 ||
        path.size() <= sizeof(kRomPrefix) - 1 || path.front() == '/' ||
        path.find('\\') != std::string::npos || path.find(':') != std::string::npos ||
        !HasSupportedExtension(path)) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t component_start = 0;
    while (component_start <= path.size()) {
        const size_t slash = path.find('/', component_start);
        const size_t component_end = slash == std::string::npos ? path.size() : slash;
        const std::string component = path.substr(component_start,
                                                  component_end - component_start);
        if (component.empty() || component == "." || component == "..") {
            return ESP_ERR_INVALID_ARG;
        }
        if (slash == std::string::npos) {
            break;
        }
        component_start = slash + 1;
    }

    const std::string full_path = std::string(kSdMount) + "/" + path;
    struct stat info = {};
    if (stat(full_path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        ESP_LOGW(kTag, "ROM unavailable path=%s errno=%d", full_path.c_str(), errno);
        return ESP_ERR_NOT_FOUND;
    }

    *relative_path = std::move(path);
    return ESP_OK;
}

esp_err_t ValidatePartitions(const esp_partition_t** emulator,
                             std::string* return_partition) {
    if (!emulator || !return_partition) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running || (strcmp(running->label, "ota_0") != 0 &&
                     strcmp(running->label, "ota_1") != 0)) {
        ESP_LOGE(kTag, "launch refused: main firmware is not running from ota_0/ota_1");
        return ESP_ERR_INVALID_STATE;
    }

    const esp_partition_t* target = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_2, kEmulatorLabel);
    if (!target) {
        ESP_LOGE(kTag, "launch refused: mdemu partition missing");
        return ESP_ERR_NOT_FOUND;
    }
    if (target->address != kEmulatorAddress || target->size != kEmulatorSize) {
        ESP_LOGE(kTag, "launch refused: mdemu layout mismatch address=0x%lx size=0x%lx",
                 static_cast<unsigned long>(target->address),
                 static_cast<unsigned long>(target->size));
        return ESP_ERR_INVALID_SIZE;
    }

    esp_app_desc_t description = {};
    const esp_err_t description_result =
        esp_ota_get_partition_description(target, &description);
    if (description_result != ESP_OK) {
        ESP_LOGE(kTag, "launch refused: invalid mdemu image descriptor err=%s",
                 esp_err_to_name(description_result));
        return description_result;
    }

    *emulator = target;
    *return_partition = running->label;
    ESP_LOGI(kTag, "validated mdemu project=%s version=%s return=%s",
             description.project_name, description.version, running->label);
    return ESP_OK;
}

esp_err_t WriteAllAndSync(FILE* file, const char* data, size_t size) {
    if (!file || !data || fwrite(data, 1, size, file) != size || fflush(file) != 0 ||
        fsync(fileno(file)) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t WriteHandoff(const std::string& relative_rom,
                       const std::string& return_partition,
                       MdLaunchMode mode,
                       uint8_t save_slot) {
    if (!IsDirectory(kSdMount)) {
        ESP_LOGE(kTag, "SD card is not mounted at %s", kSdMount);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = EnsureDirectory("/sdcard/retro-go");
    if (result != ESP_OK) {
        return result;
    }
    result = EnsureDirectory(kHandoffDirectory);
    if (result != ESP_OK) {
        return result;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    const bool json_ok =
        cJSON_AddNumberToObject(root, "schema", 1) != nullptr &&
        cJSON_AddStringToObject(root, "target", kEmulatorLabel) != nullptr &&
        cJSON_AddStringToObject(root, "rom", relative_rom.c_str()) != nullptr &&
        cJSON_AddStringToObject(root, "return_partition",
                                return_partition.c_str()) != nullptr &&
        cJSON_AddStringToObject(root, "launch_mode",
                                mode == MdLaunchMode::Resume ? "resume" : "fresh") != nullptr &&
        cJSON_AddNumberToObject(root, "save_slot", save_slot) != nullptr;
    if (!json_ok) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }

    FILE* file = fopen(kHandoffNewPath, "wb");
    if (!file) {
        cJSON_free(json);
        ESP_LOGE(kTag, "handoff open failed errno=%d", errno);
        return ESP_FAIL;
    }
    result = WriteAllAndSync(file, json, strlen(json));
    cJSON_free(json);
    if (fclose(file) != 0 && result == ESP_OK) {
        result = ESP_FAIL;
    }
    if (result != ESP_OK) {
        remove(kHandoffNewPath);
        ESP_LOGE(kTag, "handoff write failed errno=%d", errno);
        return result;
    }

    struct stat previous_info = {};
    const bool had_previous = stat(kHandoffPath, &previous_info) == 0;
    if (had_previous) {
        if (remove(kHandoffBackupPath) != 0 && errno != ENOENT) {
            remove(kHandoffNewPath);
            ESP_LOGE(kTag, "handoff backup cleanup failed errno=%d", errno);
            return ESP_FAIL;
        }
        if (rename(kHandoffPath, kHandoffBackupPath) != 0) {
            remove(kHandoffNewPath);
            ESP_LOGE(kTag, "handoff backup failed errno=%d", errno);
            return ESP_FAIL;
        }
    }
    if (rename(kHandoffNewPath, kHandoffPath) != 0) {
        const int rename_errno = errno;
        if (had_previous) {
            rename(kHandoffBackupPath, kHandoffPath);
        }
        remove(kHandoffNewPath);
        ESP_LOGE(kTag, "handoff commit failed errno=%d", rename_errno);
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "handoff committed rom=%s mode=%s slot=%u return=%s",
             relative_rom.c_str(), mode == MdLaunchMode::Resume ? "resume" : "fresh",
             static_cast<unsigned>(save_slot), return_partition.c_str());
    return ESP_OK;
}

}  // namespace

MdBootService& MdBootService::GetInstance() {
    static MdBootService service;
    return service;
}

const char* MdBootService::HandoffPath() {
    return kHandoffPath;
}

esp_err_t MdBootService::PrepareAndSelect(const MdLaunchRequest& request) {
    if (request.save_slot > kMaxSaveSlot) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t* emulator = nullptr;
    std::string return_partition;
    esp_err_t result = ValidatePartitions(&emulator, &return_partition);
    if (result != ESP_OK) {
        return result;
    }

    std::string relative_rom;
    result = NormalizeAndValidateRom(request.rom_path, &relative_rom);
    if (result != ESP_OK) {
        return result;
    }

    result = WriteHandoff(relative_rom, return_partition, request.mode,
                          request.save_slot);
    if (result != ESP_OK) {
        return result;
    }

    // ESP-IDF validates the complete application image before changing boot
    // selection. The exact return OTA slot is already durable in the handoff.
    result = esp_ota_set_boot_partition(emulator);
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "failed to select mdemu err=%s", esp_err_to_name(result));
        return result;
    }

    ESP_LOGI(kTag, "mdemu selected for next boot; caller must reboot normally");
    return ESP_OK;
}

#endif  // CONFIG_QDTECH_EXPERIMENT_MD_DUAL_MODE
