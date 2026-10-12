#include "sn/drivers/hcsr04.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

namespace sn::drivers {

std::uint32_t echoUsToMm(std::uint32_t echoUs, float temperatureC) {
    // Speed of sound in air, m/s == mm/ms; halve for the round trip.
    const float speedMmPerUs = (331.3F + 0.606F * temperatureC) / 1000.0F;
    return static_cast<std::uint32_t>(
        std::lround(static_cast<float>(echoUs) * speedMmPerUs / 2.0F));
}

Hcsr04::Hcsr04(hal::IOutputPin& trig, hal::IInputPin& echo, hal::IClock& clock,
               const Config& config)
    : trig_(trig), echo_(echo), clock_(clock), config_(config) {
    trig_.write(false);
}

bool Hcsr04::waitForEcho(bool level, std::uint32_t timeoutUs, std::uint64_t& timestampUs) {
    const std::uint64_t deadlineUs = clock_.nowUs() + timeoutUs;
    while (echo_.read() != level) {
        if (clock_.nowUs() > deadlineUs) {
            return false;
        }
    }
    timestampUs = clock_.nowUs();
    return true;
}

Expected<std::uint16_t, Hcsr04::Error> Hcsr04::measureMm() {
    trig_.write(true);
    clock_.delayUs(config_.triggerPulseUs);
    trig_.write(false);

    std::uint64_t riseUs = 0;
    if (!waitForEcho(true, config_.echoStartTimeoutUs, riseUs)) {
        return unexpected(Error::EchoStartTimeout);
    }
    std::uint64_t fallUs = 0;
    if (!waitForEcho(false, config_.echoEndTimeoutUs, fallUs)) {
        return unexpected(Error::EchoEndTimeout);
    }

    const std::uint32_t mm =
        echoUsToMm(static_cast<std::uint32_t>(fallUs - riseUs), config_.temperatureC);
    if (mm < config_.minDistanceMm || mm > config_.maxDistanceMm) {
        return unexpected(Error::OutOfRange);
    }
    return static_cast<std::uint16_t>(mm);
}

Expected<std::uint16_t, Hcsr04::Error> Hcsr04::measureMedianMm(std::size_t samples) {
    samples = std::clamp<std::size_t>(samples, 1, kMaxSamples);
    std::array<std::uint16_t, kMaxSamples> valid{};
    std::size_t count = 0;
    Error lastError = Error::EchoStartTimeout;

    for (std::size_t i = 0; i < samples; ++i) {
        if (i > 0) {
            clock_.delayUs(kInterMeasurementDelayUs);
        }
        const auto result = measureMm();
        if (result) {
            valid[count++] = *result;
        } else {
            lastError = result.error();
        }
    }

    // Require a majority of good samples, otherwise the median is noise.
    if (count * 2 <= samples) {
        return unexpected(lastError);
    }
    const std::size_t half = count / 2;
    std::nth_element(valid.begin(), std::next(valid.begin(), static_cast<std::ptrdiff_t>(half)),
                     std::next(valid.begin(), static_cast<std::ptrdiff_t>(count)));
    return valid[half];
}

}  // namespace sn::drivers
