#pragma once
#include <graphics.h>
#include <windows.h>    // COLORREF
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "LineUtils.h"

// Globals owned by main.cpp
extern bool     fillEnabled;
extern int      gZCounter;
extern COLORREF currentFillColor;   

class TriangleTool {
private:
    // pointers
    POINT* p1 = nullptr;
    POINT* p2 = nullptr;
    POINT* p3 = nullptr;

    // Stored triangle
    struct StyledTriangle {
        POINT* a;
        POINT* b;
        POINT* c;
        int      style;       // 0 = solid, 1 = dashed
        bool     fill;        // draw filled polygon or not
        int      z;           // creation order
        COLORREF fillColor;   // the fill color used at commit time
    };

    // Dynamic storage
    StyledTriangle* triangles = nullptr;
    int triangleCount = 0;
    int capacity = 0;

    void ensureCapacity(int need) {
        if (capacity >= need) return;
        int newCap = capacity ? capacity * 2 : 16;
        if (newCap < need) newCap = need;
        void* nb = std::realloc(triangles, size_t(newCap) * sizeof(StyledTriangle));
        if (!nb) return;
        triangles = (StyledTriangle*)nb;
        capacity = newCap;
    }

public:
    ~TriangleTool() {
        reset();
        for (int i = 0; i < triangleCount; ++i) {
            delete triangles[i].a;
            delete triangles[i].b;
            delete triangles[i].c;
        }
        std::free(triangles);
    }

    // --- Input handling ---

    void addPoint(POINT p) {
        if (!p1)       p1 = new POINT{ p.x, p.y };
        else if (!p2)  p2 = new POINT{ p.x, p.y };
        else if (!p3)  p3 = new POINT{ p.x, p.y };
    }

    bool isReady() const { return p1 && p2 && p3; }

    // Commit current triangle and clear in-progress points
    // Final draw is COPY mode; force BLACK edges; save/restore state.
    void drawAndReset(int* style, bool fill) {
        if (!isReady()) return;

        // Save global state
        COLORREF oldFill = getfillcolor();
        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);  // never XOR on final draw

        if (fill) {
            POINT pts[3] = { *p1, *p2, *p3 };
            setfillcolor(currentFillColor);
            solidpolygon(pts, 3);
        }

        // Force a deterministic edge color (BLACK), then draw edges
        setlinecolor(BLACK);
        drawCustomLine(*p1, *p2, style);
        drawCustomLine(*p2, *p3, style);
        drawCustomLine(*p3, *p1, style);

        // Restore state
        setrop2(oldRop);
        setfillcolor(oldFill);
        setlinecolor(oldLine);

        // Store the committed triangle
        ensureCapacity(triangleCount + 1);
        if (capacity >= triangleCount + 1) {
            triangles[triangleCount] = { p1, p2, p3, *style, fill, ++gZCounter, currentFillColor };
            ++triangleCount;
            p1 = p2 = p3 = nullptr; // ownership transferred
        }

        reset(); // paranoia
    }

    // --- Z / Drawing API ---

    size_t getCount() const { return size_t(triangleCount); }

    int getZ(size_t i) const { return (i < getCount()) ? triangles[i].z : 0; }

    // Draw a stored triangle (also preserves global colors and ROP)
    void drawAt(size_t i) const {
        if (i >= getCount()) return;

        const auto& t = triangles[i];

        COLORREF oldFill = getfillcolor();
        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN); // final render: copy, not XOR

        if (t.fill) {
            POINT pts[3] = { *t.a, *t.b, *t.c };
            setfillcolor(t.fillColor);
            solidpolygon(pts, 3);
        }

        // Force BLACK for edges so UI colors don't leak in
        setlinecolor(BLACK);
        int st = t.style;
        drawCustomLine(*t.a, *t.b, &st);
        drawCustomLine(*t.b, *t.c, &st);
        drawCustomLine(*t.c, *t.a, &st);

        setrop2(oldRop);
        setfillcolor(oldFill);
        setlinecolor(oldLine);
    }

    void drawCompleted() const {
        for (size_t i = 0; i < getCount(); ++i) drawAt(i);
    }

    // Preview current triangle (fixed LIGHTGRAY + XOR)
    void drawPreview(POINT mouse, int* style) const {
        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setlinecolor(LIGHTGRAY);
        setrop2(R2_XORPEN);

        if (p1 && p2 && !p3) {
            drawCustomLine(*p1, *p2, style);
            drawCustomLine(*p2, mouse, style);
            drawCustomLine(mouse, *p1, style);
        }
        else if (p1 && !p2) {
            drawCustomLine(*p1, mouse, style);
        }

        setrop2(oldRop);
        setlinecolor(oldLine);
    }

    void reset() {
        delete p1; delete p2; delete p3;
        p1 = p2 = p3 = nullptr;
    }

    void resetAll() {
        reset();
        for (int i = 0; i < triangleCount; ++i) {
            delete triangles[i].a;
            delete triangles[i].b;
            delete triangles[i].c;
        }
        std::free(triangles);
        triangles = nullptr;
        capacity = 0;
        triangleCount = 0;
    }

    // --- Right click Deletion ---

    bool deleteTriangleNear(POINT mouse, int threshold = 10) {
        for (int i = 0; i < triangleCount; ++i) {
            if (pointNearSegment(mouse, *triangles[i].a, *triangles[i].b, threshold) ||
                pointNearSegment(mouse, *triangles[i].b, *triangles[i].c, threshold) ||
                pointNearSegment(mouse, *triangles[i].c, *triangles[i].a, threshold)) {

                delete triangles[i].a;
                delete triangles[i].b;
                delete triangles[i].c;

                if (i < triangleCount - 1) {
                    std::memmove(&triangles[i],
                        &triangles[i + 1],
                        size_t(triangleCount - i - 1) * sizeof(StyledTriangle));
                }

                --triangleCount;
                return true;
            }
        }
        return false;
    }

private:
    // calculating distance from point p to segment ab
    bool pointNearSegment(POINT p, POINT a, POINT b, int th) const {
        double dx = b.x - a.x;
        double dy = b.y - a.y;

        if (dx == 0 && dy == 0) {
            double d0 = hypot(p.x - a.x, p.y - a.y);
            return d0 <= th;
        }

        double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy);
        if (t < 0)      t = 0;
        else if (t > 1) t = 1;

        double px = a.x + t * dx;
        double py = a.y + t * dy;
        double d = hypot(p.x - px, p.y - py);
        return d <= th;
    }
};
