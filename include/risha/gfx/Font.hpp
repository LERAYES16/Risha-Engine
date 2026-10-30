#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace risha::gfx {

struct GlyphBitmap {
    int left{0};
    int top{0};
    int width{0};
    int height{0};
    std::vector<uint8_t> alpha;
};

struct GlyphBounds {
    float x_min{0.0f};
    float y_min{0.0f};
    float x_max{0.0f};
    float y_max{0.0f};
    bool empty{true};
};

class Font {
public:
    bool load(const uint8_t* data, size_t size);
    bool loaded() const { return loaded_; }
    int glyph_index(char32_t code_point) const;
    int glyph_count() const { return glyph_count_; }
    float advance_units(int glyph) const;
    float units_per_em() const { return units_per_em_; }
    float ascent_units() const { return ascent_; }
    float descent_units() const { return descent_; }
    float line_gap_units() const { return line_gap_; }
    GlyphBounds bounds(int glyph) const;
    bool rasterize(int glyph, float pixel_size, float subpixel_x, GlyphBitmap& out) const;

private:
    struct Point {
        float x;
        float y;
        bool on_curve;
    };
    using Contour = std::vector<Point>;

    bool load_contours(int glyph, std::vector<Contour>& contours, int depth) const;
    uint16_t u16(size_t offset) const;
    int16_t i16(size_t offset) const;
    uint32_t u32(size_t offset) const;
    bool parse_cmap();

    const uint8_t* data_{nullptr};
    size_t size_{0};
    bool loaded_{false};
    float units_per_em_{2048.0f};
    float ascent_{0.0f};
    float descent_{0.0f};
    float line_gap_{0.0f};
    int glyph_count_{0};
    int metric_count_{0};
    bool long_loca_{false};
    size_t glyf_{0};
    size_t glyf_length_{0};
    size_t loca_{0};
    size_t hmtx_{0};
    size_t cmap_{0};
    size_t cmap_format_offset_{0};
    int cmap_format_{0};
};

}
