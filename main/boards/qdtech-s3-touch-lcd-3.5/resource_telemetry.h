#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <sdkconfig.h>

struct QdResourceSnapshot {
    size_t internal_free_current = 0;
    size_t internal_free_min = 0;
    size_t internal_largest_block = 0;
    size_t psram_free_current = 0;
    size_t psram_largest_block = 0;
    UBaseType_t task_count = 0;
};

static inline QdResourceSnapshot qd_take_resource_snapshot() {
#if CONFIG_QDTECH_EXPERIMENT_RESOURCE_TELEMETRY
    constexpr uint32_t kInternalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    return {
        heap_caps_get_free_size(kInternalCaps),
        heap_caps_get_minimum_free_size(kInternalCaps),
        heap_caps_get_largest_free_block(kInternalCaps),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        uxTaskGetNumberOfTasks(),
    };
#else
    return {};
#endif
}
