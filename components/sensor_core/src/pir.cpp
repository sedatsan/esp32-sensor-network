#include "sn/drivers/pir.hpp"

namespace sn::drivers {

Pir::Pir(hal::IInputPin& out, hal::IClock& clock, const Config& config)
    : out_(out), clock_(clock), config_(config), startUs_(clock.nowUs()) {}

bool Pir::isReady() const {
    return warmedUp_ || clock_.nowUs() - startUs_ >= config_.warmupUs;
}

bool Pir::motionDetected() const {
    return isReady() && out_.read();
}

std::optional<Pir::Event> Pir::poll() {
    if (!isReady()) {
        return std::nullopt;
    }
    warmedUp_ = true;
    const bool level = out_.read();
    if (level == lastLevel_) {
        return std::nullopt;
    }
    lastLevel_ = level;
    return level ? Event::MotionStarted : Event::MotionEnded;
}

}  // namespace sn::drivers
