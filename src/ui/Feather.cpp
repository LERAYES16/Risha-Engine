#include "risha/ui/Feather.hpp"
#include <algorithm>
#include <cmath>

namespace risha::ui {
namespace {

using gfx::Color;
using gfx::PointF;

struct Vec {
    double x{0.0};
    double y{0.0};
};

Vec bezier(const Vec* p, double t) {
    const double u = 1.0 - t;
    const double a = u * u * u, b = 3.0 * u * u * t, c = 3.0 * u * t * t, d = t * t * t;
    return Vec{a * p[0].x + b * p[1].x + c * p[2].x + d * p[3].x, a * p[0].y + b * p[1].y + c * p[2].y + d * p[3].y};
}

Vec bezier_tangent(const Vec* p, double t) {
    const double u = 1.0 - t;
    Vec v{3.0 * u * u * (p[1].x - p[0].x) + 6.0 * u * t * (p[2].x - p[1].x) + 3.0 * t * t * (p[3].x - p[2].x),
          3.0 * u * u * (p[1].y - p[0].y) + 6.0 * u * t * (p[2].y - p[1].y) + 3.0 * t * t * (p[3].y - p[2].y)};
    const double length = std::max(1e-9, std::hypot(v.x, v.y));
    return Vec{v.x / length, v.y / length};
}

double hash(int n, int salt) {
    unsigned int h = static_cast<unsigned int>(n) * 374761393u + static_cast<unsigned int>(salt) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<double>(h & 0xFFFFFF) / static_cast<double>(0xFFFFFF);
}

double half_width(double t, double side) {
    const double start = 0.07;
    if (t <= start) return 0.0;
    const double u = std::clamp((t - start) / (1.0 - start), 0.0, 1.0);
    const double rise = std::pow(std::sin(u * 3.14159265 * 0.5), 0.8);
    const double fall = std::pow(std::cos(std::pow(u, 1.55) * 3.14159265 * 0.5), 0.62);
    const double notch = 1.0 - 0.10 * std::exp(-std::pow((u - 0.8) / 0.05, 2.0)) * (side > 0 ? 1.0 : 0.4);
    return rise * fall * notch;
}

Color shade(double along, double across, double noise, int alpha) {
    const Color root(247, 244, 236);
    const Color mid(214, 224, 238);
    const Color edge(132, 154, 188);
    Color base = across < 0.5 ? gfx::mix(root, mid, static_cast<float>(across * 2.0)) : gfx::mix(mid, edge, static_cast<float>((across - 0.5) * 2.0));
    base = gfx::scale(base, static_cast<float>(1.0 - 0.10 * along + (noise - 0.5) * 0.16));
    return base.with_alpha(alpha);
}

}

void draw_feather(gfx::Canvas& canvas, float cx, float cy, float length, float progress, double time) {
    const double p = std::clamp(static_cast<double>(progress), 0.0, 1.0);
    if (p <= 0.0) return;
    const double sway = std::sin(time * 1.1) * 0.018;
    const double angle = -0.62 + sway;
    const double ca = std::cos(angle), sa = std::sin(angle);
    auto place = [&](double x, double y) { return Vec{cx + x * ca - y * sa, cy + x * sa + y * ca}; };
    const double L = length;
    const Vec ctl[4] = {place(-L * 0.5, 0.0), place(-L * 0.16, -L * 0.07), place(L * 0.2, -L * 0.10), place(L * 0.5, L * 0.02)};
    const double span = L * 0.205;
    const double ease = p < 0.5 ? 2.0 * p * p : 1.0 - std::pow(-2.0 * p + 2.0, 2.0) / 2.0;
    const double reach = std::clamp(ease * 1.04, 0.0, 1.0);

    const int barbs = 150;
    std::vector<PointF> left_edge, right_edge;
    for (int i = 0; i <= barbs; ++i) {
        const double t = static_cast<double>(i) / barbs;
        if (t > reach) break;
        const Vec c = bezier(ctl, t);
        const Vec tg = bezier_tangent(ctl, t);
        const Vec n{-tg.y, tg.x};
        const double grow = std::clamp((reach - t) / 0.06, 0.0, 1.0);
        const double wl = half_width(t, 1.0) * span * 1.0 * grow;
        const double wr = half_width(t, -1.0) * span * 0.66 * grow;
        const double sweep_l = wl * 0.55, sweep_r = wr * 0.55;
        left_edge.push_back(PointF{static_cast<float>(c.x + n.x * wl + tg.x * sweep_l), static_cast<float>(c.y + n.y * wl + tg.y * sweep_l)});
        right_edge.push_back(PointF{static_cast<float>(c.x - n.x * wr + tg.x * sweep_r), static_cast<float>(c.y - n.y * wr + tg.y * sweep_r)});
    }
    if (left_edge.size() > 3) {
        std::vector<PointF> body;
        const Vec base = bezier(ctl, 0.07);
        body.push_back(PointF{static_cast<float>(base.x), static_cast<float>(base.y)});
        for (const auto& point : left_edge) body.push_back(point);
        for (auto it = right_edge.rbegin(); it != right_edge.rend(); ++it) body.push_back(*it);
        canvas.fill_polygon(body, [&](float x, float y) {
            const double d = std::hypot(x - cx, y - cy) / (L * 0.6);
            return gfx::mix(Color(232, 238, 248, 70), Color(150, 170, 205, 46), static_cast<float>(std::clamp(d, 0.0, 1.0)));
        });
    }

    for (int side = 0; side < 2; ++side) {
        const double sign = side == 0 ? 1.0 : -1.0;
        const double side_scale = side == 0 ? 1.0 : 0.66;
        for (int i = 0; i < barbs; ++i) {
            const double t = (static_cast<double>(i) + 0.5) / barbs;
            if (t > reach) break;
            const double w = half_width(t, sign);
            if (w <= 0.01) continue;
            const double grow = std::clamp((reach - t) / 0.06, 0.0, 1.0);
            const Vec c = bezier(ctl, t);
            const Vec tg = bezier_tangent(ctl, t);
            const Vec n{-tg.y * sign, tg.x * sign};
            const double jitter = hash(i, side + 3);
            const double jitter2 = hash(i, side + 11);
            const bool split = hash(i / 3, side + 29) < 0.07;
            double barb_length = w * span * side_scale * (0.94 + 0.10 * jitter) * grow;
            const double sweep = 0.42 + 0.30 * t + (jitter2 - 0.5) * 0.10 + (split ? 0.22 : 0.0);
            const double bend = 0.12 + 0.18 * jitter2;
            const Vec end{c.x + n.x * barb_length + tg.x * barb_length * sweep, c.y + n.y * barb_length + tg.y * barb_length * sweep};
            const Vec mid{c.x + n.x * barb_length * 0.55 + tg.x * barb_length * (sweep * 0.32 - bend * 0.35), c.y + n.y * barb_length * 0.55 + tg.y * barb_length * (sweep * 0.32 - bend * 0.35)};
            std::vector<PointF> curve;
            const int steps = 7;
            for (int s = 0; s <= steps; ++s) {
                const double u = static_cast<double>(s) / steps;
                const double a = (1 - u) * (1 - u), b = 2 * (1 - u) * u, d = u * u;
                curve.push_back(PointF{static_cast<float>(a * c.x + b * mid.x + d * end.x), static_cast<float>(a * c.y + b * mid.y + d * end.y)});
            }
            const double noise = hash(i, side + 41);
            const int alpha = static_cast<int>((150 + 80 * noise) * (split ? 0.7 : 1.0));
            for (int s = 0; s < steps; ++s) {
                const double along = static_cast<double>(s) / steps;
                canvas.line(curve[static_cast<size_t>(s)].x, curve[static_cast<size_t>(s)].y, curve[static_cast<size_t>(s) + 1].x, curve[static_cast<size_t>(s) + 1].y, 1.25f,
                            shade(t, along, noise, alpha));
            }
            if (i % 5 == 0) {
                const PointF a = curve[2], b = curve[5];
                canvas.line(a.x, a.y, b.x, b.y, 0.9f, Color(255, 255, 255, 70));
            }
        }
    }

    if (p > 0.04) {
        const double base_fuzz_end = std::min(reach, 0.30);
        for (int i = 0; i < 40; ++i) {
            const double t = 0.06 + base_fuzz_end * 0.8 * hash(i, 77) * 0.6;
            if (t > reach) continue;
            const Vec c = bezier(ctl, t);
            const Vec tg = bezier_tangent(ctl, t);
            const double side = hash(i, 5) < 0.5 ? 1.0 : -1.0;
            const Vec n{-tg.y * side, tg.x * side};
            const double len = L * (0.03 + 0.05 * hash(i, 9));
            const double a = 0.9 + hash(i, 13) * 0.9;
            const double ex = c.x + (n.x * std::cos(a) + tg.x * std::sin(a) * -1.0) * len;
            const double ey = c.y + (n.y * std::cos(a) + tg.y * std::sin(a) * -1.0) * len;
            canvas.line(static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(ex), static_cast<float>(ey), 1.0f, Color(236, 238, 242, 120));
        }
    }

    std::vector<PointF> shaft_left, shaft_right;
    const int shaft_steps = 90;
    for (int i = 0; i <= shaft_steps; ++i) {
        const double t = static_cast<double>(i) / shaft_steps * std::min(1.0, reach);
        const Vec c = bezier(ctl, t);
        const Vec tg = bezier_tangent(ctl, t);
        const Vec n{-tg.y, tg.x};
        const double thickness = 1.0 + 3.6 * std::pow(1.0 - t, 1.2) + 0.4;
        shaft_left.push_back(PointF{static_cast<float>(c.x + n.x * thickness), static_cast<float>(c.y + n.y * thickness)});
        shaft_right.push_back(PointF{static_cast<float>(c.x - n.x * thickness), static_cast<float>(c.y - n.y * thickness)});
    }
    if (shaft_left.size() > 2) {
        std::vector<PointF> poly = shaft_left;
        for (auto it = shaft_right.rbegin(); it != shaft_right.rend(); ++it) poly.push_back(*it);
        canvas.fill_polygon(poly, [&](float x, float y) {
            const double d = std::hypot(x - ctl[0].x, y - ctl[0].y) / L;
            return gfx::mix(Color(238, 226, 196), Color(196, 186, 168), static_cast<float>(std::clamp(d, 0.0, 1.0)));
        });
        std::vector<PointF> highlight;
        for (int i = 0; i <= shaft_steps; ++i) {
            const double t = static_cast<double>(i) / shaft_steps * std::min(1.0, reach);
            const Vec c = bezier(ctl, t);
            const Vec tg = bezier_tangent(ctl, t);
            const double off = (1.0 + 3.0 * std::pow(1.0 - t, 1.2)) * 0.35;
            highlight.push_back(PointF{static_cast<float>(c.x - tg.y * off), static_cast<float>(c.y + tg.x * off)});
        }
        canvas.polyline(highlight, 0.9f, Color(255, 252, 240, 190));
    }

    const double quill_end = 0.075;
    const double base_t = 0.0;
    std::vector<PointF> quill_l, quill_r;
    for (int i = 0; i <= 12; ++i) {
        const double t = base_t + (quill_end - base_t) * i / 12.0;
        const Vec c = bezier(ctl, t);
        const Vec tg = bezier_tangent(ctl, t);
        const Vec n{-tg.y, tg.x};
        const double radius = 4.4;
        quill_l.push_back(PointF{static_cast<float>(c.x + n.x * radius), static_cast<float>(c.y + n.y * radius)});
        quill_r.push_back(PointF{static_cast<float>(c.x - n.x * radius), static_cast<float>(c.y - n.y * radius)});
    }
    std::vector<PointF> quill = quill_l;
    for (auto it = quill_r.rbegin(); it != quill_r.rend(); ++it) quill.push_back(*it);
    canvas.fill_polygon(quill, [&](float x, float y) {
        const double d = std::hypot(x - ctl[0].x, y - ctl[0].y) / (L * 0.08);
        return gfx::mix(Color(248, 240, 220, 235), Color(208, 192, 160, 235), static_cast<float>(std::clamp(d, 0.0, 1.0)));
    });
    canvas.line(quill_l.front().x, quill_l.front().y, quill_r.front().x, quill_r.front().y, 1.2f, Color(120, 108, 90, 200));
}

}
