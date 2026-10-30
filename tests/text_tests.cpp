#include "risha/gfx/Text.hpp"
#include <iostream>

int main() {
    using namespace risha::gfx;
    if (!is_rtl_text(L"محرك ريشة")) return 1;
    if (is_rtl_text(L"Risha")) return 2;
    if (text_width(L"محرك ريشة", 20.0f) < 40.0f) return 3;
    const TextLayout layout = text_layout(L"سلام", 20.0f);
    if (layout.glyphs.empty() || layout.caret.size() != 5) return 4;
    if (text_width(L"abc", 20.0f) <= 0.0f) return 5;
    Canvas canvas(200, 40);
    canvas.clear(Color(0, 0, 0));
    draw_text(canvas, L"Risha ريشة", 10.0f, 28.0f, 20.0f, Color(255, 255, 255));
    bool lit = false;
    for (int x = 0; x < 200 && !lit; ++x) for (int y = 0; y < 40; ++y) if (canvas.pixel(x, y) != 0) { lit = true; break; }
    if (!lit) return 6;
    return 0;
}
