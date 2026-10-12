#pragma once

#include <cstdint>

namespace sn::app {

// Fires once when a button has been held for `holdMs`; it must be released
// before it can fire again. Fed from a polling loop, so no ISR debouncing.
class LongPressDetector {
public:
    explicit constexpr LongPressDetector(std::uint32_t holdMs) : holdMs_(holdMs) {}

    // Returns true exactly once per long press.
    constexpr bool update(bool pressed, std::uint32_t nowMs) {
        if (!pressed) {
            held_ = false;
            fired_ = false;
            return false;
        }
        if (!held_) {
            held_ = true;
            pressedAtMs_ = nowMs;
        }
        if (!fired_ && nowMs - pressedAtMs_ >= holdMs_) {
            fired_ = true;
            return true;
        }
        return false;
    }

private:
    std::uint32_t holdMs_;
    std::uint32_t pressedAtMs_ = 0;
    bool held_ = false;
    bool fired_ = false;
};

}  // namespace sn::app
