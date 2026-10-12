#include "sn/app/node_controller.hpp"

#include <algorithm>

namespace sn::app {
namespace {

constexpr protocol::ReportFlags flagIf(bool condition, protocol::ReportFlags flag) {
    return condition ? flag : protocol::ReportFlags::None;
}

}  // namespace

SleepPlan NodeController::runCycle(WakeCause cause, RetainedState& state) {
    const bool motion = sensors_.motion();
    const std::uint16_t distance = sensors_.distanceMm();

    protocol::SensorReport report;
    report.nodeId = config_.nodeId;
    report.sequence = state.sequence++;
    report.distanceMm = distance;
    report.batteryMv = sensors_.batteryMv();
    report.flags = flagIf(motion, protocol::ReportFlags::Motion) |
                   flagIf(distance != 0, protocol::ReportFlags::DistanceValid) |
                   flagIf(cause == WakeCause::ColdBoot, protocol::ReportFlags::ColdBoot);

    if (state.channel == 0) {
        state.channel = config_.defaultChannel;
    }
    transport_.setChannel(state.channel);

    const auto payload = protocol::encode(report);
    if (!sendWithRetry(payload) && !scanForGateway(payload, state)) {
        ++state.failedReports;
    }

    // After power-up the PIR needs to settle before its output can be
    // trusted, so the first sleep is a short timer-only one.
    if (cause == WakeCause::ColdBoot) {
        return SleepPlan{std::min(config_.reportIntervalS, config_.pirWarmupS), false};
    }

    // If the PIR output is still high, a level-triggered wake would fire
    // immediately; rely on the timer until motion has ended.
    return SleepPlan{config_.reportIntervalS, !motion};
}

bool NodeController::scanForGateway(std::span<const std::uint8_t> payload, RetainedState& state) {
    // One attempt per channel keeps a full sweep short; the next wake retries.
    for (std::uint8_t channel = 1; channel <= config_.maxChannel; ++channel) {
        if (channel == state.channel) {
            continue;
        }
        transport_.setChannel(channel);
        if (transport_.send(payload)) {
            state.channel = channel;
            return true;
        }
    }
    transport_.setChannel(state.channel);
    return false;
}

bool NodeController::sendWithRetry(std::span<const std::uint8_t> payload) {
    std::uint32_t backoffMs = config_.retryBaseDelayMs;
    for (std::uint8_t attempt = 0; attempt < config_.maxSendAttempts; ++attempt) {
        if (attempt > 0) {
            delay_.delayMs(backoffMs);
            backoffMs *= 2;
        }
        if (transport_.send(payload)) {
            return true;
        }
    }
    return false;
}

}  // namespace sn::app
