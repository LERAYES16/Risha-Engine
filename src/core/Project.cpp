#include "risha/core/Project.hpp"
#include "risha/core/Text.hpp"
#include "risha/core/Paths.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace risha::core {
namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> result;
    size_t start = 0;
    while (true) {
        const size_t end = line.find('|', start);
        result.push_back(line.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

std::string encode(const std::wstring& value) { return to_utf8(value); }

std::wstring decode(const std::string& value) {
    bool valid = true;
    std::wstring result = from_utf8(value, &valid);
    return valid ? result : std::wstring();
}

bool to_int(const std::string& text, int& value) {
    long long parsed = 0;
    if (!parse_int(text, parsed) || parsed < -2000000000LL || parsed > 2000000000LL) return false;
    value = static_cast<int>(parsed);
    return true;
}

}

ProjectSettings default_project_settings() {
    ProjectSettings settings;
    settings.input_actions = {
        {L"حركة_يمين", {L"D", L"Right"}},
        {L"حركة_يسار", {L"A", L"Q", L"Left"}},
        {L"حركة_أعلى", {L"W", L"Z", L"Up"}},
        {L"حركة_أسفل", {L"S", L"Down"}},
        {L"قفز", {L"Space"}}
    };
    return settings;
}

bool ProjectSettings::save(const std::wstring& path) const {
    std::ofstream file(core::make_path(path), std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file << "RISHA_PROJECT_0_5\n";
    file << "name=" << encode(name) << '\n';
    file << "main=" << encode(main_scene) << '\n';
    file << "width=" << width << '\n';
    file << "height=" << height << '\n';
    file << "fullscreen=" << (fullscreen ? 1 : 0) << '\n';
    file << "background=" << background_color << '\n';
    file << "fps=" << target_fps << '\n';
    for (const auto& action : input_actions) {
        file << "input=" << encode(action.name) << '|';
        for (size_t i = 0; i < action.keys.size(); ++i) {
            if (i) file << ';';
            file << encode(action.keys[i]);
        }
        file << '\n';
    }
    file.flush();
    return static_cast<bool>(file);
}

bool ProjectSettings::load(const std::wstring& path) {
    std::ifstream file(core::make_path(path), std::ios::binary);
    if (!file) return false;
    std::string header;
    if (!std::getline(file, header)) return false;
    while (!header.empty() && (header.back() == '\r' || header.back() == ' ')) header.pop_back();
    if (header != "RISHA_PROJECT_0_4" && header != "RISHA_PROJECT_0_5") return false;
    ProjectSettings loaded = default_project_settings();
    loaded.input_actions.clear();
    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("name=", 0) == 0) loaded.name = decode(line.substr(5));
        else if (line.rfind("main=", 0) == 0) loaded.main_scene = decode(line.substr(5));
        else if (line.rfind("width=", 0) == 0) { if (!to_int(line.substr(6), loaded.width)) return false; }
        else if (line.rfind("height=", 0) == 0) { if (!to_int(line.substr(7), loaded.height)) return false; }
        else if (line.rfind("fullscreen=", 0) == 0) {
            int value = 0;
            if (!to_int(line.substr(11), value) || (value != 0 && value != 1)) return false;
            loaded.fullscreen = value != 0;
        } else if (line.rfind("background=", 0) == 0) {
            long long value = 0;
            if (!parse_int(line.substr(11), value) || value < 0 || value > 0xFFFFFFFFLL) return false;
            loaded.background_color = static_cast<unsigned int>(value);
        } else if (line.rfind("fps=", 0) == 0) { if (!to_int(line.substr(4), loaded.target_fps)) return false; }
        else if (line.rfind("input=", 0) == 0) {
            const auto parts = split(line.substr(6));
            if (parts.size() != 2) return false;
            InputAction action;
            action.name = decode(parts[0]);
            std::stringstream keys(parts[1]);
            std::string key;
            while (std::getline(keys, key, ';')) {
                auto decoded = decode(key);
                if (decoded.empty()) return false;
                action.keys.push_back(std::move(decoded));
            }
            if (action.name.empty() || action.keys.empty()) return false;
            if (std::any_of(loaded.input_actions.begin(), loaded.input_actions.end(), [&](const InputAction& existing) { return existing.name == action.name; })) return false;
            loaded.input_actions.push_back(std::move(action));
        }
    }
    if (loaded.name.empty() || loaded.main_scene.empty() || loaded.width < 320 || loaded.width > 16384 || loaded.height < 240 || loaded.height > 16384 || loaded.target_fps < 1 || loaded.target_fps > 1000) return false;
    *this = std::move(loaded);
    return true;
}

}
