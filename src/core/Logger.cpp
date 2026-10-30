#include "risha/core/Logger.hpp"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace risha::core {
namespace {
#ifdef _WIN32
void write_windows_console(std::string_view utf8) {
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return;
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) return;
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), size);
    DWORD written = 0;
    if (GetConsoleMode(handle, &written)) {
        WriteConsoleW(handle, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
        WriteConsoleW(handle, L"\r\n", 2, &written, nullptr);
    } else {
        SetConsoleOutputCP(CP_UTF8);
        DWORD bytes = 0;
        WriteFile(handle, utf8.data(), static_cast<DWORD>(utf8.size()), &bytes, nullptr);
        const char nl[] = "\r\n";
        WriteFile(handle, nl, 2, &bytes, nullptr);
    }
}
#endif
}
Logger& Logger::instance(){static Logger logger;return logger;}
void Logger::set_debug_enabled(bool enabled)noexcept{debug_enabled_=enabled;}
bool Logger::debug_enabled()const noexcept{return debug_enabled_;}
void Logger::log(LogLevel level,std::string_view message){if(level==LogLevel::Debug&&!debug_enabled_)return;std::lock_guard<std::mutex> lock(mutex_);const char* prefix="[معلومات]";switch(level){case LogLevel::Info:prefix="[معلومات]";break;case LogLevel::Warning:prefix="[تحذير]";break;case LogLevel::Error:prefix="[خطأ]";break;case LogLevel::Debug:prefix="[تصحيح]";break;}std::string line=std::string(prefix)+" "+std::string(message);
#ifdef _WIN32
write_windows_console(line);
#else
std::cout<<line<<'\n'<<std::flush;
#endif
}
void log_info(std::string_view m){Logger::instance().log(LogLevel::Info,m);}void log_warning(std::string_view m){Logger::instance().log(LogLevel::Warning,m);}void log_error(std::string_view m){Logger::instance().log(LogLevel::Error,m);}void log_debug(std::string_view m){Logger::instance().log(LogLevel::Debug,m);}
}
