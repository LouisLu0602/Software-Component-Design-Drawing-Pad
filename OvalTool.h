#pragma once
#include <graphics.h>
#include <windows.h>   // COLORREF
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <climits>

// Globals owned by main.cpp
extern bool     fillEnabled;
extern int      gZCounter;
extern COLORREF currentFillColor;   // ALWAYS set via RGB(r,g,b)

// Oval uses: p1 = center, p2 = a point defining radii (rx = |dx|, ry = |dy|)
class OvalTool {
private:
    // In-progress points
    POINT* p1 = nullptr;  // center
    POINT* p2 = nullptr;  // defines radii relative to center

    struct StyledOval {
        POINT* center;   // committed center
        int      rx;       // radius x
        int      ry;       // radius y
        int      style;    // 0 = solid, 1 = dashed
        bool     fill;     // fill the ellipse?
        int      z;        // creation order
        COLORREF fillColor;
    };

    StyledOval* ovals = nullptr;
    int count = 0;
    int capacity = 0;

    void ensureCapacity(int need) {
        if (capacity >= need) return;
        int newCap = capacity ? capacity * 2 : 16;
        if (newCap < need) newCap = need;
        void* nb = std::realloc(ovals, size_t(newCap) * sizeof(StyledOval));
        if (!nb) return;
        ovals = (StyledOval*)nb;
        capacity = newCap;
    }

    static inline int absi(int v) { return v < 0 ? -v : v; }

    static inline void radiiFromPoints(const POINT& c, const POINT& q, int& rx, int& ry) {
        rx = absi(q.x - c.x);
        ry = absi(q.y - c.y);
    }

    // --- OUTLINE DRAWING ---

    // Outline drawer: solid uses EasyX ellipse(); dashed approximates with short chords.
    // NOTE: This function DOES NOT change the line color — caller controls it.
    static void drawOvalOutline(int cx, int cy, int rx, int ry, int style) {
        if (rx <= 0 || ry <= 0) return;

        if (style == 0) {
            ellipse(cx - rx, cy - ry, cx + rx, cy + ry);
            return;
        }

        // dashed: draw small chord segments along the parametric ellipse
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

            int x0 = cx + int(std::lround(rx * std::cos(a0)));
            int y0 = cy + int(std::lround(ry * std::sin(a0)));
            int x1 = cx + int(std::lround(rx * std::cos(a1)));
            int y1 = cy + int(std::lround(ry * std::sin(a1)));

            line(x0, y0, x1, y1);
        }
    }

    // --- SOLID FILL (scanline) ---

    // Fills the entire ellipse area using horizontal scanlines.
    // This ignores other outlines/shapes and paints every pixel inside.
    static void fillOvalSolid(int cx, int cy, int rx, int ry, COLORREF color) {
        if (rx <= 0 || ry <= 0) return;

        COLORREF oldFill = getfillcolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);     // overwrite pixels deterministically
        setfillcolor(color);

        // For each y, compute span width: xSpan = rx * sqrt(1 - ((y-cy)^2 / ry^2))
        for (int y = cy - ry; y <= cy + ry; ++y) {
            double ny = double(y - cy) / double(ry);
            double inside = 1.0 - ny * ny;
            if (inside < 0.0) continue; // numerical guard
            int halfw = int(std::floor(double(rx) * std::sqrt(inside) + 0.5));
            int x0 = cx - halfw;
            int x1 = cx + halfw;
            if (x1 >= x0) {
                // 1-pixel tall solid rectangle (faster than drawing tons of tiny lines)
                solidrectangle(x0, y, x1, y);
            }
        }

        setrop2(oldRop);
        setfillcolor(oldFill);
    }

    // For deletion hit-test: distance from mouse to nearest point on ellipse boundary
    // Approximate by using parameter t from angle computed in scaled space.
    static int distanceToEllipseEdge(POINT p, int cx, int cy, int rx, int ry) {
        if (rx <= 0 || ry <= 0) return INT_MAX;
        double dx = double(p.x - cx);
        double dy = double(p.y - cy);

        // Angle in scaled space:
        double t = std::atan2(dy / (ry ? ry : 1), dx / (rx ? rx : 1));

        double bx = cx + rx * std::cos(t);
        double by = cy + ry * std::sin(t);

        double ddx = double(p.x) - bx;
        double ddy = double(p.y) - by;
        return int(std::lround(std::sqrt(ddx * ddx + ddy * ddy)));
    }

public:
    ~OvalTool() {
        reset();
        for (int i = 0; i < count; ++i) {
            delete ovals[i].center;
        }
        std::free(ovals);
    }

    // --- Input handling ---

    void addPoint(POINT p) {
        if (!p1)       p1 = new POINT{ p.x, p.y };
        else if (!p2)  p2 = new POINT{ p.x, p.y };
    }

    bool isReady() const { return p1 && p2; }

    // Commit current oval; final draw in COPY mode; force BLACK edges; save/restore state.
    void drawAndReset(int* style) {
        if (!isReady()) return;

        int rx, ry;
        radiiFromPoints(*p1, *p2, rx, ry);

        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);  // final render should not XOR

        if (fillEnabled) {
            fillOvalSolid(p1->x, p1->y, rx, ry, currentFillColor);
        }

        setlinecolor(BLACK);  // deterministic outline color for final draw
        drawOvalOutline(p1->x, p1->y, rx, ry, (style ? *style : 0));

        // Restore state
        setrop2(oldRop);
        setlinecolor(oldLine);

        // Store committed oval
        ensureCapacity(count + 1);
        if (capacity >= count + 1) {
            ovals[count] = {
                p1,                 // take ownership
                rx,
                ry,
                (style ? *style : 0),
                fillEnabled,
                ++gZCounter,
                currentFillColor
            };
            ++count;

            delete p2; p2 = nullptr;  // not needed after commit
            p1 = nullptr;             // ownership moved
        }

        reset(); // paranoia
    }

    // --- Z / Drawing API ---

    size_t getCount() const { return size_t(count); }
    int    getZ(size_t i) const { return (i < getCount()) ? ovals[i].z : 0; }

    void drawAt(size_t i) const {
        if (i >= getCount()) return;

        const auto& o = ovals[i];

        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);

        if (o.fill) {
            fillOvalSolid(o.center->x, o.center->y, o.rx, o.ry, o.fillColor);
        }

        setlinecolor(BLACK);
        drawOvalOutline(o.center->x, o.center->y, o.rx, o.ry, o.style);

        setrop2(oldRop);
        setlinecolor(oldLine);
    }

    void drawCompleted() const {
        for (size_t i = 0; i < getCount(); ++i)
            drawAt(i);
    }

    // Preview with COPY mode (no XOR) and LIGHTGRAY outline
    void drawPreview(POINT mouse, int* style) const {
        if (!p1) return;

        int rx, ry;
        if (p2) {
            radiiFromPoints(*p1, *p2, rx, ry);
        }
        else {
            radiiFromPoints(*p1, mouse, rx, ry);
        }
        if (rx <= 0 || ry <= 0) return;

        int      s = (style ? *style : 0);
        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);     // full-frame redraw => stable copy render
        setlinecolor(LIGHTGRAY); // visible preview color

        drawOvalOutline(p1->x, p1->y, rx, ry, s);

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
            delete ovals[i].center;
        }
        std::free(ovals);
        ovals = nullptr;
        capacity = 0;
        count = 0;
    }

    // --- Deletion ---

    bool deleteOvalNear(POINT mouse, int threshold = 10) {
        for (int i = 0; i < count; ++i) {
            int cx = ovals[i].center->x;
            int cy = ovals[i].center->y;
            int rx = ovals[i].rx;
            int ry = ovals[i].ry;

            int d = distanceToEllipseEdge(mouse, cx, cy, rx, ry);
            if (d <= threshold) {
                delete ovals[i].center;

                if (i < count - 1) {
                    std::memmove(&ovals[i],
                        &ovals[i + 1],
                        size_t(count - i - 1) * sizeof(StyledOval));
                }
                --count;
                return true;
            }
        }
        return false;
    }
};
