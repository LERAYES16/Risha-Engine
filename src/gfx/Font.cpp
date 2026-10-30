#include "risha/gfx/Font.hpp"
#include "risha/gfx/Raster.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace risha::gfx {
namespace {

struct Transform {
    float a{1.0f}, b{0.0f}, c{0.0f}, d{1.0f};
};

}

uint16_t Font::u16(size_t offset) const {
    if (offset + 2 > size_) return 0;
    return static_cast<uint16_t>((data_[offset] << 8) | data_[offset + 1]);
}

int16_t Font::i16(size_t offset) const { return static_cast<int16_t>(u16(offset)); }

uint32_t Font::u32(size_t offset) const {
    if (offset + 4 > size_) return 0;
    return (static_cast<uint32_t>(data_[offset]) << 24) | (static_cast<uint32_t>(data_[offset + 1]) << 16) | (static_cast<uint32_t>(data_[offset + 2]) << 8) | data_[offset + 3];
}

bool Font::load(const uint8_t* data, size_t size) {
    data_ = data;
    size_ = size;
    loaded_ = false;
    if (!data || size < 12) return false;
    const int table_count = u16(4);
    size_t head = 0, maxp = 0, hhea = 0;
    for (int i = 0; i < table_count; ++i) {
        const size_t record = 12 + static_cast<size_t>(i) * 16;
        if (record + 16 > size_) return false;
        const char* tag = reinterpret_cast<const char*>(data_ + record);
        const size_t offset = u32(record + 8);
        const size_t length = u32(record + 12);
        if (offset + length > size_) return false;
        if (!std::memcmp(tag, "head", 4)) head = offset;
        else if (!std::memcmp(tag, "maxp", 4)) maxp = offset;
        else if (!std::memcmp(tag, "hhea", 4)) hhea = offset;
        else if (!std::memcmp(tag, "hmtx", 4)) hmtx_ = offset;
        else if (!std::memcmp(tag, "loca", 4)) loca_ = offset;
        else if (!std::memcmp(tag, "glyf", 4)) { glyf_ = offset; glyf_length_ = length; }
        else if (!std::memcmp(tag, "cmap", 4)) cmap_ = offset;
    }
    if (!head || !maxp || !hhea || !hmtx_ || !loca_ || !glyf_ || !cmap_) return false;
    units_per_em_ = static_cast<float>(u16(head + 18));
    if (units_per_em_ < 16.0f) return false;
    long_loca_ = i16(head + 50) != 0;
    glyph_count_ = u16(maxp + 4);
    ascent_ = static_cast<float>(i16(hhea + 4));
    descent_ = static_cast<float>(i16(hhea + 6));
    line_gap_ = static_cast<float>(i16(hhea + 8));
    metric_count_ = u16(hhea + 34);
    if (glyph_count_ <= 0 || metric_count_ <= 0) return false;
    if (!parse_cmap()) return false;
    loaded_ = true;
    return true;
}

bool Font::parse_cmap() {
    const int count = u16(cmap_ + 2);
    size_t best = 0;
    int best_format = 0;
    for (int i = 0; i < count; ++i) {
        const size_t record = cmap_ + 4 + static_cast<size_t>(i) * 8;
        const int platform = u16(record);
        const int encoding = u16(record + 2);
        const size_t offset = cmap_ + u32(record + 4);
        const int format = u16(offset);
        const bool unicode = platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
        if (!unicode || (format != 4 && format != 12)) continue;
        if (!best || format == 12) { best = offset; best_format = format; }
    }
    cmap_format_offset_ = best;
    cmap_format_ = best_format;
    return best != 0;
}

int Font::glyph_index(char32_t code_point) const {
    if (cmap_format_ == 12) {
        const size_t base = cmap_format_offset_;
        const uint32_t groups = u32(base + 12);
        for (uint32_t i = 0; i < groups; ++i) {
            const size_t group = base + 16 + static_cast<size_t>(i) * 12;
            const uint32_t start = u32(group);
            const uint32_t end = u32(group + 4);
            if (code_point >= start && code_point <= end) return static_cast<int>(u32(group + 8) + (code_point - start));
        }
        return 0;
    }
    if (cmap_format_ == 4) {
        if (code_point > 0xFFFF) return 0;
        const size_t base = cmap_format_offset_;
        const int segment_count = u16(base + 6) / 2;
        const size_t end_codes = base + 14;
        const size_t start_codes = end_codes + static_cast<size_t>(segment_count) * 2 + 2;
        const size_t deltas = start_codes + static_cast<size_t>(segment_count) * 2;
        const size_t range_offsets = deltas + static_cast<size_t>(segment_count) * 2;
        for (int i = 0; i < segment_count; ++i) {
            const uint16_t end = u16(end_codes + static_cast<size_t>(i) * 2);
            if (code_point > end) continue;
            const uint16_t start = u16(start_codes + static_cast<size_t>(i) * 2);
            if (code_point < start) return 0;
            const uint16_t delta = u16(deltas + static_cast<size_t>(i) * 2);
            const uint16_t range = u16(range_offsets + static_cast<size_t>(i) * 2);
            if (range == 0) return static_cast<int>((code_point + delta) & 0xFFFF);
            const size_t address = range_offsets + static_cast<size_t>(i) * 2 + range + static_cast<size_t>(code_point - start) * 2;
            const uint16_t glyph = u16(address);
            return glyph == 0 ? 0 : static_cast<int>((glyph + delta) & 0xFFFF);
        }
    }
    return 0;
}

float Font::advance_units(int glyph) const {
    if (glyph < 0 || glyph >= glyph_count_) glyph = 0;
    const int index = std::min(glyph, metric_count_ - 1);
    return static_cast<float>(u16(hmtx_ + static_cast<size_t>(index) * 4));
}

bool Font::load_contours(int glyph, std::vector<Contour>& contours, int depth) const {
    if (glyph < 0 || glyph >= glyph_count_ || depth > 4) return false;
    size_t start = 0, end = 0;
    if (long_loca_) {
        start = u32(loca_ + static_cast<size_t>(glyph) * 4);
        end = u32(loca_ + static_cast<size_t>(glyph) * 4 + 4);
    } else {
        start = static_cast<size_t>(u16(loca_ + static_cast<size_t>(glyph) * 2)) * 2;
        end = static_cast<size_t>(u16(loca_ + static_cast<size_t>(glyph) * 2 + 2)) * 2;
    }
    if (end <= start) return true;
    if (end > glyf_length_) return false;
    const size_t base = glyf_ + start;
    const int contour_count = i16(base);
    if (contour_count >= 0) {
        std::vector<int> ends(static_cast<size_t>(contour_count));
        for (int i = 0; i < contour_count; ++i) ends[static_cast<size_t>(i)] = u16(base + 10 + static_cast<size_t>(i) * 2);
        const int point_count = contour_count ? ends.back() + 1 : 0;
        if (point_count > 4000) return false;
        size_t cursor = base + 10 + static_cast<size_t>(contour_count) * 2;
        cursor += 2 + u16(cursor);
        std::vector<uint8_t> flags(static_cast<size_t>(point_count));
        for (int i = 0; i < point_count;) {
            if (cursor >= size_) return false;
            const uint8_t flag = data_[cursor++];
            flags[static_cast<size_t>(i++)] = flag;
            if (flag & 8) {
                if (cursor >= size_) return false;
                int repeat = data_[cursor++];
                while (repeat-- > 0 && i < point_count) flags[static_cast<size_t>(i++)] = flag;
            }
        }
        std::vector<float> xs(static_cast<size_t>(point_count)), ys(static_cast<size_t>(point_count));
        int value = 0;
        for (int i = 0; i < point_count; ++i) {
            const uint8_t flag = flags[static_cast<size_t>(i)];
            if (flag & 2) {
                if (cursor >= size_) return false;
                const int delta = data_[cursor++];
                value += (flag & 16) ? delta : -delta;
            } else if (!(flag & 16)) {
                value += i16(cursor);
                cursor += 2;
            }
            xs[static_cast<size_t>(i)] = static_cast<float>(value);
        }
        value = 0;
        for (int i = 0; i < point_count; ++i) {
            const uint8_t flag = flags[static_cast<size_t>(i)];
            if (flag & 4) {
                if (cursor >= size_) return false;
                const int delta = data_[cursor++];
                value += (flag & 32) ? delta : -delta;
            } else if (!(flag & 32)) {
                value += i16(cursor);
                cursor += 2;
            }
            ys[static_cast<size_t>(i)] = static_cast<float>(value);
        }
        int first = 0;
        for (int c = 0; c < contour_count; ++c) {
            Contour contour;
            for (int i = first; i <= ends[static_cast<size_t>(c)]; ++i) contour.push_back(Point{xs[static_cast<size_t>(i)], ys[static_cast<size_t>(i)], (flags[static_cast<size_t>(i)] & 1) != 0});
            first = ends[static_cast<size_t>(c)] + 1;
            if (contour.size() >= 2) contours.push_back(std::move(contour));
        }
        return true;
    }
    size_t cursor = base + 10;
    while (true) {
        const uint16_t flags = u16(cursor);
        const int component = u16(cursor + 2);
        cursor += 4;
        float dx = 0.0f, dy = 0.0f;
        if (flags & 1) {
            if (flags & 2) { dx = i16(cursor); dy = i16(cursor + 2); }
            cursor += 4;
        } else {
            if (flags & 2) { dx = static_cast<int8_t>(data_[cursor]); dy = static_cast<int8_t>(data_[cursor + 1]); }
            cursor += 2;
        }
        Transform t;
        auto f2dot14 = [&](size_t offset) { return static_cast<float>(i16(offset)) / 16384.0f; };
        if (flags & 8) { t.a = t.d = f2dot14(cursor); cursor += 2; }
        else if (flags & 0x40) { t.a = f2dot14(cursor); t.d = f2dot14(cursor + 2); cursor += 4; }
        else if (flags & 0x80) { t.a = f2dot14(cursor); t.b = f2dot14(cursor + 2); t.c = f2dot14(cursor + 4); t.d = f2dot14(cursor + 6); cursor += 8; }
        std::vector<Contour> part;
        if (!load_contours(component, part, depth + 1)) return false;
        for (auto& contour : part) {
            for (auto& point : contour) {
                const float x = point.x * t.a + point.y * t.c + dx;
                const float y = point.x * t.b + point.y * t.d + dy;
                point.x = x;
                point.y = y;
            }
            contours.push_back(std::move(contour));
        }
        if (!(flags & 0x20)) break;
    }
    return true;
}

GlyphBounds Font::bounds(int glyph) const {
    GlyphBounds result;
    std::vector<Contour> contours;
    if (!load_contours(glyph, contours, 0)) return result;
    for (const auto& contour : contours) {
        for (const auto& point : contour) {
            if (result.empty) {
                result = GlyphBounds{point.x, point.y, point.x, point.y, false};
            } else {
                result.x_min = std::min(result.x_min, point.x);
                result.y_min = std::min(result.y_min, point.y);
                result.x_max = std::max(result.x_max, point.x);
                result.y_max = std::max(result.y_max, point.y);
            }
        }
    }
    return result;
}

bool Font::rasterize(int glyph, float pixel_size, float subpixel_x, GlyphBitmap& out) const {
    out = GlyphBitmap{};
    std::vector<Contour> contours;
    if (!load_contours(glyph, contours, 0)) return false;
    if (contours.empty()) return true;
    const float scale = pixel_size / units_per_em_;
    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
    for (const auto& contour : contours) {
        for (const auto& point : contour) {
            min_x = std::min(min_x, point.x * scale);
            max_x = std::max(max_x, point.x * scale);
            min_y = std::min(min_y, -point.y * scale);
            max_y = std::max(max_y, -point.y * scale);
        }
    }
    const int left = static_cast<int>(std::floor(min_x + subpixel_x)) - 1;
    const int top = static_cast<int>(std::floor(min_y)) - 1;
    const int right = static_cast<int>(std::ceil(max_x + subpixel_x)) + 1;
    const int bottom = static_cast<int>(std::ceil(max_y)) + 1;
    const int width = right - left;
    const int height = bottom - top;
    if (width <= 0 || height <= 0 || width > 512 || height > 512) return true;
    CoverageBuffer buffer(width, height);
    auto add = [&](float x0, float y0, float x1, float y1) {
        buffer.line(x0 * scale + subpixel_x - left, -y0 * scale - top, x1 * scale + subpixel_x - left, -y1 * scale - top);
    };
    for (const auto& contour : contours) {
        const size_t count = contour.size();
        size_t first_on = count;
        for (size_t i = 0; i < count; ++i) if (contour[i].on_curve) { first_on = i; break; }
        Point start{};
        size_t begin = 0;
        if (first_on < count) {
            start = contour[first_on];
            begin = first_on + 1;
        } else {
            start = Point{(contour[0].x + contour[count - 1].x) * 0.5f, (contour[0].y + contour[count - 1].y) * 0.5f, true};
            begin = 0;
        }
        Point current = start;
        Point control{};
        bool have_control = false;
        const size_t steps = first_on < count ? count - 1 : count;
        auto curve = [&](Point from, Point ctrl, Point to) {
            const float span = std::fabs(from.x - to.x) + std::fabs(from.y - to.y) + std::fabs(ctrl.x - from.x) + std::fabs(ctrl.y - from.y);
            const int segments = std::clamp(static_cast<int>(span * scale * 0.5f) + 2, 2, 24);
            Point previous = from;
            for (int s = 1; s <= segments; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(segments);
                const float u = 1.0f - t;
                const Point next{u * u * from.x + 2.0f * u * t * ctrl.x + t * t * to.x, u * u * from.y + 2.0f * u * t * ctrl.y + t * t * to.y, true};
                add(previous.x, previous.y, next.x, next.y);
                previous = next;
            }
        };
        for (size_t k = 0; k < steps; ++k) {
            const Point point = contour[(begin + k) % count];
            if (point.on_curve) {
                if (have_control) curve(current, control, point);
                else add(current.x, current.y, point.x, point.y);
                current = point;
                have_control = false;
            } else {
                if (have_control) {
                    const Point middle{(control.x + point.x) * 0.5f, (control.y + point.y) * 0.5f, true};
                    curve(current, control, middle);
                    current = middle;
                }
                control = point;
                have_control = true;
            }
        }
        if (have_control) curve(current, control, start);
        else add(current.x, current.y, start.x, start.y);
    }
    out.left = left;
    out.top = top;
    out.width = width;
    out.height = height;
    out.alpha.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    buffer.resolve([&](int x, int y, float coverage) {
        out.alpha[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = static_cast<uint8_t>(coverage * 255.0f + 0.5f);
    });
    return true;
}

}
