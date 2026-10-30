#pragma once
#include "risha/editor/Scene.hpp"
#include "risha/gfx/Canvas.hpp"
#include "risha/gfx/Text.hpp"
#include <string>
#include <unordered_map>

namespace risha::ui {

namespace theme {
inline const gfx::Color background{12, 16, 23};
inline const gfx::Color panel{19, 25, 35};
inline const gfx::Color panel_raised{27, 35, 49};
inline const gfx::Color border{43, 54, 72};
inline const gfx::Color accent{59, 142, 255};
inline const gfx::Color accent_soft{36, 78, 140};
inline const gfx::Color danger{222, 82, 92};
inline const gfx::Color text{232, 237, 246};
inline const gfx::Color muted{139, 152, 175};
inline const gfx::Color warning{236, 190, 96};
}

enum class ButtonStyle { Normal, Primary, Danger, Ghost, Toggle };

class Ui {
public:
    gfx::Canvas* canvas{nullptr};
    int mouse_x{-1};
    int mouse_y{-1};
    bool mouse_down{false};
    bool pressed{false};
    bool released{false};
    bool blocked{false};
    double dt{0.016};
    double time{0.0};

    void begin_frame(double delta, gfx::Canvas* target);
    void end_frame();
    bool animating() const { return animating_; }
    void keep_animating() { animating_ = true; }
    bool hot(gfx::Rect rect) const { return !blocked && rect.contains(mouse_x, mouse_y); }
    bool button(const std::string& id, gfx::Rect rect, const std::wstring& label, ButtonStyle style = ButtonStyle::Normal, bool enabled = true, float size = 15.0f, bool selected = false);
    float hover_of(const std::string& id);
    void text(const std::wstring& value, int right_x, int baseline, float size, gfx::Color color, bool bold = false);
    void text_left(const std::wstring& value, int left_x, int baseline, float size, gfx::Color color, bool bold = false);
    void text_center(const std::wstring& value, int center_x, int baseline, float size, gfx::Color color, bool bold = false);

private:
    struct Ripple {
        float x{0.0f};
        float y{0.0f};
        float t{1.0f};
    };
    std::unordered_map<std::string, float> hover_;
    std::unordered_map<std::string, float> press_;
    std::unordered_map<std::string, Ripple> ripple_;
    std::string active_;
    bool animating_{false};
};

void draw_panel(gfx::Canvas& canvas, gfx::Rect rect, gfx::Color fill);
void draw_node(gfx::Canvas& canvas, const editor::SceneNode& node, float origin_x, float origin_y, float scale, bool editor_mode, double time);
std::string key_id(const std::wstring& prefix, int number);

}
