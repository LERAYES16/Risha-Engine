#include "Game.hpp"
#include "risha/core/Paths.hpp"
#include "risha/core/Text.hpp"
#include "risha/gfx/Text.hpp"
#include "risha/ui/Ui.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>

namespace risha::app {
using platform::EventType;
using platform::Key;

std::wstring key_name(Key key) {
    const int value = static_cast<int>(key);
    if (key >= Key::A && key <= Key::Z) return std::wstring(1, static_cast<wchar_t>(L'A' + (value - static_cast<int>(Key::A))));
    if (key >= Key::D0 && key <= Key::D9) return L"D" + std::to_wstring(value - static_cast<int>(Key::D0));
    switch (key) {
        case Key::Left: return L"Left";
        case Key::Right: return L"Right";
        case Key::Up: return L"Up";
        case Key::Down: return L"Down";
        case Key::Space: return L"Space";
        case Key::Enter: return L"Enter";
        case Key::Tab: return L"Tab";
        default: return L"";
    }
}

static std::wstring read_file(const std::filesystem::path& path, bool& ok) {
    std::ifstream file(path, std::ios::binary);
    ok = static_cast<bool>(file);
    if (!ok) return L"";
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return core::from_utf8(data);
}

bool Game::start(const editor::Scene& scene, const core::ProjectSettings& settings, const std::filesystem::path& directory, std::vector<OutputLine>& sink) {
    stop();
    settings_ = settings;
    runtime_ = runtime::Runtime();
    runtime_.load_scene(scene);
    for (const auto& node : runtime_.scene().nodes()) {
        if (node.script.empty()) continue;
        bool ok = false;
        const std::wstring source = read_file(directory / core::make_path(node.script), ok);
        if (!ok) {
            sink.push_back({L"تعذر فتح السكربت: " + node.script, true});
            continue;
        }
        runtime_.attach_script(node.id, source);
    }
    if (!runtime_.play()) {
        sink.push_back({L"أضف كائنًا واحدًا على الأقل قبل التشغيل.", true});
        return false;
    }
    if (!window_.create(L"تشغيل - " + settings.name, std::max(480, settings.width), std::max(270, settings.height), 480, 270)) {
        sink.push_back({L"تعذر فتح نافذة اللعبة.", true});
        return false;
    }
    canvas_.resize(window_.width(), window_.height());
    accumulator_ = 0.0;
    clock_ = 0.0;
    hud_.clear();
    dirty_ = true;
    sink.push_back({L"بدأ التشغيل.", false});
    return true;
}

void Game::stop() {
    if (window_.valid()) window_.close();
    runtime_.stop();
}

void Game::release_all() {
    for (const auto& action : settings_.input_actions) runtime_.set_action(action.name, false);
}

void Game::handle(const std::vector<platform::Event>& events) {
    for (const auto& event : events) {
        switch (event.type) {
            case EventType::Close: stop(); return;
            case EventType::Resize: canvas_.resize(window_.width(), window_.height()); dirty_ = true; break;
            case EventType::Redraw: dirty_ = true; break;
            case EventType::LostFocus: release_all(); break;
            case EventType::KeyDown:
            case EventType::KeyUp: {
                if (event.key == Key::Escape && event.type == EventType::KeyDown) { stop(); return; }
                const std::wstring name = key_name(event.key);
                if (name.empty()) break;
                for (const auto& action : settings_.input_actions) {
                    if (std::find(action.keys.begin(), action.keys.end(), name) == action.keys.end()) continue;
                    if (event.type == EventType::KeyDown) {
                        if (!event.repeat) { runtime_.set_action(action.name, true); runtime_.set_action_pressed(action.name); }
                    } else runtime_.set_action(action.name, false);
                }
                break;
            }
            default: break;
        }
    }
}

void Game::tick(double dt, std::vector<OutputLine>& sink) {
    if (!open()) return;
    clock_ += dt;
    accumulator_ += std::min(dt, 0.1);
    const double step = 1.0 / 60.0;
    while (accumulator_ >= step) {
        accumulator_ -= step;
        runtime_.step(step);
        runtime_.clear_input_edges();
    }
    for (const auto& message : runtime_.messages()) {
        std::wstring text = message.text;
        if (message.error) text = (message.line > 0 ? L"سطر " + std::to_wstring(message.line) + L": " : L"") + text;
        if (!message.node_name.empty()) text = message.node_name + L" ← " + text;
        sink.push_back({text, message.error});
        hud_.push_back({text, message.error});
        if (sink.size() > 300) sink.erase(sink.begin(), sink.begin() + 100);
    }
    runtime_.clear_messages();
    while (hud_.size() > 4) hud_.pop_front();
    if (runtime_.stop_requested()) { stop(); return; }
    render();
}

void Game::render() {
    if (!open()) return;
    using gfx::Color;
    const float w = static_cast<float>(canvas_.width());
    const float h = static_cast<float>(canvas_.height());
    canvas_.clear(Color::from_argb(settings_.background_color));
    const float world_w = static_cast<float>(std::max(1, settings_.width));
    const float world_h = static_cast<float>(std::max(1, settings_.height));
    float fit = std::min(w / world_w, h / world_h);
    float ox = (w - world_w * fit) * 0.5f;
    float oy = (h - world_h * fit) * 0.5f;
    const auto camera = runtime_.camera();
    if (camera.active) {
        fit *= static_cast<float>(std::max(0.1, camera.zoom));
        ox = w * 0.5f - static_cast<float>(camera.center_x) * fit;
        oy = h * 0.5f - static_cast<float>(camera.center_y) * fit;
    }
    std::vector<const editor::SceneNode*> order;
    for (const auto& node : runtime_.scene().nodes()) if (node.parent_id != 0) order.push_back(&node);
    std::stable_sort(order.begin(), order.end(), [](auto* a, auto* b) { return a->z < b->z; });
    {
        gfx::ClipScope clip(canvas_, gfx::Rect{static_cast<int>(camera.active ? 0 : ox), static_cast<int>(camera.active ? 0 : oy), static_cast<int>(camera.active ? w : world_w * fit), static_cast<int>(camera.active ? h : world_h * fit)});
        for (const auto* node : order) ui::draw_node(canvas_, *node, ox, oy, fit, false, clock_);
    }
    int y = 24;
    for (const auto& line : hud_) {
        gfx::draw_text(canvas_, line.text, w - 14.0f, static_cast<float>(y), 14.0f, line.error ? Color(255, 120, 120) : Color(235, 240, 250), gfx::Anchor::Right);
        y += 20;
    }
    gfx::draw_text(canvas_, L"Esc للإغلاق", w - 14.0f, h - 12.0f, 12.0f, Color(255, 255, 255, 120), gfx::Anchor::Right);
    window_.present(canvas_);
}

}
