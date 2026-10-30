#include "risha/runtime/Runtime.hpp"
#include "risha/core/Project.hpp"
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>

int main() {
    risha::editor::Scene scene;
    const int player = scene.add_node(1, L"Player", L"لاعب");
    auto* node = scene.find(player);
    if (!node) return 1;
    node->x = 10;
    node->y = 20;
    risha::runtime::Runtime runtime;
    if (!runtime.load_scene(scene) || !runtime.play()) return 2;
    runtime.set_action(L"حركة_يمين", true);
    if (!runtime.run_script(L"حرك(\"Player\", 5 * دلتا(), 0)\nاطبع موضع_س(\"Player\")\n")) return 3;
    if (runtime.output().size() != 1) return 4;
    runtime.stop();
    if (runtime.running()) return 5;
    risha::core::ProjectSettings settings = risha::core::default_project_settings();
    settings.name = L"اختبار ريشة";
    settings.main_scene = L"main.risha";
    settings.input_actions.push_back({L"اختبار", {L"Q", L"F1"}});
    const std::wstring path = L"risha_project_test.rishaproject";
    if (!settings.save(path)) return 6;
    risha::core::ProjectSettings loaded;
    if (!loaded.load(path)) return 7;
    if (loaded.name != settings.name || loaded.main_scene != settings.main_scene || loaded.input_actions.size() != settings.input_actions.size()) return 8;
    std::remove("risha_project_test.rishaproject");
    const std::wstring malformed_path = L"risha_project_malformed_test.rishaproject";
    {
        std::ofstream malformed(std::filesystem::path(malformed_path), std::ios::binary | std::ios::trunc);
        malformed << "RISHA_PROJECT_0_4\nwidth=not-a-number\n";
    }
    risha::core::ProjectSettings malformed_settings;
    if (malformed_settings.load(malformed_path)) return 14;
    std::remove("risha_project_malformed_test.rishaproject");
    {
        std::ofstream malformed(std::filesystem::path(malformed_path), std::ios::binary | std::ios::trunc);
        malformed << "RISHA_PROJECT_0_4\nname=x\nmain=main.risha\nwidth=100000\nheight=540\nfps=60\n";
    }
    if (malformed_settings.load(malformed_path)) return 15;
    std::remove("risha_project_malformed_test.rishaproject");
    risha::editor::Scene demo;
    if (!demo.load(std::filesystem::path(RISHA_SOURCE_DIR).append("examples").append("demo.risha").wstring())) return 9;
    if (!demo.find(2) || demo.find(2)->name != L"لاعب" || !demo.find(3) || demo.find(3)->name != L"أرض" || !demo.find(4) || demo.find(4)->name != L"كاميرا") return 10;
    risha::runtime::Runtime demo_runtime;
    if (!demo_runtime.load_scene(demo) || !demo_runtime.play()) return 11;
    demo_runtime.set_action(L"حركة_يمين", true);
    demo_runtime.update(1.0 / 60.0);
    const int starting_x = demo_runtime.scene().find(2)->x;
    if (!demo_runtime.run_script(L"إذا زر_مستمر(\"حركة_يمين\")\n    حرك(\"لاعب\", 120 * دلتا(), 0)\nنهاية\nاطبع موضع_س(\"لاعب\")\n", 2)) return 12;
    if (demo_runtime.output().size() != 1) return 13;
    if (demo_runtime.scene().find(2)->x <= starting_x) return 16;
    std::cout << "runtime tests passed\n";
    return 0;
}
