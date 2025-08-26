#pragma once
#define NOMINMAX
#include <graphics.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <list>
#include "LineUtils.h"

// global variables
extern int gZCounter;

class LineTool {
private:
    POINT* start = nullptr;
    POINT* end = nullptr;

    struct StyledLine {
        POINT start;
        POINT end;
        int   style;
        int   z;     // creation order
    };

    std::list<StyledLine> completedLines;

    std::list<StyledLine>::const_iterator itAt(size_t i) const {
        auto it = completedLines.cbegin();
        while (i-- && it != completedLines.cend()) ++it;
        return it;
    }

public:
    ~LineTool() { reset(); }

    void addPoint(POINT p) {
        if (!start) {
            start = new POINT{ p.x, p.y };
        }
        else if (!end) {
            end = new POINT{ p.x, p.y };
        }
    }

    bool isReady() const { return start && end; }

    void drawAndReset(int* style) {
        if (!isReady()) return;

        drawCustomLine(*start, *end, style);
        completedLines.push_back({ *start, *end, *style, ++gZCounter });
        reset();
    }

    // ---- Z-ordered drawing API ----
    size_t getCount() const { return completedLines.size(); }

    int getZ(size_t i) const {
        auto it = itAt(i);
        return (it == completedLines.cend()) ? 0 : it->z;
    }

    void drawAt(size_t i) const {
        auto it = itAt(i);
        if (it == completedLines.cend()) return;
        int style = it->style;
        drawCustomLine(it->start, it->end, &style);
    }

    void drawCompleted() const {
        for (const auto& line : completedLines) {
            int style = line.style;
            drawCustomLine(line.start, line.end, &style);
        }
    }

    void drawPreview(POINT mouse, int* style) const {
        if (start && !end) {
            drawCustomLine(*start, mouse, style);
        }
    }

    void reset() {
        delete start;
        delete end;
        start = end = nullptr;
    }

    void resetAll() {
        reset();
        completedLines.clear();
    }

    bool deleteLineNear(POINT mouse, int threshold = 10) {
        for (auto it = completedLines.begin(); it != completedLines.end(); ++it) {
            double dist = pointToSegmentDistance(mouse, it->start, it->end);
            if (dist <= threshold) {
                completedLines.erase(it);
                return true;
            }
        }
        return false;
    }

private:
    double pointToSegmentDistance(POINT p, POINT a, POINT b) const {
        double dx = b.x - a.x;
        double dy = b.y - a.y;
        if (dx == 0 && dy == 0) return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));

        double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy);
        if (t < 0.0) t = 0.0;
        else if (t > 1.0) t = 1.0;

        double projX = a.x + t * dx;
        double projY = a.y + t * dy;
        return std::sqrt((p.x - projX) * (p.x - projX) + (p.y - projY) * (p.y - projY));
    }
};
