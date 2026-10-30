#include "risha/core/Paths.hpp"
#include "risha/runtime/Runtime.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
int fail(int code, const char* what) {
    std::cerr << what << '\n';
    return code;
}
}

int main() {
    const auto dir = std::filesystem::path(RISHA_SOURCE_DIR) / "examples" / "demo_game";
    risha::editor::Scene scene;
    if (!scene.load(risha::core::path_string(dir / "main.risha"))) return fail(1, "scene");
    std::ifstream file(dir / risha::core::make_path(L"الكتلة.rishascript"), std::ios::binary);
    if (!file) return fail(2, "script file");
    const std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    risha::runtime::Runtime runtime;
    runtime.load_scene(scene);
    const int block = [&] { for (const auto& n : runtime.scene().nodes()) if (n.name == L"الكتلة") return n.id; return 0; }();
    if (!block || !runtime.attach_script(block, risha::core::from_utf8(data))) return fail(3, "attach");
    if (!runtime.play()) return fail(4, "play");
    for (int i = 0; i < 90; ++i) runtime.step(1.0 / 60.0);
    const auto* node = runtime.scene().find(block);
    if (node->y + node->height != 440) { std::cerr << node->y << " " << node->grounded << " " << node->body << " " << node->gravity << "\n"; return fail(5, "not resting on ground"); }
    const int start_x = node->x;
    runtime.set_action(L"حركة_يمين", true);
    for (int i = 0; i < 30; ++i) runtime.step(1.0 / 60.0);
    if (node->x <= start_x + 100) return fail(6, "did not move right");
    runtime.set_action(L"حركة_يمين", false);
    runtime.set_action(L"حركة_يسار", true);
    for (int i = 0; i < 300; ++i) runtime.step(1.0 / 60.0);
    if (node->x < -8 || node->x > 0) return fail(7, "not clamped at left");
    runtime.set_action(L"حركة_يسار", false);
    runtime.set_action(L"قفز", true);
    runtime.set_action_pressed(L"قفز");
    int top = node->y;
    for (int i = 0; i < 30; ++i) { runtime.step(1.0 / 60.0); runtime.clear_input_edges(); if (node->y < top) top = node->y; }
    if (top > 320) { std::cerr << top << " " << node->y << " " << node->grounded << " " << node->x << "\n"; return fail(8, "did not jump"); }
    for (const auto& m : runtime.messages()) if (m.error) return fail(9, "script error");
    return 0;
}
