#include "risha/core/Time.hpp"
#include <algorithm>

namespace risha::core {

Time::Time() { reset(); }

void Time::reset() {
    start_ = Clock::now();
    previous_ = start_;
    delta_seconds_ = 0.0;
}

void Time::tick() {
    const auto now = Clock::now();
    delta_seconds_ = std::chrono::duration<double>(now - previous_).count();
    previous_ = now;
    delta_seconds_ = std::max(0.0, delta_seconds_);
}

double Time::elapsed_seconds() const noexcept {
    return std::chrono::duration<double>(Clock::now() - start_).count();
}

}
