#include "risha/core/Time.hpp"
#include "risha/core/Logger.hpp"
#include <cassert>
#include <chrono>
#include <thread>

int main() {
    risha::core::Time time;
    assert(time.delta_seconds() == 0.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    time.tick();
    assert(time.delta_seconds() > 0.0);
    assert(time.elapsed_seconds() >= time.delta_seconds());

    auto& logger = risha::core::Logger::instance();
    logger.set_debug_enabled(false);
    assert(!logger.debug_enabled());
    logger.set_debug_enabled(true);
    assert(logger.debug_enabled());
    logger.set_debug_enabled(false);
    return 0;
}
