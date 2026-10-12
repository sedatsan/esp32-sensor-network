#pragma once

#include <cstdint>
#include <span>

#include "sn/protocol/report.hpp"

namespace sn::app {

enum class WakeCause : std::uint8_t { ColdBoot, Timer, Motion };

// What the controller needs from the outside world. Each is a narrow
// interface so the wake cycle can be unit-tested on the host.
class ISensors {
public:
    virtual ~ISensors() = default;
    [[nodiscard]] virtual bool motion() = 0;
    // Returns 0 when no valid distance could be measured.
    [[nodiscard]] virtual std::uint16_t distanceMm() = 0;
    [[nodiscard]] virtual std::uint16_t batteryMv() = 0;
};

class ITransport {
public:
    virtual ~ITransport() = default;
    // Returns true once the link layer confirmed delivery.
    [[nodiscard]] virtual bool send(std::span<const std::uint8_t> payload) = 0;
    // Moves the radio to another Wi-Fi channel. The gateway follows its
    // router's channel, so nodes have to find it.
    virtual void setChannel(std::uint8_t channel) = 0;
};

class IClockDelay {
public:
    virtual ~IClockDelay() = default;
    virtual void delayMs(std::uint32_t ms) = 0;
};

struct NodeConfig {
    std::uint32_t nodeId = 0;
    std::uint32_t reportIntervalS = 300;
    std::uint8_t maxSendAttempts = 3;
    std::uint32_t retryBaseDelayMs = 20;  // doubled on each retry
    std::uint32_t pirWarmupS = 60;        // HC-SR501 output is unreliable until then
    std::uint8_t defaultChannel = 1;
    std::uint8_t maxChannel = 13;  // 11 in the US/Canada
};

// State that must survive deep sleep (kept in RTC memory on target).
struct RetainedState {
    std::uint32_t sequence = 0;
    std::uint32_t failedReports = 0;
    std::uint8_t channel = 0;  // last channel the gateway answered on, 0 = unknown
};

struct SleepPlan {
    std::uint32_t timerWakeS;
    bool wakeOnMotion;  // false while the PIR output is still high
};

// One wake cycle of a sensor node: sample, report, decide how to sleep.
class NodeController {
public:
    NodeController(ISensors& sensors, ITransport& transport, IClockDelay& delay,
                   const NodeConfig& config)
        : sensors_(sensors), transport_(transport), delay_(delay), config_(config) {}

    [[nodiscard]] SleepPlan runCycle(WakeCause cause, RetainedState& state);

private:
    bool sendWithRetry(std::span<const std::uint8_t> payload);
    bool scanForGateway(std::span<const std::uint8_t> payload, RetainedState& state);

    ISensors& sensors_;
    ITransport& transport_;
    IClockDelay& delay_;
    NodeConfig config_;
};

}  // namespace sn::app
