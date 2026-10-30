#include "risha/gfx/Canvas.hpp"
#include "risha/gfx/Raster.hpp"
#include <algorithm>
#include <cmath>

namespace risha::gfx {
namespace {

inline int blend_channel(int destination, int source, int alpha) {
    return destination + (((source - destination) * alpha + 127) / 255);
}

float rounded_distance(float px, float py, float cx, float cy, float half_w, float half_h, float radius) {
    const float qx = std::fabs(px - cx) - (half_w - radius);
    const float qy = std::fabs(py - cy) - (half_h - radius);
    const float ox = std::max(qx, 0.0f);
    const float oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - radius;
}

float segment_distance(float px, float py, float x0, float y0, float x1, float y1) {
    const float vx = x1 - x0;
    const float vy = y1 - y0;
    const float length_squared = vx * vx + vy * vy;
    float t = 0.0f;
    if (length_squared > 1e-8f) t = std::clamp(((px - x0) * vx + (py - y0) * vy) / length_squared, 0.0f, 1.0f);
    const float dx = px - (x0 + vx * t);
    const float dy = py - (y0 + vy * t);
    return std::sqrt(dx * dx + dy * dy);
}

}

Color mix(Color from, Color to, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    return Color(static_cast<int>(from.r + (to.r - from.r) * amount + 0.5f), static_cast<int>(from.g + (to.g - from.g) * amount + 0.5f),
                 static_cast<int>(from.b + (to.b - from.b) * amount + 0.5f), static_cast<int>(from.a + (to.a - from.a) * amount + 0.5f));
}

Color scale(Color color, float factor) {
    return Color(std::clamp(static_cast<int>(color.r * factor + 0.5f), 0, 255), std::clamp(static_cast<int>(color.g * factor + 0.5f), 0, 255),
                 std::clamp(static_cast<int>(color.b * factor + 0.5f), 0, 255), color.a);
}

void Canvas::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    pixels_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0u);
    clips_.clear();
}

uint32_t Canvas::pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
    return pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
}

void Canvas::clear(Color color) {
    std::fill(pixels_.begin(), pixels_.end(), color.rgb());
}

Rect Canvas::clip() const {
    return clips_.empty() ? Rect{0, 0, width_, height_} : clips_.back();
}

void Canvas::push_clip(Rect rect) {
    const Rect current = clip();
    const int x0 = std::max(current.x, rect.x);
    const int y0 = std::max(current.y, rect.y);
    const int x1 = std::min(current.x + current.w, rect.x + rect.w);
    const int y1 = std::min(current.y + current.h, rect.y + rect.h);
    clips_.push_back(Rect{x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)});
}

void Canvas::pop_clip() {
    if (!clips_.empty()) clips_.pop_back();
}

void Canvas::put(int x, int y, Color color, float coverage) {
    const int alpha = static_cast<int>(coverage * color.a + 0.5f);
    if (alpha <= 0) return;
    uint32_t& destination = pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
    if (alpha >= 255) {
        destination = color.rgb();
        return;
    }
    const int dr = static_cast<int>((destination >> 16) & 255);
    const int dg = static_cast<int>((destination >> 8) & 255);
    const int db = static_cast<int>(destination & 255);
    destination = (static_cast<uint32_t>(blend_channel(dr, color.r, alpha)) << 16) | (static_cast<uint32_t>(blend_channel(dg, color.g, alpha)) << 8) |
                  static_cast<uint32_t>(blend_channel(db, color.b, alpha));
}

void Canvas::blend(int x, int y, Color color, float coverage) {
    const Rect c = clip();
    if (x < c.x || y < c.y || x >= c.x + c.w || y >= c.y + c.h) return;
    put(x, y, color, coverage);
}

void Canvas::fill_rect(Rect rect, Color color) {
    const Rect c = clip();
    const int x0 = std::max(c.x, rect.x);
    const int y0 = std::max(c.y, rect.y);
    const int x1 = std::min(c.x + c.w, rect.x + rect.w);
    const int y1 = std::min(c.y + c.h, rect.y + rect.h);
    if (x0 >= x1 || y0 >= y1 || color.a == 0) return;
    if (color.a == 255) {
        const uint32_t value = color.rgb();
        for (int y = y0; y < y1; ++y) {
            uint32_t* row = pixels_.data() + static_cast<size_t>(y) * static_cast<size_t>(width_);
            std::fill(row + x0, row + x1, value);
        }
        return;
    }
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) put(x, y, color, 1.0f);
}

void Canvas::fill_rounded(Rect rect, float radius, Color color) {
    gradient_rounded(rect, radius, color, color);
}

void Canvas::gradient_rounded(Rect rect, float radius, Color top, Color bottom) {
    if (rect.w <= 0 || rect.h <= 0) return;
    radius = std::clamp(radius, 0.0f, static_cast<float>(std::min(rect.w, rect.h)) * 0.5f);
    const Rect c = clip();
    const int x0 = std::max(c.x, rect.x);
    const int y0 = std::max(c.y, rect.y);
    const int x1 = std::min(c.x + c.w, rect.x + rect.w);
    const int y1 = std::min(c.y + c.h, rect.y + rect.h);
    const float cx = rect.x + rect.w * 0.5f;
    const float cy = rect.y + rect.h * 0.5f;
    const float half_w = rect.w * 0.5f;
    const float half_h = rect.h * 0.5f;
    const int band = static_cast<int>(std::ceil(radius)) + 1;
    for (int y = y0; y < y1; ++y) {
        const float t = rect.h > 1 ? static_cast<float>(y - rect.y) / static_cast<float>(rect.h - 1) : 0.0f;
        const Color row_color = mix(top, bottom, t);
        const bool corner_row = radius > 0.0f && (y - rect.y < band || rect.y + rect.h - 1 - y < band);
        for (int x = x0; x < x1; ++x) {
            if (corner_row && (x - rect.x < band || rect.x + rect.w - 1 - x < band)) {
                const float d = rounded_distance(x + 0.5f, y + 0.5f, cx, cy, half_w, half_h, radius);
                const float coverage = std::clamp(0.5f - d, 0.0f, 1.0f);
                if (coverage > 0.0f) put(x, y, row_color, coverage);
            } else {
                put(x, y, row_color, 1.0f);
            }
        }
    }
}

void Canvas::gradient_rect(Rect rect, Color top, Color bottom) {
    gradient_rounded(rect, 0.0f, top, bottom);
}

void Canvas::stroke_rounded(Rect rect, float radius, float thickness, Color color) {
    if (rect.w <= 0 || rect.h <= 0) return;
    radius = std::clamp(radius, 0.0f, static_cast<float>(std::min(rect.w, rect.h)) * 0.5f);
    const Rect c = clip();
    const int x0 = std::max(c.x, rect.x);
    const int y0 = std::max(c.y, rect.y);
    const int x1 = std::min(c.x + c.w, rect.x + rect.w);
    const int y1 = std::min(c.y + c.h, rect.y + rect.h);
    const float cx = rect.x + rect.w * 0.5f;
    const float cy = rect.y + rect.h * 0.5f;
    const float half_w = rect.w * 0.5f;
    const float half_h = rect.h * 0.5f;
    const int band = static_cast<int>(std::ceil(thickness)) + 1;
    for (int y = y0; y < y1; ++y) {
        const bool edge_row = y - rect.y < band || rect.y + rect.h - 1 - y < band;
        for (int x = x0; x < x1; ++x) {
            if (!edge_row && x - rect.x >= band && rect.x + rect.w - 1 - x >= band) {
                x = std::max(x, rect.x + rect.w - band - 1);
                continue;
            }
            const float d = rounded_distance(x + 0.5f, y + 0.5f, cx, cy, half_w, half_h, radius);
            const float coverage = std::clamp(0.5f - std::fabs(d + thickness * 0.5f) + thickness * 0.5f, 0.0f, 1.0f);
            if (coverage > 0.0f) put(x, y, color, coverage);
        }
    }
}

void Canvas::fill_circle(float cx, float cy, float radius, Color color) {
    const Rect c = clip();
    const int x0 = std::max(c.x, static_cast<int>(std::floor(cx - radius - 1.0f)));
    const int y0 = std::max(c.y, static_cast<int>(std::floor(cy - radius - 1.0f)));
    const int x1 = std::min(c.x + c.w, static_cast<int>(std::ceil(cx + radius + 1.0f)));
    const int y1 = std::min(c.y + c.h, static_cast<int>(std::ceil(cy + radius + 1.0f)));
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy) - radius;
            const float coverage = std::clamp(0.5f - d, 0.0f, 1.0f);
            if (coverage > 0.0f) put(x, y, color, coverage);
        }
    }
}

void Canvas::stroke_circle(float cx, float cy, float radius, float thickness, Color color) {
    const float outer = radius + thickness * 0.5f + 1.0f;
    const Rect c = clip();
    const int x0 = std::max(c.x, static_cast<int>(std::floor(cx - outer)));
    const int y0 = std::max(c.y, static_cast<int>(std::floor(cy - outer)));
    const int x1 = std::min(c.x + c.w, static_cast<int>(std::ceil(cx + outer)));
    const int y1 = std::min(c.y + c.h, static_cast<int>(std::ceil(cy + outer)));
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const float d = std::fabs(std::hypot(x + 0.5f - cx, y + 0.5f - cy) - radius);
            const float coverage = std::clamp(thickness * 0.5f + 0.5f - d, 0.0f, 1.0f);
            if (coverage > 0.0f) put(x, y, color, coverage);
        }
    }
}

void Canvas::line(float x0, float y0, float x1, float y1, float thickness, Color color) {
    const float radius = std::max(0.5f, thickness * 0.5f);
    const Rect c = clip();
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const bool horizontal = std::fabs(dx) >= std::fabs(dy);
    const float reach = radius + 1.0f;
    if (horizontal) {
        const float low = std::min(x0, x1) - reach;
        const float high = std::max(x0, x1) + reach;
        const int start = std::max(c.x, static_cast<int>(std::floor(low)));
        const int stop = std::min(c.x + c.w, static_cast<int>(std::ceil(high)));
        const float slope = dx != 0.0f ? dy / dx : 0.0f;
        const float spread = reach * std::sqrt(1.0f + slope * slope) + 1.0f;
        for (int x = start; x < stop; ++x) {
            const float px = x + 0.5f;
            float center = y0;
            if (dx != 0.0f) center = y0 + (std::clamp(px, std::min(x0, x1), std::max(x0, x1)) - x0) * slope;
            const int row_start = std::max(c.y, static_cast<int>(std::floor(center - spread)));
            const int row_stop = std::min(c.y + c.h, static_cast<int>(std::ceil(center + spread)));
            for (int y = row_start; y < row_stop; ++y) {
                const float d = segment_distance(px, y + 0.5f, x0, y0, x1, y1);
                const float coverage = std::clamp(radius + 0.5f - d, 0.0f, 1.0f);
                if (coverage > 0.0f) put(x, y, color, coverage);
            }
        }
    } else {
        const float low = std::min(y0, y1) - reach;
        const float high = std::max(y0, y1) + reach;
        const int start = std::max(c.y, static_cast<int>(std::floor(low)));
        const int stop = std::min(c.y + c.h, static_cast<int>(std::ceil(high)));
        const float slope = dy != 0.0f ? dx / dy : 0.0f;
        const float spread = reach * std::sqrt(1.0f + slope * slope) + 1.0f;
        for (int y = start; y < stop; ++y) {
            const float py = y + 0.5f;
            float center = x0;
            if (dy != 0.0f) center = x0 + (std::clamp(py, std::min(y0, y1), std::max(y0, y1)) - y0) * slope;
            const int col_start = std::max(c.x, static_cast<int>(std::floor(center - spread)));
            const int col_stop = std::min(c.x + c.w, static_cast<int>(std::ceil(center + spread)));
            for (int x = col_start; x < col_stop; ++x) {
                const float d = segment_distance(x + 0.5f, py, x0, y0, x1, y1);
                const float coverage = std::clamp(radius + 0.5f - d, 0.0f, 1.0f);
                if (coverage > 0.0f) put(x, y, color, coverage);
            }
        }
    }
}

void Canvas::polyline(const std::vector<PointF>& points, float thickness, Color color) {
    for (size_t i = 1; i < points.size(); ++i) line(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, thickness, color);
}

void Canvas::fill_polygon(const std::vector<PointF>& points, Color color) {
    fill_polygon(points, [color](float, float) { return color; });
}

void Canvas::fill_polygon(const std::vector<PointF>& points, const std::function<Color(float, float)>& shader) {
    if (points.size() < 3) return;
    float min_x = points[0].x, max_x = points[0].x, min_y = points[0].y, max_y = points[0].y;
    for (const auto& p : points) {
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    const Rect c = clip();
    const int x0 = std::max(c.x, static_cast<int>(std::floor(min_x)) - 1);
    const int y0 = std::max(c.y, static_cast<int>(std::floor(min_y)) - 1);
    const int x1 = std::min(c.x + c.w, static_cast<int>(std::ceil(max_x)) + 1);
    const int y1 = std::min(c.y + c.h, static_cast<int>(std::ceil(max_y)) + 1);
    if (x0 >= x1 || y0 >= y1) return;
    CoverageBuffer buffer(x1 - x0, y1 - y0);
    for (size_t i = 0; i < points.size(); ++i) {
        const PointF& a = points[i];
        const PointF& b = points[(i + 1) % points.size()];
        buffer.line(a.x - x0, a.y - y0, b.x - x0, b.y - y0);
    }
    buffer.resolve([&](int x, int y, float coverage) {
        const int px = x0 + x;
        const int py = y0 + y;
        put(px, py, shader(px + 0.5f, py + 0.5f), coverage);
    });
}

void Canvas::shadow(Rect rect, float radius, int spread, int alpha) {
    for (int i = spread; i >= 1; --i) {
        const float t = 1.0f - static_cast<float>(i) / static_cast<float>(spread + 1);
        const int layer_alpha = static_cast<int>(alpha * t * t / static_cast<float>(spread) * 2.2f);
        fill_rounded(Rect{rect.x - i, rect.y - i + spread / 3, rect.w + i * 2, rect.h + i * 2}, radius + static_cast<float>(i), Color(0, 0, 0, std::clamp(layer_alpha, 0, 255)));
    }
}

void Canvas::blit_coverage(int x, int y, int width, int height, const uint8_t* coverage, Color color) {
    const Rect c = clip();
    const int x0 = std::max(c.x, x);
    const int y0 = std::max(c.y, y);
    const int x1 = std::min(c.x + c.w, x + width);
    const int y1 = std::min(c.y + c.h, y + height);
    for (int py = y0; py < y1; ++py) {
        const uint8_t* source = coverage + static_cast<size_t>(py - y) * static_cast<size_t>(width) + static_cast<size_t>(x0 - x);
        for (int px = x0; px < x1; ++px, ++source) {
            if (*source) put(px, py, color, static_cast<float>(*source) / 255.0f);
        }
    }
}

void Canvas::blit(const Canvas& source, int x, int y, float opacity) {
    const Rect c = clip();
    const int x0 = std::max(c.x, x);
    const int y0 = std::max(c.y, y);
    const int x1 = std::min(c.x + c.w, x + source.width());
    const int y1 = std::min(c.y + c.h, y + source.height());
    const int alpha = static_cast<int>(std::clamp(opacity, 0.0f, 1.0f) * 255.0f + 0.5f);
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            const uint32_t value = source.pixel(px - x, py - y);
            put(px, py, Color(static_cast<int>((value >> 16) & 255), static_cast<int>((value >> 8) & 255), static_cast<int>(value & 255), alpha), 1.0f);
        }
    }
}

}
