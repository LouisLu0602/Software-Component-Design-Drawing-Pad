#pragma once
#include <graphics.h>
#include <cmath>

// 0 = solid, 1 = dashed
inline void drawCustomLine(POINT a, POINT b, int* style) {
    setlinestyle(*style == 0 ? PS_SOLID : PS_DASH, 1);
    line(a.x, a.y, b.x, b.y);
    setlinestyle(PS_SOLID, 1); // Reset to solid for future shapes
}
