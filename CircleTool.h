#pragma once
#include <graphics.h>
#include <windows.h>    // COLORREF
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "LineUtils.h"  // style convention (0=solid, 1=dashed)

// Globals owned by main.cpp
extern bool     fillEnabled;
extern int      gZCounter;
extern COLORREF currentFillColor;   //set via RGB(r,g,b)

class CircleTool {
private:
    // In-progress points
    POINT* p1 = nullptr;   // center
    POINT* p2 = nullptr;   // point on radius

    struct StyledCircle {
        POINT* center;   // committed center
        int      radius;   // committed radius
        int      style;    // 0 = solid, 1 = dashed
        bool     fill;     // fill the disk?
        int      z;        // creation order
        COLORREF fillColor;
    };

    StyledCircle* circles = nullptr;
    int count = 0;
    int capacity = 0;

    void ensureCapacity(int need) {
        if (capacity >= need) return;

        int newCap = capacity ? capacity * 2 : 16;
        if (newCap < need) newCap = need;

        void* nb = std::realloc(circles, size_t(newCap) * sizeof(StyledCircle));
        if (!nb) return;

        circles = (StyledCircle*)nb;
        capacity = newCap;
    }

    static inline int distancei(POINT a, POINT b) {
        int dx = b.x - a.x, dy = b.y - a.y;
        return (int)std::lround(std::sqrt(double(dx) * dx + double(dy) * dy));
    }
    static void drawCircleOutline(int cx, int cy, int r, int style) {
        if (r <= 0) return;

        if (style == 0) {
            circle(cx, cy, r);
            return;
        }

        // dashed: draw small chord segments, skip every other chunk
        const double TWO_PI = 6.283185307179586;
        const int    SEG_DEG = 6;  // chord step (degrees)
        const int    DASH_RUN = 3;  // draw 3 segments, skip 3
        const int    GAP_RUN = 3;

        int segCount = int(std::ceil(360.0 / SEG_DEG));
        int runLen = DASH_RUN + GAP_RUN;

        for (int s = 0; s < segCount; ++s) {
            int posInRun = s % runLen;
            bool drawThis = posInRun < DASH_RUN;
            if (!drawThis) continue;

            double a0 = (s * SEG_DEG) * (TWO_PI / 360.0);
            double a1 = ((s + 1) * SEG_DEG) * (TWO_PI / 360.0);

            int x0 = cx + int(std::lround(r * std::cos(a0)));
            int y0 = cy + int(std::lround(r * std::sin(a0)));
            int x1 = cx + int(std::lround(r * std::cos(a1)));
            int y1 = cy + int(std::lround(r * std::sin(a1)));

            line(x0, y0, x1, y1);
        }
    }

public:
    ~CircleTool() {
        reset();
        for (int i = 0; i < count; ++i) {
            delete circles[i].center;
        }
        std::free(circles);
    }

    void addPoint(POINT p) {
        if (!p1)       p1 = new POINT{ p.x, p.y };
        else if (!p2)  p2 = new POINT{ p.x, p.y };
    }

    bool isReady() const { return p1 && p2; }

    // Commit current circle; final draw in COPY mode; force BLACK edges; save/restore state.
    void drawAndReset(int* style) {
        if (!isReady()) return;

        int r = distancei(*p1, *p2);

        COLORREF oldFill = getfillcolor();
        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);  // never XOR on final render

        if (fillEnabled) {
            setfillcolor(currentFillColor);
            solidcircle(p1->x, p1->y, r);
        }

        setlinecolor(BLACK);  // deterministic outline color for final draw
        drawCircleOutline(p1->x, p1->y, r, *style);

        // Restore state
        setrop2(oldRop);
        setfillcolor(oldFill);
        setlinecolor(oldLine);

        // Store committed circle
        ensureCapacity(count + 1);
        if (capacity >= count + 1) {
            circles[count] = {
                p1,                 
                r,
                *style,
                fillEnabled,
                ++gZCounter,
                currentFillColor
            };
            ++count;

            delete p2; p2 = nullptr;  // freeing memories after commiting
            p1 = nullptr;             // ownership moved
        }

        reset(); 
    }

    // --- Z / Drawing API ---

    size_t getCount() const { return size_t(count); }
    int    getZ(size_t i) const { return (i < getCount()) ? circles[i].z : 0; }

    void drawAt(size_t i) const {
        if (i >= getCount()) return;

        const auto& c = circles[i];

        COLORREF oldFill = getfillcolor();
        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);

        if (c.fill) {
            setfillcolor(c.fillColor);
            solidcircle(c.center->x, c.center->y, c.radius);
        }

        setlinecolor(BLACK);
        drawCircleOutline(c.center->x, c.center->y, c.radius, c.style);

        setrop2(oldRop);
        setfillcolor(oldFill);
        setlinecolor(oldLine);
    }

    void drawCompleted() const {
        for (size_t i = 0; i < getCount(); ++i)
            drawAt(i);
    }

    // Preview with COPY mode (no XOR) and LIGHTGRAY outline
    void drawPreview(POINT mouse, int* style) const {
        if (!p1) return;

        int r = p2 ? distancei(*p1, *p2) : distancei(*p1, mouse);

        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);     
        setlinecolor(LIGHTGRAY); 

        drawCircleOutline(p1->x, p1->y, r, *style);

        setrop2(oldRop);
        setlinecolor(oldLine);
    }

    // --- Memory management ---

    void reset() {
        delete p1; delete p2;
        p1 = p2 = nullptr;
    }

    void resetAll() {
        reset();
        for (int i = 0; i < count; ++i) {
            delete circles[i].center;
        }
        std::free(circles);
        circles = nullptr;
        capacity = 0;
        count = 0;
    }

    // --- Deletion ---

    bool deleteCircleNear(POINT mouse, int threshold = 10) {
        for (int i = 0; i < count; ++i) {
            int cx = circles[i].center->x;
            int cy = circles[i].center->y;
            int r = circles[i].radius;

            int dx = mouse.x - cx;
            int dy = mouse.y - cy;
            int d = int(std::lround(std::sqrt(double(dx) * dx + double(dy) * dy)));

            if (std::abs(d - r) <= threshold) {
                delete circles[i].center;

                if (i < count - 1) {
                    std::memmove(&circles[i],
                        &circles[i + 1],
                        size_t(count - i - 1) * sizeof(StyledCircle));
                }
                --count;
                return true;
            }
        }
        return false;
    }
};
