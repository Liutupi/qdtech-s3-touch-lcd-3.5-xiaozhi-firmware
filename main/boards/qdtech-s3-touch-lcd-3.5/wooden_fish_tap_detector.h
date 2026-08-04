#pragma once

#include <cstdint>

class WoodenFishTapDetector {
public:
    struct Sample {
        int16_t accel_x;
        int16_t accel_y;
        int16_t accel_z;
        int16_t gyro_x;
        int16_t gyro_y;
        int16_t gyro_z;
        int64_t time_ms;
    };

    struct Result {
        bool tapped = false;
        bool armed = false;
        uint16_t impulse = 0;
        uint16_t accel_deviation = 0;
        uint16_t gyro_peak = 0;
        uint16_t impulse_threshold = 0;
        uint16_t deviation_threshold = 0;
        uint16_t gyro_limit = 0;
    };

    void Arm(int64_t now_ms);
    void Reset();
    Result Process(const Sample& sample);

private:
    int32_t gravity_baseline_ = 0;
    int16_t previous_accel_x_ = 0;
    int16_t previous_accel_y_ = 0;
    int16_t previous_accel_z_ = 0;
    int64_t last_tap_ms_ = 0;
    int64_t quiet_started_ms_ = 0;
    bool have_previous_sample_ = false;
    bool armed_ = false;
};
