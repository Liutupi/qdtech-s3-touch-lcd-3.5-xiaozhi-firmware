#include "wooden_fish_tap_detector.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
// Physical traces from this QDTech enclosure with BMI270 at +/-16 g:
// idle noise <= 6 counts, a finger knock ~= 40/24 impulse/deviation with
// gyro ~= 2, and a gentle board shake ~= 45/33 with gyro ~= 49. Require a
// short acceleration edge with little rotation to separate a knock from
// picking up or shaking the product.
constexpr uint16_t kTapImpulseThreshold = 18;
constexpr uint16_t kTapDeviationThreshold = 12;
constexpr uint16_t kTapGyroLimit = 25;
constexpr uint16_t kQuietImpulseThreshold = 8;
constexpr uint16_t kQuietDeviationThreshold = 8;
constexpr int64_t kInitialSettleMs = 180;
constexpr int64_t kQuietToRearmMs = 110;
constexpr int64_t kTapDebounceMs = 300;
}  // namespace

void WoodenFishTapDetector::Arm(int64_t now_ms) {
    Reset();
    quiet_started_ms_ = now_ms;
}

void WoodenFishTapDetector::Reset() {
    gravity_baseline_ = 0;
    previous_accel_x_ = 0;
    previous_accel_y_ = 0;
    previous_accel_z_ = 0;
    last_tap_ms_ = 0;
    quiet_started_ms_ = 0;
    have_previous_sample_ = false;
    armed_ = false;
}

WoodenFishTapDetector::Result WoodenFishTapDetector::Process(const Sample& sample) {
    Result result;
    const int64_t magnitude_sq = static_cast<int64_t>(sample.accel_x) * sample.accel_x +
        static_cast<int64_t>(sample.accel_y) * sample.accel_y +
        static_cast<int64_t>(sample.accel_z) * sample.accel_z;
    const int32_t magnitude = static_cast<int32_t>(std::sqrt(static_cast<double>(magnitude_sq)));
    if (gravity_baseline_ == 0) {
        gravity_baseline_ = magnitude;
    }

    const uint16_t deviation = static_cast<uint16_t>(std::min<int32_t>(65535,
        std::abs(magnitude - gravity_baseline_)));
    result.accel_deviation = deviation;
    result.gyro_peak = static_cast<uint16_t>(std::min<int>(65535,
        std::max({std::abs(static_cast<int>(sample.gyro_x)),
                  std::abs(static_cast<int>(sample.gyro_y)),
                  std::abs(static_cast<int>(sample.gyro_z))})));
    result.impulse_threshold = kTapImpulseThreshold;
    result.deviation_threshold = kTapDeviationThreshold;
    result.gyro_limit = kTapGyroLimit;

    if (!have_previous_sample_) {
        previous_accel_x_ = sample.accel_x;
        previous_accel_y_ = sample.accel_y;
        previous_accel_z_ = sample.accel_z;
        have_previous_sample_ = true;
        quiet_started_ms_ = sample.time_ms;
        return result;
    }

    const int32_t dx = static_cast<int32_t>(sample.accel_x) - previous_accel_x_;
    const int32_t dy = static_cast<int32_t>(sample.accel_y) - previous_accel_y_;
    const int32_t dz = static_cast<int32_t>(sample.accel_z) - previous_accel_z_;
    const int64_t impulse_sq = static_cast<int64_t>(dx) * dx +
        static_cast<int64_t>(dy) * dy + static_cast<int64_t>(dz) * dz;
    result.impulse = static_cast<uint16_t>(std::min<int32_t>(65535,
        static_cast<int32_t>(std::sqrt(static_cast<double>(impulse_sq)))));

    previous_accel_x_ = sample.accel_x;
    previous_accel_y_ = sample.accel_y;
    previous_accel_z_ = sample.accel_z;

    const bool quiet = result.impulse <= kQuietImpulseThreshold &&
                       deviation <= kQuietDeviationThreshold;
    if (quiet) {
        if (quiet_started_ms_ == 0) {
            quiet_started_ms_ = sample.time_ms;
        }
        gravity_baseline_ += (magnitude - gravity_baseline_) / 16;
        const int64_t required_quiet = last_tap_ms_ == 0 ? kInitialSettleMs : kQuietToRearmMs;
        if (sample.time_ms - quiet_started_ms_ >= required_quiet &&
            (last_tap_ms_ == 0 || sample.time_ms - last_tap_ms_ >= kTapDebounceMs)) {
            armed_ = true;
        }
    } else {
        quiet_started_ms_ = 0;
    }

    if (armed_ && result.impulse >= result.impulse_threshold &&
        deviation >= result.deviation_threshold &&
        result.gyro_peak <= result.gyro_limit &&
        (last_tap_ms_ == 0 || sample.time_ms - last_tap_ms_ >= kTapDebounceMs)) {
        result.tapped = true;
        armed_ = false;
        last_tap_ms_ = sample.time_ms;
    }
    result.armed = armed_;
    return result;
}
