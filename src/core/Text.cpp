#include "risha/core/Text.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <locale>
#include <sstream>

namespace risha::core {

std::u32string to_u32(std::wstring_view text) {
    std::u32string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t code = static_cast<char32_t>(text[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (code >= 0xD800 && code <= 0xDBFF && i + 1 < text.size()) {
                const char32_t low = static_cast<char32_t>(text[i + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    ++i;
                }
            }
        }
        result.push_back(code);
    }
    return result;
}

std::wstring from_u32(std::u32string_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (char32_t code : text) {
        if constexpr (sizeof(wchar_t) == 2) {
            if (code > 0xFFFF) {
                const char32_t value = code - 0x10000;
                result.push_back(static_cast<wchar_t>(0xD800 + (value >> 10)));
                result.push_back(static_cast<wchar_t>(0xDC00 + (value & 0x3FF)));
                continue;
            }
        }
        result.push_back(static_cast<wchar_t>(code));
    }
    return result;
}

std::string to_utf8(std::wstring_view text) {
    const std::u32string codes = to_u32(text);
    std::string result;
    result.reserve(codes.size());
    for (char32_t code : codes) {
        if (code >= 0xD800 && code <= 0xDFFF) code = 0xFFFD;
        if (code <= 0x7F) {
            result.push_back(static_cast<char>(code));
        } else if (code <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (code >> 6)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (code >> 12)));
            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code <= 0x10FFFF) {
            result.push_back(static_cast<char>(0xF0 | (code >> 18)));
            result.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            result.append("\xEF\xBF\xBD");
        }
    }
    return result;
}

std::wstring from_utf8(std::string_view bytes, bool* valid) {
    std::u32string result;
    bool ok = true;
    size_t i = 0;
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF && static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) i = 3;
    while (i < bytes.size()) {
        const unsigned char first = static_cast<unsigned char>(bytes[i]);
        char32_t code = 0;
        size_t count = 0;
        if (first < 0x80) { code = first; count = 1; }
        else if (first >= 0xC2 && first <= 0xDF) { code = first & 0x1F; count = 2; }
        else if (first >= 0xE0 && first <= 0xEF) { code = first & 0x0F; count = 3; }
        else if (first >= 0xF0 && first <= 0xF4) { code = first & 0x07; count = 4; }
        else { ok = false; result.push_back(0xFFFD); ++i; continue; }
        if (i + count > bytes.size()) { ok = false; result.push_back(0xFFFD); break; }
        bool good = true;
        for (size_t j = 1; j < count; ++j) {
            const unsigned char part = static_cast<unsigned char>(bytes[i + j]);
            if ((part & 0xC0) != 0x80) { good = false; break; }
            code = (code << 6) | (part & 0x3F);
        }
        if (good && ((count == 3 && code < 0x800) || (count == 4 && code < 0x10000) || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))) good = false;
        if (!good) { ok = false; result.push_back(0xFFFD); ++i; continue; }
        result.push_back(code);
        i += count;
    }
    if (valid) *valid = ok;
    return from_u32(result);
}

std::string encode_hex(std::wstring_view text) {
    static const char digits[] = "0123456789abcdef";
    const std::string bytes = to_utf8(text);
    std::string out;
    out.reserve(bytes.size() * 3);
    for (unsigned char c : bytes) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 15]);
        out.push_back(';');
    }
    return out;
}

bool decode_hex(std::string_view text, std::wstring& out) {
    std::string bytes;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(';', start);
        if (end == std::string_view::npos) end = text.size();
        const std::string part(text.substr(start, end - start));
        if (!part.empty()) {
            if (part.size() > 2) return false;
            char* stop = nullptr;
            const unsigned long value = std::strtoul(part.c_str(), &stop, 16);
            if (!stop || *stop != '\0' || value > 255) return false;
            bytes.push_back(static_cast<char>(value));
        }
        start = end + 1;
    }
    bool valid = true;
    out = from_utf8(bytes, &valid);
    return valid;
}

bool parse_double(std::string_view text, double& out) {
    if (text.empty()) return false;
    const std::string copy(text);
    std::istringstream stream(copy);
    stream.imbue(std::locale::classic());
    double value = 0.0;
    stream >> value;
    if (stream.fail()) return false;
    stream >> std::ws;
    if (!stream.eof()) return false;
    if (!std::isfinite(value)) return false;
    out = value;
    return true;
}

bool parse_int(std::string_view text, long long& out) {
    if (text.empty()) return false;
    size_t i = 0;
    bool negative = false;
    if (text[0] == '-' || text[0] == '+') { negative = text[0] == '-'; i = 1; }
    if (i >= text.size()) return false;
    long long value = 0;
    for (; i < text.size(); ++i) {
        if (text[i] < '0' || text[i] > '9') return false;
        value = value * 10 + (text[i] - '0');
        if (value > 4000000000000LL) return false;
    }
    out = negative ? -value : value;
    return true;
}

std::string format_double(double value, int precision) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream.precision(precision);
    stream << value;
    return stream.str();
}

}
