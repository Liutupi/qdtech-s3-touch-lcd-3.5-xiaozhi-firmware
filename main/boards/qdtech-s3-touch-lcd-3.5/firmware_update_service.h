#pragma once

#include "desktop_ui.h"

#include <atomic>
#include <mutex>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class FirmwareUpdateService {
public:
    static FirmwareUpdateService& GetInstance();

    void Start(DesktopUI* ui);
    void HandleButton();

private:
    enum class TaskAction {
        Check,
        Upgrade,
    };

    struct TaskArg {
        FirmwareUpdateService* service;
        TaskAction action;
    };

    DesktopUI* desktop_ui_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    std::atomic<bool> busy_{false};
    bool update_available_ = false;
    std::string latest_version_;
    std::string firmware_url_;
    std::mutex state_mutex_;

    void StartTask(TaskAction action);
    static void TaskWrapper(void* arg);
    void Task(TaskAction action);
    void CheckLatest();
    bool FetchLatestRelease(std::string* version, std::string* firmware_url);
    void RunUpgrade();
    void SetUi(const char* status, bool update_available, bool busy, int progress = -1);

    static bool IsNewerVersion(const std::string& current, const std::string& latest);
};
