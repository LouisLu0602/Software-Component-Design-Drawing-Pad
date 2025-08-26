#pragma once
#include <graphics.h>
#include <cstdlib>      // malloc, realloc, free
#include "LineUtils.h"

// from main.cpp
extern int gZCounter;

class FreehandTool {
private:
    struct Stroke {
        POINT start;
        POINT end;
        int   style;
        int   z;
    };

    Stroke* strokes = nullptr;   // dynamic array of strokes
    int     strokeCount = 0;     // number used
    int     capacity = 0;     // number allocated

public:
    ~FreehandTool() { clearMemory(); }

    // Add a small segment of a freehand path
    void addStroke(const POINT& from, const POINT& to, int* style) {
        ensureCapacity(strokeCount + 1);

        strokes[strokeCount].start = from;
        strokes[strokeCount].end = to;
        strokes[strokeCount].style = *style;
        strokes[strokeCount].z = ++gZCounter;   // newest stroke on top

        // Draw immediately (interactive feel)
        drawCustomLine(from, to, style);

        ++strokeCount;
    }

    // --- Z-order API ---
    size_t getCount() const { return static_cast<size_t>(strokeCount); }

    int getZ(size_t i) const {
        return (i < static_cast<size_t>(strokeCount)) ? strokes[i].z : 0;
    }

    void drawAt(size_t i) const {
        if (i >= static_cast<size_t>(strokeCount)) return;
        int style = strokes[i].style;
        drawCustomLine(strokes[i].start, strokes[i].end, &style);
    }

    void drawCompleted() const {
        for (size_t i = 0; i < static_cast<size_t>(strokeCount); ++i)
            drawAt(i);
    }

    void reset() {
        strokeCount = 0; // reseting capasity
    }

    void resetAll() {
        clearMemory();
        strokes = nullptr;
        strokeCount = 0;
        capacity = 0;
    }

private:
    void ensureCapacity(int minNeeded) {
        if (minNeeded <= capacity) return;

        int newCap = (capacity == 0) ? 16 : capacity * 2;
        if (newCap < minNeeded) newCap = minNeeded;

        void* newBuf = std::realloc(strokes, static_cast<size_t>(newCap) * sizeof(Stroke));
        if (!newBuf) {
            return;
        }
        strokes = static_cast<Stroke*>(newBuf);
        capacity = newCap;
    }

    void clearMemory() {
        std::free(strokes);
        strokes = nullptr;
        capacity = 0;
    }
};
