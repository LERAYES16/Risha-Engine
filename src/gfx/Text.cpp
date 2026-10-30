#include "risha/gfx/Text.hpp"
#include "risha/core/Text.hpp"
#include "risha/gfx/FontData.hpp"
#include <algorithm>
#include <cmath>

namespace risha::gfx {
namespace {

struct JoinForms {
    char32_t base;
    char32_t isolated;
    char32_t final_form;
    char32_t initial;
    char32_t medial;
};

const JoinForms kJoinTable[] = {
    {0x0621, 0xFE80, 0, 0, 0}, {0x0622, 0xFE81, 0xFE82, 0, 0}, {0x0623, 0xFE83, 0xFE84, 0, 0}, {0x0624, 0xFE85, 0xFE86, 0, 0},
    {0x0625, 0xFE87, 0xFE88, 0, 0}, {0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C}, {0x0627, 0xFE8D, 0xFE8E, 0, 0},
    {0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92}, {0x0629, 0xFE93, 0xFE94, 0, 0}, {0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98},
    {0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C}, {0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0}, {0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4},
    {0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8}, {0x062F, 0xFEA9, 0xFEAA, 0, 0}, {0x0630, 0xFEAB, 0xFEAC, 0, 0},
    {0x0631, 0xFEAD, 0xFEAE, 0, 0}, {0x0632, 0xFEAF, 0xFEB0, 0, 0}, {0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4},
    {0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8}, {0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC}, {0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0},
    {0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4}, {0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8}, {0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC},
    {0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0}, {0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4}, {0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8},
    {0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC}, {0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0}, {0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4},
    {0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8}, {0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC}, {0x0648, 0xFEED, 0xFEEE, 0, 0},
    {0x0649, 0xFEEF, 0xFEF0, 0, 0}, {0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4}, {0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59},
    {0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D}, {0x0698, 0xFB8A, 0xFB8B, 0, 0}, {0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91},
    {0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95}, {0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF},
};

const JoinForms* find_join(char32_t c) {
    for (const auto& entry : kJoinTable) if (entry.base == c) return &entry;
    return nullptr;
}

bool is_mark(char32_t c) {
    return (c >= 0x064B && c <= 0x065F) || c == 0x0670 || (c >= 0x0610 && c <= 0x061A) || (c >= 0x06D6 && c <= 0x06DC) ||
           (c >= 0x06DF && c <= 0x06E4) || c == 0x06E7 || c == 0x06E8 || (c >= 0x06EA && c <= 0x06ED);
}

bool joins_forward(char32_t c) {
    if (c == 0x0640 || c == 0x200D) return true;
    const JoinForms* forms = find_join(c);
    return forms && forms->initial != 0;
}

bool joins_backward(char32_t c) {
    if (c == 0x0640 || c == 0x200D) return true;
    return find_join(c) != nullptr && c != 0x0621;
}

enum class Cls : unsigned char { L, R, EN, AN, WS, ON, ZERO };

Cls classify(char32_t c) {
    if (c == 0x200E) return Cls::L;
    if (c == 0x200F) return Cls::R;
    if (c == 0x200C || c == 0x200D) return Cls::ZERO;
    if (c == L' ' || c == L'\t' || c == 0xA0) return Cls::WS;
    if (c >= L'0' && c <= L'9') return Cls::EN;
    if ((c >= 0x0660 && c <= 0x0669) || (c >= 0x06F0 && c <= 0x06F9)) return Cls::AN;
    if (c == 0x060C) return Cls::ON;
    if ((c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF)) return Cls::R;
    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) return Cls::L;
    if (c < 0x80) return Cls::ON;
    if (c >= 0x00C0 && c <= 0x024F) return Cls::L;
    if ((c >= 0x00A0 && c <= 0x00BF) || c == 0xD7 || c == 0xF7) return Cls::ON;
    if ((c >= 0x2000 && c <= 0x2BFF) || (c >= 0x3000 && c <= 0x303F)) return Cls::ON;
    return Cls::L;
}

char32_t mirror(char32_t c) {
    switch (c) {
        case U'(': return U')';
        case U')': return U'(';
        case U'[': return U']';
        case U']': return U'[';
        case U'{': return U'}';
        case U'}': return U'{';
        case U'<': return U'>';
        case U'>': return U'<';
        case 0xAB: return 0xBB;
        case 0xBB: return 0xAB;
        default: return c;
    }
}

struct Unit {
    int glyph{0};
    float advance{0.0f};
    size_t start{0};
    size_t end{0};
    char32_t code{0};
    Cls cls{Cls::ON};
    int level{0};
    std::vector<int> marks;
    bool zero{false};
};

}

TextEngine& TextEngine::instance() {
    static TextEngine engine;
    return engine;
}

TextEngine::TextEngine() {
    font_.load(kFontData, static_cast<size_t>(kFontDataSize));
}

float TextEngine::ascent(float size) const { return font_.ascent_units() * size / font_.units_per_em(); }
float TextEngine::descent(float size) const { return -font_.descent_units() * size / font_.units_per_em(); }
float TextEngine::line_height(float size) const { return (font_.ascent_units() - font_.descent_units() + font_.line_gap_units()) * size / font_.units_per_em(); }

TextLayout TextEngine::build(const std::u32string& text, float size, Direction direction) {
    TextLayout result;
    const float scale = size / font_.units_per_em();
    const size_t count = text.size();
    std::vector<Unit> units;
    units.reserve(count);

    auto next_base = [&](size_t from) -> char32_t {
        for (size_t j = from; j < count; ++j) if (!is_mark(text[j])) return text[j];
        return 0;
    };

    bool previous_joins = false;
    for (size_t i = 0; i < count; ++i) {
        const char32_t c = text[i];
        if (is_mark(c)) {
            const int glyph = font_.glyph_index(c);
            if (!units.empty() && !units.back().zero) {
                units.back().marks.push_back(glyph);
                units.back().end = i + 1;
            } else {
                Unit unit;
                unit.glyph = glyph;
                unit.start = i;
                unit.end = i + 1;
                unit.code = c;
                unit.cls = Cls::R;
                units.push_back(std::move(unit));
            }
            continue;
        }
        Unit unit;
        unit.start = i;
        unit.end = i + 1;
        unit.code = c;
        unit.cls = classify(c);
        const JoinForms* forms = find_join(c);
        if (c == 0x200C || c == 0x200D || c == 0x200E || c == 0x200F) {
            unit.glyph = -1;
            unit.zero = true;
            if (c == 0x200D) previous_joins = true;
            else if (c == 0x200C) previous_joins = false;
            units.push_back(std::move(unit));
            continue;
        }
        if (forms || c == 0x0640) {
            const bool join_prev = previous_joins && joins_backward(c);
            const char32_t following = next_base(i + 1);
            bool join_next = false;
            if (c == 0x0644 && (following == 0x0622 || following == 0x0623 || following == 0x0625 || following == 0x0627)) {
                size_t alef_index = i + 1;
                while (alef_index < count && is_mark(text[alef_index])) ++alef_index;
                char32_t ligature = 0;
                if (following == 0x0622) ligature = join_prev ? 0xFEF6 : 0xFEF5;
                else if (following == 0x0623) ligature = join_prev ? 0xFEF8 : 0xFEF7;
                else if (following == 0x0625) ligature = join_prev ? 0xFEFA : 0xFEF9;
                else ligature = join_prev ? 0xFEFC : 0xFEFB;
                const int glyph = font_.glyph_index(ligature);
                if (glyph > 0) {
                    unit.glyph = glyph;
                    unit.end = alef_index + 1;
                    i = alef_index;
                    unit.cls = Cls::R;
                    unit.advance = font_.advance_units(glyph) * scale;
                    previous_joins = false;
                    units.push_back(std::move(unit));
                    continue;
                }
            }
            if (forms && forms->initial != 0) join_next = joins_backward(following) && following != 0;
            if (c == 0x0640) join_next = joins_backward(following) && following != 0;
            char32_t form = c;
            if (forms) {
                if (join_prev && join_next && forms->medial) form = forms->medial;
                else if (join_prev && forms->final_form) form = forms->final_form;
                else if (join_next && forms->initial) form = forms->initial;
                else form = forms->isolated;
            }
            int glyph = font_.glyph_index(form);
            if (glyph == 0) glyph = font_.glyph_index(c);
            unit.glyph = glyph;
            unit.cls = Cls::R;
            unit.advance = font_.advance_units(glyph) * scale;
            previous_joins = joins_forward(c) && join_next;
            units.push_back(std::move(unit));
            continue;
        }
        previous_joins = false;
        int glyph = font_.glyph_index(c);
        if (c == 0x0A0 || c == U'\t') glyph = font_.glyph_index(U' ');
        unit.glyph = glyph;
        unit.advance = font_.advance_units(glyph) * scale * (c == U'\t' ? 4.0f : 1.0f);
        units.push_back(std::move(unit));
    }

    const size_t n = units.size();
    bool paragraph_rtl = direction == Direction::RightToLeft;
    if (direction == Direction::Auto) {
        for (const auto& unit : units) {
            if (unit.cls == Cls::R) { paragraph_rtl = true; break; }
            if (unit.cls == Cls::L) break;
        }
    }
    result.rtl = paragraph_rtl;

    Cls last_strong = paragraph_rtl ? Cls::R : Cls::L;
    for (auto& unit : units) {
        if (unit.cls == Cls::L || unit.cls == Cls::R) last_strong = unit.cls;
        else if (unit.cls == Cls::EN && last_strong == Cls::L) unit.cls = Cls::L;
    }
    for (size_t i = 1; i + 1 < n; ++i) {
        const Cls prev = units[i - 1].cls;
        const Cls next = units[i + 1].cls;
        const char32_t c = units[i].code;
        if (units[i].cls == Cls::ON && (prev == Cls::EN || prev == Cls::AN) && prev == next && (c == U'.' || c == U',' || c == U':' || c == U'/' || c == 0x066B || c == 0x066C)) units[i].cls = prev;
    }
    for (size_t i = 0; i < n; ++i) {
        const char32_t c = units[i].code;
        if (units[i].cls == Cls::ON && c == U'%' && i > 0 && units[i - 1].cls == Cls::EN) units[i].cls = Cls::EN;
        if (units[i].cls == Cls::ON && (c == U'$' || c == U'#') && i + 1 < n && units[i + 1].cls == Cls::EN) units[i].cls = Cls::EN;
    }
    auto effective = [&](size_t i) -> Cls {
        const Cls c = units[i].cls;
        if (c == Cls::EN || c == Cls::AN) return Cls::R;
        return c;
    };
    for (size_t i = 0; i < n;) {
        if (units[i].cls != Cls::WS && units[i].cls != Cls::ON && units[i].cls != Cls::ZERO) { ++i; continue; }
        size_t j = i;
        while (j < n && (units[j].cls == Cls::WS || units[j].cls == Cls::ON || units[j].cls == Cls::ZERO)) ++j;
        Cls left = paragraph_rtl ? Cls::R : Cls::L;
        Cls right = left;
        if (i > 0) left = effective(i - 1);
        if (j < n) right = effective(j);
        const Cls resolved = left == right ? left : (paragraph_rtl ? Cls::R : Cls::L);
        for (size_t k = i; k < j; ++k) units[k].cls = resolved;
        i = j;
    }
    for (auto& unit : units) {
        if (unit.cls == Cls::R) unit.level = 1;
        else if (unit.cls == Cls::L) unit.level = paragraph_rtl ? 2 : 0;
        else unit.level = 2;
    }
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i) order[i] = i;
    int max_level = 0;
    int min_odd = 100;
    for (const auto& unit : units) {
        max_level = std::max(max_level, unit.level);
        if (unit.level & 1) min_odd = std::min(min_odd, unit.level);
    }
    for (int level = max_level; level >= min_odd && level >= 1; --level) {
        for (size_t i = 0; i < n;) {
            if (units[order[i]].level < level) { ++i; continue; }
            size_t j = i;
            while (j < n && units[order[j]].level >= level) ++j;
            std::reverse(order.begin() + static_cast<std::ptrdiff_t>(i), order.begin() + static_cast<std::ptrdiff_t>(j));
            i = j;
        }
    }

    result.caret.assign(count + 1, 0.0f);
    result.char_rtl.assign(count + 1, paragraph_rtl);
    std::vector<float> left_edge(n, 0.0f), right_edge(n, 0.0f);
    float pen = 0.0f;
    for (size_t visual = 0; visual < n; ++visual) {
        const size_t index = order[visual];
        Unit& unit = units[index];
        left_edge[index] = pen;
        if (unit.glyph >= 0 && !unit.zero) {
            const bool mirrored = (unit.level & 1) && mirror(unit.code) != unit.code;
            const int glyph = mirrored ? font_.glyph_index(mirror(unit.code)) : unit.glyph;
            if (unit.advance > 0.0f || unit.marks.empty()) {
                result.glyphs.push_back(TextLayout::Glyph{glyph, pen, 0.0f});
            }
            if (!unit.marks.empty()) {
                const GlyphBounds base = font_.bounds(glyph);
                float top = base.empty ? 0.0f : base.y_max;
                float bottom = base.empty ? 0.0f : base.y_min;
                const float base_center = base.empty ? unit.advance / scale * 0.5f : (base.x_min + base.x_max) * 0.5f;
                for (int mark_glyph : unit.marks) {
                    const GlyphBounds mark = font_.bounds(mark_glyph);
                    if (mark.empty) continue;
                    float shift = 0.0f;
                    if (mark.y_min > 0.0f) {
                        shift = std::max(0.0f, top + 60.0f - mark.y_min);
                        top = mark.y_max + shift;
                    } else {
                        shift = std::min(0.0f, bottom - 60.0f - mark.y_max);
                        bottom = mark.y_min + shift;
                    }
                    const float mark_center = (mark.x_min + mark.x_max) * 0.5f;
                    result.glyphs.push_back(TextLayout::Glyph{mark_glyph, pen + (base_center - mark_center) * scale, -shift * scale});
                }
            }
        }
        pen += unit.advance;
        right_edge[index] = pen;
    }
    result.width = pen;
    for (size_t index = 0; index < n; ++index) {
        const Unit& unit = units[index];
        const bool odd = (unit.level & 1) != 0;
        for (size_t k = unit.start; k < unit.end && k < count; ++k) {
            const float lead = odd ? right_edge[index] : left_edge[index];
            const float trail = odd ? left_edge[index] : right_edge[index];
            const float t = unit.end - unit.start > 1 ? static_cast<float>(k - unit.start) / static_cast<float>(unit.end - unit.start) : 0.0f;
            result.caret[k] = lead + (trail - lead) * t;
            result.char_rtl[k] = odd;
        }
    }
    if (n > 0) {
        size_t logical_last = 0;
        for (size_t index = 0; index < n; ++index) if (units[index].end >= units[logical_last].end) logical_last = index;
        const bool odd = (units[logical_last].level & 1) != 0;
        result.caret[count] = odd ? left_edge[logical_last] : right_edge[logical_last];
        result.char_rtl[count] = odd;
    }
    return result;
}

const TextLayout& TextEngine::layout(const std::u32string& text, float size, Direction direction) {
    std::u32string key = text;
    key.push_back(static_cast<char32_t>(0x110000 + static_cast<int>(size * 4.0f)));
    key.push_back(static_cast<char32_t>(0x110000 + static_cast<int>(direction)));
    const auto found = layout_cache_.find(key);
    if (found != layout_cache_.end()) return found->second;
    if (layout_cache_.size() > 4000) layout_cache_.clear();
    return layout_cache_.emplace(std::move(key), build(text, size, direction)).first->second;
}

const GlyphBitmap& TextEngine::bitmap(int glyph, float size, int phase) {
    const unsigned long long key = (static_cast<unsigned long long>(glyph) << 32) | (static_cast<unsigned long long>(static_cast<int>(size * 4.0f)) << 8) | static_cast<unsigned long long>(phase);
    const auto found = glyph_cache_.find(key);
    if (found != glyph_cache_.end()) return found->second;
    if (glyph_cache_.size() > 6000) glyph_cache_.clear();
    GlyphBitmap image;
    font_.rasterize(glyph, size, static_cast<float>(phase) * 0.25f, image);
    return glyph_cache_.emplace(key, std::move(image)).first->second;
}

void TextEngine::draw(Canvas& canvas, const TextLayout& layout, float x, float baseline, float size, Color color, bool bold) {
    for (const auto& glyph : layout.glyphs) {
        for (int pass = 0; pass < (bold ? 2 : 1); ++pass) {
            const float pen = x + glyph.x + (pass ? std::max(0.6f, size / 22.0f) : 0.0f);
            const float whole = std::floor(pen);
            const int phase = std::clamp(static_cast<int>((pen - whole) * 4.0f), 0, 3);
            const GlyphBitmap& image = bitmap(glyph.glyph, size, phase);
            if (image.width <= 0) continue;
            canvas.blit_coverage(static_cast<int>(whole) + image.left, static_cast<int>(std::lround(baseline + glyph.y)) + image.top, image.width, image.height, image.alpha.data(), color);
        }
    }
}

size_t TextEngine::caret_index_at(const TextLayout& layout, float x) const {
    size_t best = 0;
    float best_distance = 1e9f;
    for (size_t i = 0; i < layout.caret.size(); ++i) {
        const float distance = std::fabs(layout.caret[i] - x);
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

float text_width(const std::wstring& text, float size, Direction direction) {
    return text_layout(text, size, direction).width;
}

float text_ascent(float size) { return TextEngine::instance().ascent(size); }
float text_height(float size) { return TextEngine::instance().line_height(size); }

const TextLayout& text_layout(const std::wstring& text, float size, Direction direction) {
    return TextEngine::instance().layout(core::to_u32(text), size, direction);
}

void draw_text(Canvas& canvas, const std::wstring& text, float x, float baseline, float size, Color color, Anchor anchor, bool bold, Direction direction) {
    if (text.empty()) return;
    const TextLayout& layout = text_layout(text, size, direction);
    float left = x;
    if (anchor == Anchor::Center) left = x - layout.width * 0.5f;
    else if (anchor == Anchor::Right) left = x - layout.width;
    TextEngine::instance().draw(canvas, layout, left, baseline, size, color, bold);
}

bool is_rtl_text(const std::wstring& text) {
    return text_layout(text, 12.0f, Direction::Auto).rtl;
}

}
