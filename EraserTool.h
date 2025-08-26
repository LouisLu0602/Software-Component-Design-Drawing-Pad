#pragma once
#include <graphics.h>
#include <cstdlib>   // malloc, realloc, free

// Global z-order counter from main.cpp
extern int gZCounter;

// EraserTool: stores white circular "dabs" as a dynamic array allocated via malloc/realloc.
class EraserTool {
private:
    struct Dab {
        POINT p;
        int   radius;
        int   z;
    };

    // Dynamic storage (C-style)
    Dab* dabs = nullptr;     // allocated block
    size_t dabCount = 0;       // used length
    size_t dabCap = 0;       // capacity (# of Dab slots)

    // Current stroke state
    bool  inStroke = false;
    int   currentRadius = 16;  // default

    // --- tiny helpers (no std::max/min/round/abs) ---
    static inline int iabs(int v) { return (v < 0) ? -v : v; }
    static inline int iRound(float v) { return (int)(v + (v >= 0.0f ? 0.5f : -0.5f)); }

    // Ensure we have room for at least one more dab
    bool ensureCapacity() {
        if (dabCount < dabCap) return true;
        size_t newCap = (dabCap == 0) ? (size_t)4096 : (dabCap * 2);
        // guard against overflow (very defensive; unlikely to hit)
        if (newCap < dabCap || newCap >(size_t)100000000) return false;

        void* p = (dabCap == 0)
            ? std::malloc(newCap * sizeof(Dab))
            : std::realloc(dabs, newCap * sizeof(Dab));

        if (!p) return false;
        dabs = (Dab*)p;
        dabCap = newCap;
        return true;
    }

    // Push a single dab (drop silently if allocation fails)
    inline void pushDab(POINT p, int r, int z) {
        if (!ensureCapacity()) return;   // if we can't grow, skip this dab
        dabs[dabCount].p = p;
        dabs[dabCount].radius = r;
        dabs[dabCount].z = z;
        dabCount++;
    }

public:
    // ----- lifetime -----
    ~EraserTool() {
        if (dabs) {
            std::free(dabs);
            dabs = nullptr;
        }
        dabCount = 0;
        dabCap = 0;
        inStroke = false;
    }

    // ------------- Public API used by main.cpp -------------
    void setRadius(int r) {
        if (r < 1) r = 1;
        currentRadius = r;
    }

    void beginStroke(int radius) {
        if (radius > 0) currentRadius = radius;
        inStroke = true;
        // z is assigned per-dab
    }

    void addDab(POINT p) {
        if (!inStroke) return;
        int z = ++gZCounter;  // newer dab above older ones
        pushDab(p, currentRadius, z);
    }

    // Add interpolated dabs between a -> b (simple DDA)
    void addInterpolatedDabs(POINT a, POINT b) {
        if (!inStroke) return;

        int dx = b.x - a.x;
        int dy = b.y - a.y;
        int adx = iabs(dx);
        int ady = iabs(dy);
        int steps = (adx > ady) ? adx : ady;  // manual max

        float x = (float)a.x;
        float y = (float)a.y;
        float ix = (steps != 0) ? ((float)dx / (float)steps) : 0.0f;
        float iy = (steps != 0) ? ((float)dy / (float)steps) : 0.0f;

        // start from first increment (caller usually already added a dab at 'a')
        for (int i = 1; i <= steps; ++i) {
            x += ix; y += iy;
            POINT p{ iRound(x), iRound(y) };
            int z = ++gZCounter;
            pushDab(p, currentRadius, z);
        }
    }

    void endStroke() { inStroke = false; }

    // --------- Methods used by rebuild() ordering ----------
    size_t getCount() const { return dabCount; }

    int getZ(size_t i) const {
        return (i < dabCount) ? dabs[i].z : 0;
    }

    void drawAt(size_t i) const {
        if (i >= dabCount) return;
        setfillcolor(WHITE);
        solidcircle(dabs[i].p.x, dabs[i].p.y, dabs[i].radius);
    }

    void resetAll() {
        // Free all memory so we actually release RAM
        if (dabs) {
            std::free(dabs);
            dabs = nullptr;
        }
        dabCount = 0;
        dabCap = 0;
        inStroke = false;
    }
};
