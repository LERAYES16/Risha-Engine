#pragma once
#include <chrono>

namespace risha::core {

class Time final {
public:
    using Clock = std::chrono::steady_clock;

    Time();
    void reset();
    void tick();
    double delta_seconds() const noexcept { return delta_seconds_; }
    double elapsed_seconds() const noexcept;

private:
    Clock::time_point start_;
    Clock::time_point previous_;
    double delta_seconds_{0.0};
};

}
