#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "sn/hal/clock.hpp"
#include "sn/hal/gpio.hpp"

namespace sn::test {

// Time advances by `tickUs` every time the code under test looks at the
// clock, which models a busy-wait loop making progress.
class FakeClock : public hal::IClock {
public:
    explicit FakeClock(std::uint64_t tickUs = 1) : tickUs_(tickUs) {}
    std::uint64_t nowUs() const override {
        now_ += tickUs_;
        return now_;
    }
    void delayUs(std::uint32_t us) override { now_ += us; }
    void advance(std::uint64_t us) { now_ += us; }
    std::uint64_t peek() const { return now_; }

private:
    mutable std::uint64_t now_ = 0;
    std::uint64_t tickUs_;
};

class FakeInput : public hal::IInputPin {
public:
    bool read() const override { return level; }
    bool level = false;
};

// Simulates the HC-SR04: each trigger pulse consumes the next scripted echo.
// std::nullopt models a sensor that never answers.
class FakeHcsr04 : public hal::IOutputPin, public hal::IInputPin {
public:
    static constexpr std::uint64_t kEchoLatencyUs = 200;
    static constexpr std::uint64_t kStuckHigh = UINT64_MAX;

    explicit FakeHcsr04(FakeClock& clock) : clock_(clock) {}

    void write(bool level) override {
        if (lastTrig_ && !level) {
            ++triggers;
            schedule();
        }
        lastTrig_ = level;
    }

    bool read() const override {
        if (!riseAt_) {
            return false;
        }
        const std::uint64_t now = clock_.peek();
        return now >= *riseAt_ && (fallAt_ == kStuckHigh || now < fallAt_);
    }

    std::deque<std::optional<std::uint64_t>> echoes;  // echo high time in us
    int triggers = 0;

private:
    void schedule() {
        riseAt_.reset();
        if (echoes.empty()) {
            return;
        }
        const auto echo = echoes.front();
        echoes.pop_front();
        if (!echo) {
            return;
        }
        riseAt_ = clock_.peek() + kEchoLatencyUs;
        fallAt_ = *echo == kStuckHigh ? kStuckHigh : *riseAt_ + *echo;
    }

    FakeClock& clock_;
    bool lastTrig_ = false;
    std::optional<std::uint64_t> riseAt_;
    std::uint64_t fallAt_ = 0;
};

}  // namespace sn::test
