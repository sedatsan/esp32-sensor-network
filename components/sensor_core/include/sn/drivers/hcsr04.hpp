#pragma once

#include <cstddef>
#include <cstdint>

#include "sn/expected.hpp"
#include "sn/hal/clock.hpp"
#include "sn/hal/gpio.hpp"

namespace sn::drivers {

// Driver for the HC-SR04 ultrasonic ranger.
//
// Protocol: a >=10 us high pulse on TRIG starts a measurement; the module
// then drives ECHO high for the round-trip time of flight. ECHO is a 5 V
// signal and must go through a divider before reaching an ESP32 pin.
class Hcsr04 {
public:
    enum class Error : std::uint8_t {
        EchoStartTimeout,  // ECHO never went high: sensor missing or unpowered
        EchoEndTimeout,    // ECHO stayed high: no obstacle within range
        OutOfRange,        // Measured distance outside the datasheet range
    };

    struct Config {
        std::uint32_t triggerPulseUs = 10;
        std::uint32_t echoStartTimeoutUs = 5'000;
        std::uint32_t echoEndTimeoutUs = 30'000;  // ~5 m round trip
        std::uint16_t minDistanceMm = 20;
        std::uint16_t maxDistanceMm = 4'000;
        float temperatureC = 20.0F;  // speed of sound compensation
    };

    Hcsr04(hal::IOutputPin& trig, hal::IInputPin& echo, hal::IClock& clock, const Config& config);
    Hcsr04(hal::IOutputPin& trig, hal::IInputPin& echo, hal::IClock& clock)
        : Hcsr04(trig, echo, clock, Config{}) {}

    // Single blocking measurement (bounded by the configured timeouts).
    [[nodiscard]] Expected<std::uint16_t, Error> measureMm();

    // Takes up to `samples` readings and returns the median of the valid ones,
    // rejecting the outliers the HC-SR04 is known for.
    [[nodiscard]] Expected<std::uint16_t, Error> measureMedianMm(std::size_t samples);

    void setTemperature(float celsius) { config_.temperatureC = celsius; }

    static constexpr std::size_t kMaxSamples = 9;
    // Datasheet recommends >=60 ms between triggers to let echoes die out.
    static constexpr std::uint32_t kInterMeasurementDelayUs = 60'000;

private:
    bool waitForEcho(bool level, std::uint32_t timeoutUs, std::uint64_t& timestampUs);

    hal::IOutputPin& trig_;
    hal::IInputPin& echo_;
    hal::IClock& clock_;
    Config config_;
};

// Pure conversion, exposed for testing: round-trip echo time to millimetres.
[[nodiscard]] std::uint32_t echoUsToMm(std::uint32_t echoUs, float temperatureC);

}  // namespace sn::drivers
