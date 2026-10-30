#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace risha::gfx {

struct Color {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};
    constexpr Color() = default;
    constexpr Color(int red, int green, int blue, int alpha = 255)
        : r(static_cast<uint8_t>(red)), g(static_cast<uint8_t>(green)), b(static_cast<uint8_t>(blue)), a(static_cast<uint8_t>(alpha)) {}
    static constexpr Color from_argb(uint32_t value) {
        return Color(static_cast<int>((value >> 16) & 255), static_cast<int>((value >> 8) & 255), static_cast<int>(value & 255), static_cast<int>(value >> 24));
    }
    constexpr uint32_t rgb() const { return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b; }
    constexpr uint32_t argb() const { return (static_cast<uint32_t>(a) << 24) | rgb(); }
    constexpr Color with_alpha(int alpha) const { return Color(r, g, b, alpha); }
};

Color mix(Color from, Color to, float amount);
Color scale(Color color, float factor);

struct PointF {
    float x{0.0f};
    float y{0.0f};
};

struct Rect {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    Rect inset(int amount) const { return Rect{x + amount, y + amount, w - amount * 2, h - amount * 2}; }
    Rect moved(int dx, int dy) const { return Rect{x + dx, y + dy, w, h}; }
};

class Canvas {
public:
    Canvas() = default;
    Canvas(int width, int height) { resize(width, height); }

    void resize(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    uint32_t* data() { return pixels_.data(); }
    const uint32_t* data() const { return pixels_.data(); }
    uint32_t pixel(int x, int y) const;

    void clear(Color color);
    void push_clip(Rect rect);
    void pop_clip();
    Rect clip() const;

    void blend(int x, int y, Color color, float coverage);
    void fill_rect(Rect rect, Color color);
    void fill_rounded(Rect rect, float radius, Color color);
    void stroke_rounded(Rect rect, float radius, float thickness, Color color);
    void fill_circle(float cx, float cy, float radius, Color color);
    void stroke_circle(float cx, float cy, float radius, float thickness, Color color);
    void line(float x0, float y0, float x1, float y1, float thickness, Color color);
    void polyline(const std::vector<PointF>& points, float thickness, Color color);
    void fill_polygon(const std::vector<PointF>& points, Color color);
    void fill_polygon(const std::vector<PointF>& points, const std::function<Color(float, float)>& shader);
    void gradient_rect(Rect rect, Color top, Color bottom);
    void gradient_rounded(Rect rect, float radius, Color top, Color bottom);
    void shadow(Rect rect, float radius, int spread, int alpha);
    void blit_coverage(int x, int y, int width, int height, const uint8_t* coverage, Color color);
    void blit(const Canvas& source, int x, int y, float opacity = 1.0f);

private:
    void put(int x, int y, Color color, float coverage);
    int width_{0};
    int height_{0};
    std::vector<uint32_t> pixels_;
    std::vector<Rect> clips_;
};

class ClipScope {
public:
    ClipScope(Canvas& canvas, Rect rect) : canvas_(canvas) { canvas_.push_clip(rect); }
    ~ClipScope() { canvas_.pop_clip(); }
    ClipScope(const ClipScope&) = delete;
    ClipScope& operator=(const ClipScope&) = delete;

private:
    Canvas& canvas_;
};

}
