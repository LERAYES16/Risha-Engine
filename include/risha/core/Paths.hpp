#pragma once
#include "risha/core/Text.hpp"
#include <filesystem>
#include <string>

namespace risha::core {

inline std::filesystem::path make_path(const std::wstring& text) {
    return std::filesystem::u8path(to_utf8(text));
}

inline std::wstring path_string(const std::filesystem::path& path) {
    return from_utf8(path.u8string());
}

}
