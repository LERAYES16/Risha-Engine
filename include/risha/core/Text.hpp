#pragma once
#include <string>
#include <string_view>

namespace risha::core {

std::string to_utf8(std::wstring_view text);
std::wstring from_utf8(std::string_view bytes, bool* valid = nullptr);
std::u32string to_u32(std::wstring_view text);
std::wstring from_u32(std::u32string_view text);
std::string encode_hex(std::wstring_view text);
bool decode_hex(std::string_view text, std::wstring& out);
bool parse_double(std::string_view text, double& out);
bool parse_int(std::string_view text, long long& out);
std::string format_double(double value, int precision = 9);

}
