#pragma once

#include <cstdint>

namespace sn::hal {

// Monotonic microsecond clock. Implemented with esp_timer on target and by a
// controllable fake in host tests.
class IClock {
public:
    virtual ~IClock() = default;
    [[nodiscard]] virtual std::uint64_t nowUs() const = 0;
    virtual void delayUs(std::uint32_t us) = 0;
};

}  // namespace sn::hal
