#pragma once
#include "risha/gfx/Canvas.hpp"
#include "risha/gfx/Font.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace risha::gfx {

enum class Direction { Auto, LeftToRight, RightToLeft };
enum class Anchor { Left, Center, Right };

struct TextLayout {
    struct Glyph {
        int glyph{0};
        float x{0.0f};
        float y{0.0f};
    };
    std::vector<Glyph> glyphs;
    std::vector<float> caret;
    std::vector<bool> char_rtl;
    float width{0.0f};
    bool rtl{false};
};

class TextEngine {
public:
    static TextEngine& instance();

    const TextLayout& layout(const std::u32string& text, float size, Direction direction = Direction::Auto);
    float ascent(float size) const;
    float descent(float size) const;
    float line_height(float size) const;
    void draw(Canvas& canvas, const TextLayout& layout, float x, float baseline, float size, Color color, bool bold = false);
    const Font& font() const { return font_; }
    size_t caret_index_at(const TextLayout& layout, float x) const;

private:
    TextEngine();
    const GlyphBitmap& bitmap(int glyph, float size, int phase);
    TextLayout build(const std::u32string& text, float size, Direction direction);
    Font font_;
    std::unordered_map<unsigned long long, GlyphBitmap> glyph_cache_;
    std::unordered_map<std::u32string, TextLayout> layout_cache_;
};

float text_width(const std::wstring& text, float size, Direction direction = Direction::Auto);
float text_ascent(float size);
float text_height(float size);
const TextLayout& text_layout(const std::wstring& text, float size, Direction direction = Direction::Auto);
void draw_text(Canvas& canvas, const std::wstring& text, float x, float baseline, float size, Color color, Anchor anchor = Anchor::Left, bool bold = false, Direction direction = Direction::Auto);
bool is_rtl_text(const std::wstring& text);

}
