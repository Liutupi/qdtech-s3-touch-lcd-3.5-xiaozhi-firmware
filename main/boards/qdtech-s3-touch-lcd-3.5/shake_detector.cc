#include "shake_detector.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr uint16_t kAccelMotionThreshold = 2800;
constexpr uint16_t kGyroMotionThreshold = 750;
constexpr uint16_t kAccelStrongThreshold = 4400;
constexpr uint16_t kGyroStrongThreshold = 1000;
constexpr uint16_t kDirectionDeltaThreshold = 850;
constexpr int64_t kPeakMinSpacingMs = 80;
constexpr int64_t kPeakWindowMs = 800;
constexpr int64_t kMinimumShakeDurationMs = 260;
constexpr int64_t kStillToSettlingMs = 700;
constexpr int64_t kSettlingStableMs = 650;
constexpr int64_t kRevealHoldMs = 120;
constexpr int64_t kCooldownMs = 1400;
constexpr uint8_t kMinimumPeakCount = 4;
constexpr uint8_t kMinimumDirectionReversals = 2;
constexpr uint16_t kIntensityAccelCeiling = 9000;
constexpr uint16_t kIntensityGyroCeiling = 2600;
}  // namespace

void ShakeDetector::Arm(int64_t now_ms) {
    ResetWindow(now_ms);
    state_ = State::ARMED;
}

void ShakeDetector::Reset() {
    state_ = State::IDLE;
    gravity_baseline_ = 0;
    previous_accel_x_ = 0;
    previous_accel_y_ = 0;
    previous_accel_z_ = 0;
    last_direction_ = 0;
    last_peak_ms_ = 0;
    last_motion_ms_ = 0;
    shaking_started_ms_ = 0;
    settling_started_ms_ = 0;
    reveal_started_ms_ = 0;
    peak_write_index_ = 0;
    for (auto& peak_time : peak_times_) {
        peak_time = 0;
    }
    stats_ = {};
}

void ShakeDetector::ResetWindow(int64_t now_ms) {
    last_peak_ms_ = 0;
    last_motion_ms_ = now_ms;
    shaking_started_ms_ = 0;
    settling_started_ms_ = 0;
    reveal_started_ms_ = 0;
    last_direction_ = 0;
    peak_write_index_ = 0;
    for (auto& peak_time : peak_times_) {
        peak_time = 0;
    }
    stats_ = {};
}

void ShakeDetector::RecordPeak(int64_t now_ms) {
    peak_times_[peak_write_index_] = now_ms;
    peak_write_index_ = (peak_write_index_ + 1) % kPeakHistorySize;
    ++stats_.peak_count;
}

uint8_t ShakeDetector::PeaksInWindow(int64_t now_ms) const {
    uint8_t count = 0;
    for (const int64_t peak_time : peak_times_) {
        if (peak_time > 0 && now_ms - peak_time <= kPeakWindowMs) {
            ++count;
        }
    }
    return count;
}

int ShakeDetector::MotionDirection(int16_t x, int16_t y, int16_t z) const {
    const int dx = static_cast<int>(x) - previous_accel_x_;
    const int dy = static_cast<int>(y) - previous_accel_y_;
    const int dz = static_cast<int>(z) - previous_accel_z_;
    const int abs_dx = std::abs(dx);
    const int abs_dy = std::abs(dy);
    const int abs_dz = std::abs(dz);
    const int strongest = std::max({abs_dx, abs_dy, abs_dz});
    if (strongest < kDirectionDeltaThreshold) {
        return 0;
    }
    const int delta = strongest == abs_dx ? dx : (strongest == abs_dy ? dy : dz);
    return delta > 0 ? 1 : -1;
}

uint8_t ShakeDetector::CalculateIntensity(uint16_t accel_deviation, uint16_t gyro_peak) const {
    const uint16_t accel_component = std::min<uint16_t>(50, accel_deviation * 50 / kIntensityAccelCeiling);
    const uint16_t gyro_component = std::min<uint16_t>(50, gyro_peak * 50 / kIntensityGyroCeiling);
    return static_cast<uint8_t>(std::min<uint16_t>(100, accel_component + gyro_component));
}

ShakeDetector::Result ShakeDetector::Process(const Sample& sample) {
    Result result;
    result.state = state_;
    if (state_ == State::IDLE) {
        return result;
    }

    const int64_t magnitude_sq = static_cast<int64_t>(sample.accel_x) * sample.accel_x +
        static_cast<int64_t>(sample.accel_y) * sample.accel_y +
        static_cast<int64_t>(sample.accel_z) * sample.accel_z;
    const int32_t magnitude = static_cast<int32_t>(std::sqrt(static_cast<double>(magnitude_sq)));
    if (gravity_baseline_ == 0) {
        gravity_baseline_ = magnitude;
    }

    const uint16_t accel_deviation = static_cast<uint16_t>(std::min<int32_t>(65535,
        std::abs(magnitude - gravity_baseline_)));
    const uint16_t gyro_peak = static_cast<uint16_t>(std::min<int>(65535,
        std::max({std::abs(static_cast<int>(sample.gyro_x)), std::abs(static_cast<int>(sample.gyro_y)),
                  std::abs(static_cast<int>(sample.gyro_z))})));
    const bool motion = accel_deviation >= kAccelMotionThreshold || gyro_peak >= kGyroMotionThreshold;
    const bool strong_motion = accel_deviation >= kAccelStrongThreshold || gyro_peak >= kGyroStrongThreshold;
    const int direction = MotionDirection(sample.accel_x, sample.accel_y, sample.accel_z);

    if (state_ == State::ARMED && stats_.peak_count > 0 && PeaksInWindow(sample.time_ms) == 0) {
        ResetWindow(sample.time_ms);
    }

    if (state_ == State::ARMED && !motion) {
        gravity_baseline_ += (magnitude - gravity_baseline_) / 16;
    }
    if (motion) {
        last_motion_ms_ = sample.time_ms;
        stats_.max_accel_deviation = std::max(stats_.max_accel_deviation, accel_deviation);
        stats_.max_gyro_peak = std::max(stats_.max_gyro_peak, gyro_peak);
        if (strong_motion && sample.time_ms - last_peak_ms_ >= kPeakMinSpacingMs) {
            RecordPeak(sample.time_ms);
            last_peak_ms_ = sample.time_ms;
        }
        if (direction != 0) {
            if (last_direction_ != 0 && direction != last_direction_) {
                ++stats_.direction_reversals;
            }
            last_direction_ = static_cast<int8_t>(direction);
        }
    }

    previous_accel_x_ = sample.accel_x;
    previous_accel_y_ = sample.accel_y;
    previous_accel_z_ = sample.accel_z;
    result.intensity = CalculateIntensity(accel_deviation, gyro_peak);

    if (state_ == State::ARMED) {
        const uint8_t peaks = PeaksInWindow(sample.time_ms);
        int64_t earliest_peak_ms = sample.time_ms;
        for (const int64_t peak_time : peak_times_) {
            if (peak_time > 0 && sample.time_ms - peak_time <= kPeakWindowMs) {
                earliest_peak_ms = std::min(earliest_peak_ms, peak_time);
            }
        }
        if (peaks >= kMinimumPeakCount && stats_.direction_reversals >= kMinimumDirectionReversals &&
            sample.time_ms - earliest_peak_ms >= kMinimumShakeDurationMs) {
            state_ = State::SHAKING;
            shaking_started_ms_ = sample.time_ms;
            result.transition = Transition::ARMED_TO_SHAKING;
        }
    } else if (state_ == State::SHAKING) {
        stats_.shaking_duration_ms = static_cast<uint32_t>(sample.time_ms - shaking_started_ms_);
        if (sample.time_ms - last_motion_ms_ >= kStillToSettlingMs) {
            state_ = State::SETTLING;
            settling_started_ms_ = sample.time_ms;
            result.transition = Transition::SHAKING_TO_SETTLING;
        }
    } else if (state_ == State::SETTLING) {
        if (motion) {
            state_ = State::SHAKING;
            last_motion_ms_ = sample.time_ms;
        } else if (sample.time_ms - settling_started_ms_ >= kSettlingStableMs) {
            state_ = State::REVEAL;
            reveal_started_ms_ = sample.time_ms;
            result.transition = Transition::SETTLING_TO_REVEAL;
        }
    } else if (state_ == State::REVEAL) {
        if (sample.time_ms - reveal_started_ms_ >= kRevealHoldMs) {
            state_ = State::COOLDOWN;
        }
    } else if (state_ == State::COOLDOWN && sample.time_ms - reveal_started_ms_ >= kCooldownMs) {
        state_ = State::ARMED;
        ResetWindow(sample.time_ms);
        result.transition = Transition::COOLDOWN_COMPLETE;
    }

    result.state = state_;
    result.stats = stats_;
    return result;
}
