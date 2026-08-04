#include "wooden_fish_tap_detector.h"

#include <cassert>
#include <cstdint>

namespace {
WoodenFishTapDetector::Result Feed(WoodenFishTapDetector& detector, int64_t time_ms,
                                   int16_t z, int16_t gyro = 0) {
    return detector.Process({0, 0, z, gyro, 0, 0, time_ms});
}
}  // namespace

int main() {
    WoodenFishTapDetector detector;
    detector.Arm(0);
    for (int64_t time_ms = 0; time_ms <= 240; time_ms += 30) {
        assert(!Feed(detector, time_ms, 8192).tapped);
    }

    const auto first = Feed(detector, 270, 14000, 10);
    assert(first.tapped);
    assert(first.impulse >= first.impulse_threshold);

    // Mechanical ringing from the same knock must not count again.
    assert(!Feed(detector, 300, 7900, 300).tapped);
    assert(!Feed(detector, 330, 12500, 500).tapped);
    for (int64_t time_ms = 360; time_ms <= 690; time_ms += 30) {
        assert(!Feed(detector, time_ms, 8192).tapped);
    }

    const auto second = Feed(detector, 720, 13900, 10);
    assert(second.tapped);

    detector.Reset();
    detector.Arm(1000);
    for (int64_t time_ms = 1000; time_ms <= 1420; time_ms += 30) {
        const int16_t gentle = static_cast<int16_t>(8192 + ((time_ms / 30) % 2 ? 500 : -500));
        assert(!Feed(detector, time_ms, gentle, 900).tapped);
    }

    // QDTech uses the BMI270 +/-16 g range: gravity is about 2048 counts.
    // A short enclosure knock must register, but sensor noise and gentle
    // movement must remain below the adaptive thresholds.
    detector.Reset();
    detector.Arm(2000);
    for (int64_t time_ms = 2000; time_ms <= 2240; time_ms += 10) {
        const int16_t noise = static_cast<int16_t>(2048 + ((time_ms / 10) % 2 ? 2 : -2));
        assert(!Feed(detector, time_ms, noise, 20).tapped);
    }
    const auto light_knock = Feed(detector, 2250, 2750, 10);
    assert(light_knock.tapped);
    assert(light_knock.impulse_threshold == 18);
    assert(light_knock.deviation_threshold == 12);

    // Ringing within the debounce window is a continuation of the same hit.
    assert(!Feed(detector, 2260, 1850, 220).tapped);
    assert(!Feed(detector, 2280, 2450, 160).tapped);

    // Match the physical trace: a low-rotation enclosure knock is accepted.
    detector.Reset();
    detector.Arm(3000);
    for (int64_t time_ms = 3000; time_ms <= 3240; time_ms += 10) {
        assert(!Feed(detector, time_ms, 2048 + (time_ms % 3), 2).tapped);
    }
    assert(Feed(detector, 3250, 2088, 2).tapped);

    // A similarly sized acceleration while the board rotates is a shake,
    // not a wooden-fish knock.
    detector.Reset();
    detector.Arm(4000);
    for (int64_t time_ms = 4000; time_ms <= 4240; time_ms += 10) {
        assert(!Feed(detector, time_ms, 2048 + (time_ms % 3), 2).tapped);
    }
    assert(!Feed(detector, 4250, 2093, 49).tapped);
    return 0;
}
