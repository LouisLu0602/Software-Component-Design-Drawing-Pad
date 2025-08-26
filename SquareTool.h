#pragma once
#include <graphics.h>
#include <windows.h>    // COLORREF
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "LineUtils.h"

// Globals variables
extern bool     fillEnabled;
extern int      gZCounter;
extern COLORREF currentFillColor;

// p1 = first corner, p2 = opposite corner.

class SquareTool {
private:
    // In-progress corners
    POINT* p1 = nullptr;
    POINT* p2 = nullptr;

    struct StyledSquare {
        POINT* a;           // corner 1 
        POINT* b;           // corner 2 (opposite)
        int      style;       // 0 = solid, 1 = dashed
        bool     fill;        // draw filled rectangle or not
        int      z;           // creation order
        COLORREF fillColor;   // fill color used at commit time
    };

    StyledSquare* squares = nullptr;
    int count = 0;
    int capacity = 0;

    void ensureCapacity(int need) {
        if (capacity >= need) return;
        int newCap = capacity ? capacity * 2 : 16;
        if (newCap < need) newCap = need;
        void* nb = std::realloc(squares, size_t(newCap) * sizeof(StyledSquare));
        if (!nb) return;
        squares = (StyledSquare*)nb;
        capacity = newCap;
    }

    // setting up boundries
    static inline void rectBounds(const POINT& a, const POINT& b, int& L, int& T, int& R, int& B) {
        L = (a.x < b.x) ? a.x : b.x;
        R = (a.x > b.x) ? a.x : b.x;
        T = (a.y < b.y) ? a.y : b.y;
        B = (a.y > b.y) ? a.y : b.y;
    }

public:
    ~SquareTool() {
        reset();
        for (int i = 0; i < count; ++i) {
            delete squares[i].a;
            delete squares[i].b;
        }
        std::free(squares);
    }

    void addPoint(POINT p) {
        if (!p1)       p1 = new POINT{ p.x, p.y };
        else if (!p2)  p2 = new POINT{ p.x, p.y };
    }

    bool isReady() const { return p1 && p2; }

    // Commit current rect; final draw in copy mode; force black edges; save/restore state.
    void drawAndReset(int* style, bool fill) {
        if (!isReady()) return;

        COLORREF oldFill = getfillcolor();
        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN);  // never XOR on final draw

        int L, T, R, B;
        rectBounds(*p1, *p2, L, T, R, B);

        if (fill) {
            setfillcolor(currentFillColor);
            solidrectangle(L, T, R, B);
        }

        // Outline in deterministic color
        setlinecolor(BLACK);
        // Use your dashed/solid logic via drawCustomLine on the 4 edges
        drawCustomLine({ L, T }, { R, T }, style); // top
        drawCustomLine({ R, T }, { R, B }, style); // right
        drawCustomLine({ R, B }, { L, B }, style); // bottom
        drawCustomLine({ L, B }, { L, T }, style); // left

        // Restore
        setrop2(oldRop);
        setfillcolor(oldFill);
        setlinecolor(oldLine);

        // Store the committed rect
        ensureCapacity(count + 1);
        if (capacity >= count + 1) {
            squares[count] = {
                p1, p2,
                *style,
                fill,
                ++gZCounter,
                currentFillColor
            };
            ++count;
            p1 = p2 = nullptr; // ownership transferred
        }

        reset(); // paranoia
    }

    // --- Z / Drawing API ---

    size_t getCount() const { return size_t(count); }

    int getZ(size_t i) const { return (i < getCount()) ? squares[i].z : 0; }

    void drawAt(size_t i) const {
        if (i >= getCount()) return;

        const auto& s = squares[i];

        COLORREF oldFill = getfillcolor();
        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setrop2(R2_COPYPEN); 

        int L, T, R, B;
        rectBounds(*s.a, *s.b, L, T, R, B);

        if (s.fill) {
            setfillcolor(s.fillColor);
            solidrectangle(L, T, R, B);
        }

        
        setlinecolor(BLACK);
        int st = s.style;
        drawCustomLine({ L, T }, { R, T }, &st);
        drawCustomLine({ R, T }, { R, B }, &st);
        drawCustomLine({ R, B }, { L, B }, &st);
        drawCustomLine({ L, B }, { L, T }, &st);

        setrop2(oldRop);
        setfillcolor(oldFill);
        setlinecolor(oldLine);
    }

    void drawCompleted() const {
        for (size_t i = 0; i < getCount(); ++i) drawAt(i);
    }

    
    void drawPreview(POINT mouse, int* style) const {
        if (!p1) return;

        COLORREF oldLine = getlinecolor();
        int      oldRop = getrop2();

        setlinecolor(LIGHTGRAY);
        setrop2(R2_XORPEN);

        if (p1 && !p2) {
            // live rectangle from p1 to mouse
            int L, T, R, B;
            rectBounds(*p1, mouse, L, T, R, B);
            drawCustomLine({ L, T }, { R, T }, style);
            drawCustomLine({ R, T }, { R, B }, style);
            drawCustomLine({ R, B }, { L, B }, style);
            drawCustomLine({ L, B }, { L, T }, style);
        }

        setrop2(oldRop);
        setlinecolor(oldLine);
    }

    

    void reset() {
        delete p1; delete p2;
        p1 = p2 = nullptr;
    }

    void resetAll() {
        reset();
        for (int i = 0; i < count; ++i) {
            delete squares[i].a;
            delete squares[i].b;
        }
        std::free(squares);
        squares = nullptr;
        capacity = 0;
        count = 0;
    }

    // --- Deletion ---

    bool deleteSquareNear(POINT mouse, int threshold = 10) {
        for (int i = 0; i < count; ++i) {
            int L, T, R, B;
            rectBounds(*squares[i].a, *squares[i].b, L, T, R, B);

            if (pointNearSegment(mouse, { L, T }, { R, T }, threshold) ||
                pointNearSegment(mouse, { R, T }, { R, B }, threshold) ||
                pointNearSegment(mouse, { R, B }, { L, B }, threshold) ||
                pointNearSegment(mouse, { L, B }, { L, T }, threshold)) {

                delete squares[i].a;
                delete squares[i].b;

                if (i < count - 1) {
                    std::memmove(&squares[i],
                        &squares[i + 1],
                        size_t(count - i - 1) * sizeof(StyledSquare));
                }

                --count;
                return true;
            }
        }
        return false;
    }

private:
    // Distance from point p to segment ab <= th ?
    static bool pointNearSegment(POINT p, POINT a, POINT b, int th) {
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
