#pragma once

#include <cstdint>

class ShakeDetector {
public:
    enum class State : uint8_t {
        IDLE,
        ARMED,
        SHAKING,
        SETTLING,
        REVEAL,
        COOLDOWN,
    };

    enum class Transition : uint8_t {
        NONE,
        ARMED_TO_SHAKING,
        SHAKING_TO_SETTLING,
        SETTLING_TO_REVEAL,
        COOLDOWN_COMPLETE,
    };

    struct Sample {
        int16_t accel_x;
        int16_t accel_y;
        int16_t accel_z;
        int16_t gyro_x;
        int16_t gyro_y;
        int16_t gyro_z;
        int64_t time_ms;
    };

    struct Stats {
        uint16_t peak_count = 0;
        uint16_t direction_reversals = 0;
        uint16_t max_accel_deviation = 0;
        uint16_t max_gyro_peak = 0;
        uint32_t shaking_duration_ms = 0;
    };

    struct Result {
        State state = State::IDLE;
        Transition transition = Transition::NONE;
        uint8_t intensity = 0;
        Stats stats = {};
    };

    void Arm(int64_t now_ms);
    void Reset();
    Result Process(const Sample& sample);
    State state() const { return state_; }

private:
    static constexpr uint8_t kPeakHistorySize = 10;

    void ResetWindow(int64_t now_ms);
    void RecordPeak(int64_t now_ms);
    uint8_t PeaksInWindow(int64_t now_ms) const;
    int MotionDirection(int16_t x, int16_t y, int16_t z) const;
    uint8_t CalculateIntensity(uint16_t accel_deviation, uint16_t gyro_peak) const;

    State state_ = State::IDLE;
    int32_t gravity_baseline_ = 0;
    int16_t previous_accel_x_ = 0;
    int16_t previous_accel_y_ = 0;
    int16_t previous_accel_z_ = 0;
    int8_t last_direction_ = 0;
    int64_t last_peak_ms_ = 0;
    int64_t last_motion_ms_ = 0;
    int64_t shaking_started_ms_ = 0;
    int64_t settling_started_ms_ = 0;
    int64_t reveal_started_ms_ = 0;
    int64_t peak_times_[kPeakHistorySize] = {};
    uint8_t peak_write_index_ = 0;
    Stats stats_ = {};
};
