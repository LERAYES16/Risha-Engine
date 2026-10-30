#pragma once
#include <mutex>
#include <string_view>

namespace risha::core {

enum class LogLevel { Info, Warning, Error, Debug };

class Logger final {
public:
    static Logger& instance();
    void set_debug_enabled(bool enabled) noexcept;
    bool debug_enabled() const noexcept;
    void log(LogLevel level, std::string_view message);

private:
    Logger() = default;
    std::mutex mutex_;
    bool debug_enabled_{false};
};

void log_info(std::string_view message);
void log_warning(std::string_view message);
void log_error(std::string_view message);
void log_debug(std::string_view message);

}
