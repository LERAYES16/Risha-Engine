#include "risha/ui/Ui.hpp"
#include "risha/core/Text.hpp"
#include <algorithm>
#include <cmath>

namespace risha::ui {
using gfx::Color;
using gfx::Rect;

void Ui::begin_frame(double delta, gfx::Canvas* target) {
    dt = std::clamp(delta, 0.0, 0.1);
    time += dt;
    canvas = target;
    animating_ = false;
}

void Ui::end_frame() {
    pressed = false;
    if (released) active_.clear();
    released = false;
}

float Ui::hover_of(const std::string& id) {
    const auto found = hover_.find(id);
    return found == hover_.end() ? 0.0f : found->second;
}

bool Ui::button(const std::string& id, Rect rect, const std::wstring& label, ButtonStyle style, bool enabled, float size, bool selected) {
    const bool inside = enabled && hot(rect);
    if (inside && pressed) active_ = id;
    bool clicked = false;
    if (released && active_ == id && inside) clicked = true;
    float& h = hover_[id];
    float& p = press_[id];
    const float target_h = inside ? 1.0f : 0.0f;
    const float target_p = (inside && active_ == id && mouse_down) ? 1.0f : 0.0f;
    const float k = static_cast<float>(std::min(1.0, dt * 16.0));
    h += (target_h - h) * k;
    p += (target_p - p) * std::min(1.0f, k * 1.6f);
    if (std::fabs(target_h - h) > 0.01f || std::fabs(target_p - p) > 0.01f) animating_ = true;
    Ripple& ripple = ripple_[id];
    if (clicked) ripple = Ripple{static_cast<float>(mouse_x - rect.x), static_cast<float>(mouse_y - rect.y), 0.0f};
    if (ripple.t < 1.0f) {
        ripple.t += static_cast<float>(dt / 0.5);
        animating_ = true;
    }

    Color base = theme::panel_raised;
    Color hover = Color(40, 52, 72);
    Color top_edge = theme::border;
    Color label_color = theme::text;
    switch (style) {
        case ButtonStyle::Primary: base = Color(40, 108, 214); hover = Color(62, 132, 240); top_edge = Color(110, 170, 255); break;
        case ButtonStyle::Danger: base = Color(86, 36, 44); hover = Color(138, 48, 58); top_edge = Color(190, 80, 90); break;
        case ButtonStyle::Ghost: base = Color(19, 25, 35, 0); hover = Color(40, 52, 72); top_edge = Color(43, 54, 72, 0); break;
        case ButtonStyle::Toggle: case ButtonStyle::Normal: break;
    }
    if (selected) { base = theme::accent_soft; hover = Color(46, 100, 176); top_edge = theme::accent; }
    if (!enabled) { label_color = Color(98, 108, 126); h = 0.0f; }
    const int shrink = static_cast<int>(std::lround(p * 1.5f));
    Rect r = rect.inset(shrink);
    r.y += static_cast<int>(std::lround(p));
    const Color fill = gfx::mix(base, hover, h);
    if (fill.a > 0 || h > 0.02f) canvas->gradient_rounded(r, 8.0f, gfx::scale(fill, 1.08f - 0.1f * p), gfx::scale(fill, 0.92f - 0.08f * p));
    const Color edge = gfx::mix(top_edge, theme::accent, h * 0.8f);
    if (edge.a > 0) canvas->stroke_rounded(r, 8.0f, 1.0f, edge.with_alpha(static_cast<int>(edge.a * (0.55f + 0.45f * h))));
    if (ripple.t < 1.0f) {
        gfx::ClipScope clip(*canvas, rect);
        const float radius = ripple.t * static_cast<float>(std::max(rect.w, rect.h)) * 1.1f;
        canvas->fill_circle(rect.x + ripple.x, rect.y + ripple.y, radius, Color(255, 255, 255, static_cast<int>((1.0f - ripple.t) * 70.0f)));
    }
    gfx::ClipScope clip(*canvas, rect.inset(2));
    const float text_size = size;
    float width = gfx::text_width(label, text_size);
    float used = text_size;
    while (width > rect.w - 10 && used > 10.0f) {
        used -= 1.0f;
        width = gfx::text_width(label, used);
    }
    const float baseline = static_cast<float>(r.y) + (static_cast<float>(r.h) + gfx::text_ascent(used) - gfx::text_height(used) * 0.18f) * 0.5f;
    gfx::draw_text(*canvas, label, static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f, baseline, used, label_color, gfx::Anchor::Center, style == ButtonStyle::Primary);
    return clicked;
}

void Ui::text(const std::wstring& value, int right_x, int baseline, float size, Color color, bool bold) {
    gfx::draw_text(*canvas, value, static_cast<float>(right_x), static_cast<float>(baseline), size, color, gfx::Anchor::Right, bold);
}

void Ui::text_left(const std::wstring& value, int left_x, int baseline, float size, Color color, bool bold) {
    gfx::draw_text(*canvas, value, static_cast<float>(left_x), static_cast<float>(baseline), size, color, gfx::Anchor::Left, bold);
}

void Ui::text_center(const std::wstring& value, int center_x, int baseline, float size, Color color, bool bold) {
    gfx::draw_text(*canvas, value, static_cast<float>(center_x), static_cast<float>(baseline), size, color, gfx::Anchor::Center, bold);
}

void draw_panel(gfx::Canvas& canvas, Rect rect, Color fill) {
    canvas.fill_rect(rect, fill);
}

std::string key_id(const std::wstring& prefix, int number) {
    return core::to_utf8(prefix) + std::to_string(number);
}

void draw_node(gfx::Canvas& canvas, const editor::SceneNode& node, float ox, float oy, float scale, bool editor_mode, double time) {
    if (!node.visible) return;
    const float x = ox + static_cast<float>(node.x) * scale;
    const float y = oy + static_cast<float>(node.y) * scale;
    const float w = std::max(2.0f, static_cast<float>(node.width) * std::fabs(node.scale_x) * scale);
    const float h = std::max(2.0f, static_cast<float>(node.height) * std::fabs(node.scale_y) * scale);
    const int alpha = static_cast<int>(std::clamp(node.opacity, 0.0f, 1.0f) * 255.0f);
    Color color = Color::from_argb(node.color).with_alpha(alpha);
    const Rect box{static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)), static_cast<int>(std::lround(w)), static_cast<int>(std::lround(h))};
    const bool is_player = node.type.find(L"لاعب") != std::wstring::npos;
    const bool is_creature = node.type.find(L"مخلوق") != std::wstring::npos;
    const bool is_ground = node.type.find(L"أرض") != std::wstring::npos;
    const bool is_camera = node.type.find(L"كاميرا") != std::wstring::npos;
    const bool is_text = node.type == L"نص";
    if (is_camera) {
        if (!editor_mode) return;
        canvas.stroke_rounded(box, 4.0f, 2.0f, Color(236, 190, 96, 220));
        gfx::draw_text(canvas, L"كاميرا", x + w * 0.5f, y + h * 0.5f + 5.0f, 13.0f * std::max(0.8f, scale), Color(236, 190, 96), gfx::Anchor::Center);
        return;
    }
    if (is_text) {
        const float size = std::max(10.0f, h * 0.7f);
        gfx::draw_text(canvas, node.text.empty() ? node.name : node.text, x + w * 0.5f, y + h * 0.5f + size * 0.3f, size, color, gfx::Anchor::Center, true);
        return;
    }
    if (is_ground) {
        canvas.gradient_rounded(box, 2.0f * scale, gfx::scale(color, 1.1f), gfx::scale(color, 0.72f));
        const int grass = std::max(3, static_cast<int>(h * 0.16f));
        canvas.gradient_rounded(Rect{box.x, box.y, box.w, grass}, 2.0f, Color(126, 200, 112, alpha), Color(78, 150, 84, alpha));
        for (int i = 0; i < box.w; i += std::max(10, static_cast<int>(26 * scale))) {
            canvas.fill_rect(Rect{box.x + i + 4, box.y + grass + 6 + (i * 7) % std::max(4, box.h / 2), std::max(2, static_cast<int>(5 * scale)), std::max(1, static_cast<int>(2 * scale))}, Color(0, 0, 0, 40));
        }
        return;
    }
    if (is_player) {
        const float head = std::min(w, h) * 0.38f;
        canvas.shadow(Rect{box.x + box.w / 8, box.y + box.h - 4, box.w * 3 / 4, 6}, 3.0f, 4, 70);
        canvas.gradient_rounded(Rect{box.x + box.w / 8, static_cast<int>(y + head * 0.9f), box.w * 3 / 4, static_cast<int>(h - head * 0.9f)}, 8.0f * scale, gfx::scale(color, 1.15f), gfx::scale(color, 0.8f));
        canvas.fill_circle(x + w * 0.5f, y + head * 0.55f, head * 0.55f, Color(246, 214, 176, alpha));
        canvas.fill_circle(x + w * 0.5f - head * 0.18f, y + head * 0.5f, std::max(1.2f, head * 0.07f), Color(30, 34, 44, alpha));
        canvas.fill_circle(x + w * 0.5f + head * 0.18f, y + head * 0.5f, std::max(1.2f, head * 0.07f), Color(30, 34, 44, alpha));
        return;
    }
    if (is_creature) {
        canvas.gradient_rounded(box, std::min(w, h) * 0.45f, gfx::scale(color, 1.2f), gfx::scale(color, 0.78f));
        canvas.fill_circle(x + w * 0.35f, y + h * 0.38f, std::max(2.0f, w * 0.1f), Color(255, 255, 255, alpha));
        canvas.fill_circle(x + w * 0.65f, y + h * 0.38f, std::max(2.0f, w * 0.1f), Color(255, 255, 255, alpha));
        const float bob = static_cast<float>(std::sin(time * 4.0)) * (editor_mode ? 0.0f : 1.0f);
        canvas.fill_circle(x + w * 0.37f, y + h * 0.4f + bob, std::max(1.0f, w * 0.045f), Color(20, 24, 32, alpha));
        canvas.fill_circle(x + w * 0.67f, y + h * 0.4f + bob, std::max(1.0f, w * 0.045f), Color(20, 24, 32, alpha));
        return;
    }
    canvas.shadow(box, 6.0f * scale, 5, 60);
    canvas.gradient_rounded(box, std::min(10.0f * scale, std::min(w, h) * 0.3f), gfx::scale(color, 1.18f), gfx::scale(color, 0.82f));
    canvas.stroke_rounded(box, std::min(10.0f * scale, std::min(w, h) * 0.3f), 1.0f, Color(255, 255, 255, 60));
}

}
