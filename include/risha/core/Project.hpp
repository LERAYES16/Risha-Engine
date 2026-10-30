#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace risha::core {

struct InputAction {
    std::wstring name;
    std::vector<std::wstring> keys;
};

struct ProjectSettings {
    std::wstring name{L"مشروع ريشة"};
    std::wstring main_scene{L"main.risha"};
    int width{960};
    int height{540};
    bool fullscreen{false};
    unsigned int background_color{0xFF10141Cu};
    int target_fps{60};
    std::vector<InputAction> input_actions;

    bool save(const std::wstring& path) const;
    bool load(const std::wstring& path);
};

ProjectSettings default_project_settings();

}
