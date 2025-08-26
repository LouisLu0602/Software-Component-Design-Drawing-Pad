#include <graphics.h>
#include <conio.h>
#include <windows.h>
#include <commdlg.h>    // file dialogs
#include <cmath>
#include <vector>
#include <algorithm>
#include "LineTool.h"
#include "TriangleTool.h"
#include "SquareTool.h"
#include "CircleTool.h"
#include "OvalTool.h"
#include "FreehandTool.h"
#include "EraserTool.h"

// defining min and max
static inline int iabs(int v) { return (v < 0) ? -v : v; }
static inline int iRound(float v) { return (int)(v + (v >= 0.0f ? 0.5f : -0.5f)); }

void rebuildCanvas();

enum Tool { TOOL_FREEHAND, TOOL_LINE, TOOL_TRIANGLE, TOOL_SQUARE, TOOL_CIRCLE, TOOL_OVAL, TOOL_ERASER };
Tool currentTool = TOOL_FREEHAND;

FreehandTool freehandTool;
LineTool     lineTool;
TriangleTool triangleTool;
SquareTool   squareTool;
CircleTool   circleTool;
OvalTool     ovalTool;
EraserTool   eraserTool;

int  solidMode = 0;
int  dashedMode = 1;
int* currentLineMode = &solidMode;

// Defind global variables
bool     fillEnabled = true;                        // toggle
int      gZCounter = 0;                             // z-order counter (tools extern this)
COLORREF currentFillColor = RGB(200, 220, 255);     // palette-selected (tools may extern this)


IMAGE gCanvas;                  
bool  gNeedsRebuild = true;

// Background layer loaded from disk
IMAGE gBackground;
bool  gHasBackground = false;

static inline bool ImageReady(const IMAGE* img) {
    return img && img->getwidth() > 0 && img->getheight() > 0;
}

// toobar creation
static const int TB_Y1 = 5;
static const int TB_Y2 = 35;
static const int TB_BTN_W = 90;
static const int TB_BTN_GAP = 10;
static inline int TB_BTN_X(int i) { return 10 + i * (TB_BTN_W + TB_BTN_GAP); }
static inline bool inRect(int x, int y, int L, int T, int R, int B) {
    return (x >= L && x <= R && y >= T && y <= B);
}

static const RECT BTN_SOLID_DASH = { 10, 45, 110, 75 };
static const RECT BTN_FILL_TOG = { 120, 45, 220, 75 };
static const RECT BTN_SAVE = { 230, 45, 330, 75 };
static const RECT BTN_LOAD = { 340, 45, 440, 75 };

// color creation
static const COLORREF kPalette[] = {
    RGB(200,220,255), RGB(200,255,200), RGB(255,200,200)
};
static const int kPaletteCount = sizeof(kPalette) / sizeof(kPalette[0]);
static int  selectedPaletteIndex = 0;

static const int PALETTE_X = 10;
static const int PALETTE_Y0 = 90;
static const int SWATCH_W = 24;
static const int SWATCH_H = 24;
static const int SWATCH_GAP = 6;

void drawPalette() {
    for (int i = 0; i < kPaletteCount; ++i) {
        int y = PALETTE_Y0 + i * (SWATCH_H + SWATCH_GAP);
        setfillcolor(kPalette[i]);
        solidrectangle(PALETTE_X, y, PALETTE_X + SWATCH_W, y + SWATCH_H);
        setlinecolor(BLACK);
        rectangle(PALETTE_X, y, PALETTE_X + SWATCH_W, y + SWATCH_H);
        if (i == selectedPaletteIndex) {
            setlinecolor(RGB(0, 120, 215));
            rectangle(PALETTE_X - 2, y - 2, PALETTE_X + SWATCH_W + 2, y + SWATCH_H + 2);
        }
    }
}


static bool ShowSaveDialog(TCHAR* outPath, DWORD outPathCount) {
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetHWnd();
    ofn.lpstrFilter =
        _T("PNG Images (*.png)\0*.png\0")
        _T("Bitmap Images (*.bmp)\0*.bmp\0")
        _T("JPEG Images (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0")
        _T("All Files (*.*)\0*.*\0");
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = (DWORD)outPathCount;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = _T("png");
    return GetSaveFileName(&ofn) != FALSE;
}

static bool ShowOpenDialog(TCHAR* outPath, DWORD outPathCount) {
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetHWnd();
    ofn.lpstrFilter =
        _T("Image Files (*.png;*.bmp;*.jpg;*.jpeg)\0*.png;*.bmp;*.jpg;*.jpeg\0")
        _T("All Files (*.*)\0*.*\0");
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = (DWORD)outPathCount;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileName(&ofn) != FALSE;
}


static void SaveCanvasToFile() {
    TCHAR path[MAX_PATH] = _T("");
    if (!ShowSaveDialog(path, MAX_PATH)) return;

    if (!ImageReady(&gCanvas)) {
        getimage(&gCanvas, 0, 0, 800, 600);
        SetWorkingImage(&gCanvas);
        setbkcolor(WHITE);
        cleardevice();
        SetWorkingImage();
    }
    if (gNeedsRebuild) rebuildCanvas();

    saveimage(path, &gCanvas);  // saves background + shapes
}

static void LoadCanvasFromFile() {
    TCHAR path[MAX_PATH] = _T("");
    if (!ShowOpenDialog(path, MAX_PATH)) return;

    loadimage(&gBackground, path);
    gHasBackground = ImageReady(&gBackground);

    // Reset model so the scene equals background
    freehandTool.resetAll();
    lineTool.resetAll();
    triangleTool.resetAll();
    squareTool.resetAll();
    circleTool.resetAll();
    ovalTool.resetAll();
    eraserTool.resetAll();


    gNeedsRebuild = true; 
}

// -------------- Toolbar drawing --------------
void drawToolbar() {  
    if (GetImageBuffer() != nullptr) {
        setfillcolor(RGB(230, 230, 230));
    }
    solidrectangle(0, 0, 800, 80);

    const TCHAR* labels[] = {
        _T("Freehand"), _T("Line"), _T("Triangle"),
        _T("Square"), _T("Circle"), _T("Oval"), _T("Eraser")
    };

    for (int i = 0; i < 7; ++i) {
        int x = TB_BTN_X(i);
        setfillcolor(currentTool == i ? RGB(180, 220, 255) : RGB(255, 255, 255));
        solidrectangle(x, TB_Y1, x + TB_BTN_W, TB_Y2);
        outtextxy(x + 10, TB_Y1 + 7, labels[i]);
    }

    // Clear (top-right)
    int clearL = 800 - 90;
    int clearR = 799;
    setfillcolor(RGB(255, 150, 150));
    solidrectangle(clearL, TB_Y1, clearR, TB_Y2);
    outtextxy(clearL + 20, TB_Y1 + 7, _T("Clear"));

    // Solid/Dashed toggle
    setfillcolor((*currentLineMode == 0) ? RGB(200, 255, 200) : RGB(255, 255, 255));
    solidrectangle(BTN_SOLID_DASH.left, BTN_SOLID_DASH.top, BTN_SOLID_DASH.right, BTN_SOLID_DASH.bottom);
    outtextxy(BTN_SOLID_DASH.left + 20, BTN_SOLID_DASH.top + 7, (*currentLineMode == 0) ? _T("Solid") : _T("Dashed"));

    // Fill toggle
    setfillcolor(fillEnabled ? RGB(255, 255, 150) : RGB(255, 255, 255));
    solidrectangle(BTN_FILL_TOG.left, BTN_FILL_TOG.top, BTN_FILL_TOG.right, BTN_FILL_TOG.bottom);
    outtextxy(BTN_FILL_TOG.left + 20, BTN_FILL_TOG.top + 7, fillEnabled ? _T("Filled") : _T("Unfilled"));

    // Save button
    setfillcolor(RGB(200, 220, 255));
    solidrectangle(BTN_SAVE.left, BTN_SAVE.top, BTN_SAVE.right, BTN_SAVE.bottom);
    outtextxy(BTN_SAVE.left + 28, BTN_SAVE.top + 7, _T("Save"));

    // Load button
    setfillcolor(RGB(200, 220, 255));
    solidrectangle(BTN_LOAD.left, BTN_LOAD.top, BTN_LOAD.right, BTN_LOAD.bottom);
    outtextxy(BTN_LOAD.left + 28, BTN_LOAD.top + 7, _T("Load"));

    // Palette
    drawPalette();
}

static inline void drawToolbarAndResetState() {
    drawToolbar();
    setlinecolor(BLACK);
    setrop2(R2_COPYPEN);
}

// Palette clicking
bool handlePaletteClick(int mx, int my) {
    if (mx < PALETTE_X || mx > PALETTE_X + SWATCH_W) return false;
    if (my < PALETTE_Y0) return false;
    int dy = my - PALETTE_Y0;
    int cell = SWATCH_H + SWATCH_GAP;
    int idx = dy / cell;
    int localY = dy - idx * cell;
    if (idx >= 0 && idx < kPaletteCount && localY >= 0 && localY <= SWATCH_H) {
        selectedPaletteIndex = idx;
        currentFillColor = kPalette[idx];
        return true;
    }
    return false;
}

// -------------------- Render model rebuild -------------------
struct RenderRef { int z; Tool tool; size_t index; };
static inline bool byZ(const RenderRef& a, const RenderRef& b) { return a.z < b.z; }

void rebuildCanvas() {
    // Ensure canvas exists
    if (!ImageReady(&gCanvas)) {
        getimage(&gCanvas, 0, 0, 800, 600);
        SetWorkingImage(&gCanvas);
        setbkcolor(WHITE);
        cleardevice();
        SetWorkingImage();
    }

    SetWorkingImage(&gCanvas);
    // Start clean
    setbkcolor(WHITE);
    cleardevice();

    // 1) Background layer (if any)
    if (gHasBackground && ImageReady(&gBackground)) {
        putimage(0, 0, &gBackground);
    }

    // 2) Vector model on top
    setrop2(R2_COPYPEN);
    setlinecolor(BLACK);

    std::vector<RenderRef> refs;
    refs.reserve(
        freehandTool.getCount() + lineTool.getCount() +
        triangleTool.getCount() + squareTool.getCount() +
        circleTool.getCount() + ovalTool.getCount() +
        eraserTool.getCount()
    );

    for (size_t i = 0; i < freehandTool.getCount(); ++i) refs.push_back({ (int)freehandTool.getZ(i), TOOL_FREEHAND, i });
    for (size_t i = 0; i < lineTool.getCount(); ++i)      refs.push_back({ (int)lineTool.getZ(i), TOOL_LINE, i });
    for (size_t i = 0; i < triangleTool.getCount(); ++i)  refs.push_back({ (int)triangleTool.getZ(i), TOOL_TRIANGLE, i });
    for (size_t i = 0; i < squareTool.getCount(); ++i)    refs.push_back({ (int)squareTool.getZ(i), TOOL_SQUARE, i });
    for (size_t i = 0; i < circleTool.getCount(); ++i)    refs.push_back({ (int)circleTool.getZ(i), TOOL_CIRCLE, i });
    for (size_t i = 0; i < ovalTool.getCount(); ++i)      refs.push_back({ (int)ovalTool.getZ(i), TOOL_OVAL, i });
    for (size_t i = 0; i < eraserTool.getCount(); ++i)    refs.push_back({ (int)eraserTool.getZ(i), TOOL_ERASER, i });

    std::sort(refs.begin(), refs.end(), byZ);

    for (const auto& r : refs) {
        switch (r.tool) {
        case TOOL_FREEHAND: freehandTool.drawAt(r.index); break;
        case TOOL_LINE:     lineTool.drawAt(r.index);     break;
        case TOOL_TRIANGLE: triangleTool.drawAt(r.index); break;
        case TOOL_SQUARE:   squareTool.drawAt(r.index);   break;
        case TOOL_CIRCLE:   circleTool.drawAt(r.index);   break;
        case TOOL_OVAL:     ovalTool.drawAt(r.index);     break;
        case TOOL_ERASER:   eraserTool.drawAt(r.index);   break;
        default: break;
        }
    }

    SetWorkingImage();
    gNeedsRebuild = false;
}

static bool deleteAnythingAt(POINT mouse) {
    bool deleted = false;
    deleted = deleted || lineTool.deleteLineNear(mouse);
    deleted = deleted || triangleTool.deleteTriangleNear(mouse);
    deleted = deleted || squareTool.deleteSquareNear(mouse);
    deleted = deleted || circleTool.deleteCircleNear(mouse);
    deleted = deleted || ovalTool.deleteOvalNear(mouse);
    if (deleted) gNeedsRebuild = true;
    return deleted;
}

int main() {
    initgraph(800, 600);
    setbkcolor(WHITE);
    cleardevice();

    // Ensure gCanvas owns a valid bitmap
    getimage(&gCanvas, 0, 0, 800, 600);
    SetWorkingImage(&gCanvas);
    setbkcolor(WHITE);
    cleardevice();
    SetWorkingImage();

    setlinecolor(BLACK);
    settextstyle(16, 0, _T("Consolas"));
    settextcolor(BLACK);

    drawToolbarAndResetState();

    POINT lastPoint = { -1, -1 };
    bool mouseReleased = true;
    bool eraserDown = false;
    int  eraserRadius = 16;

    bool plusHeld = false, minusHeld = false;

    // Batch once; flush per frame
    BeginBatchDraw();

    while (true) {
        // Delete on right-click
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
            POINT mouse;
            GetCursorPos(&mouse);
            ScreenToClient(GetHWnd(), &mouse);
            while (deleteAnythingAt(mouse)) {}
            Sleep(150);
        }

        // Resizing with +/-
        SHORT sPlus = GetAsyncKeyState(VK_OEM_PLUS);
        SHORT sAdd = GetAsyncKeyState(VK_ADD);
        SHORT sMinus = GetAsyncKeyState(VK_OEM_MINUS);
        SHORT sSub = GetAsyncKeyState(VK_SUBTRACT);

        bool plusNow = (sPlus & 0x8000) || (sAdd & 0x8000);
        bool minusNow = (sMinus & 0x8000) || (sSub & 0x8000);

        if (plusNow && !plusHeld) {
            eraserRadius += 2;
            if (eraserRadius > 100) eraserRadius = 100;
            eraserTool.setRadius(eraserRadius);
        }
        if (minusNow && !minusHeld) {
            eraserRadius -= 2;
            if (eraserRadius < 1) eraserRadius = 1;
            eraserTool.setRadius(eraserRadius);
        }
        plusHeld = plusNow;
        minusHeld = minusNow;

        // Left click handling
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(GetHWnd(), &p);

            if (p.y <= 80) {
                bool hit = false;
                for (int i = 0; i < 7; ++i) {
                    int L = TB_BTN_X(i), R = L + TB_BTN_W;
                    if (inRect(p.x, p.y, L, TB_Y1, R, TB_Y2)) {
                        currentTool = static_cast<Tool>(i);
                        hit = true;

                        // Popup if eraser selected
                        if (currentTool == TOOL_ERASER) {
                            MessageBox(GetHWnd(), _T("Eraser: click +/- to resize"), _T("Tool Selected"), MB_OK | MB_ICONINFORMATION);
                        }
                        break;
                    }
                }

                if (!hit) {
                    int clearL = 800 - 90, clearR = 799;
                    // Clear
                    if (inRect(p.x, p.y, clearL, TB_Y1, clearR, TB_Y2)) {
                        freehandTool.resetAll();
                        lineTool.resetAll();
                        triangleTool.resetAll();
                        squareTool.resetAll();
                        circleTool.resetAll();
                        ovalTool.resetAll();
                        eraserTool.resetAll();

                        gHasBackground = false; // also clear background layer

                        SetWorkingImage(&gCanvas);
                        setbkcolor(WHITE);
                        cleardevice();
                        SetWorkingImage();

                        gNeedsRebuild = false;
                        lastPoint = POINT{ -1, -1 };
                        mouseReleased = false;
                        drawToolbarAndResetState();
                        Sleep(150);
                        continue;
                    }

                    // Solid/Dashed toggle
                    if (inRect(p.x, p.y, BTN_SOLID_DASH.left, BTN_SOLID_DASH.top, BTN_SOLID_DASH.right, BTN_SOLID_DASH.bottom)) {
                        currentLineMode = (*currentLineMode == 0) ? &dashedMode : &solidMode;
                        gNeedsRebuild = true;
                        drawToolbarAndResetState();
                        Sleep(150);
                        continue;
                    }

                    // Fill toggle
                    if (inRect(p.x, p.y, BTN_FILL_TOG.left, BTN_FILL_TOG.top, BTN_FILL_TOG.right, BTN_FILL_TOG.bottom)) {
                        fillEnabled = !fillEnabled;
                        gNeedsRebuild = true;
                        drawToolbarAndResetState();
                        Sleep(150);
                        continue;
                    }

                    // Save
                    if (inRect(p.x, p.y, BTN_SAVE.left, BTN_SAVE.top, BTN_SAVE.right, BTN_SAVE.bottom)) {
                        if (gNeedsRebuild) rebuildCanvas();
                        SaveCanvasToFile();
                        drawToolbarAndResetState();
                        Sleep(150);
                        continue;
                    }

                    // Load
                    if (inRect(p.x, p.y, BTN_LOAD.left, BTN_LOAD.top, BTN_LOAD.right, BTN_LOAD.bottom)) {
                        LoadCanvasFromFile();
                        drawToolbarAndResetState();
                        Sleep(150);
                        continue;
                    }
                }

                drawToolbarAndResetState();
                Sleep(150);
            }
            else {
                if (handlePaletteClick(p.x, p.y)) {
                    drawToolbarAndResetState();
                    gNeedsRebuild = true;
                    Sleep(150);
                }
                else {
                    if (currentTool == TOOL_ERASER) {
                        static POINT lastPointLocal = { -1, -1 };
                        if (!eraserDown) {
                            eraserTool.beginStroke(eraserRadius);
                            eraserTool.addDab(p);
                            eraserDown = true;
                            gNeedsRebuild = true;
                        }
                        else {
                            eraserTool.addInterpolatedDabs(lastPointLocal, p);
                            gNeedsRebuild = true;
                        }
                        lastPointLocal = p;
                    }
                    else if (currentTool == TOOL_FREEHAND) {
                        if (lastPoint.x != -1)
                            freehandTool.addStroke(lastPoint, p, currentLineMode);
                        lastPoint = p;
                        mouseReleased = false;
                        gNeedsRebuild = true;
                    }
                    else {
                        lastPoint = POINT{ -1, -1 };
                        if (mouseReleased) {
                            if (currentTool == TOOL_LINE) {
                                lineTool.addPoint(p);
                                if (lineTool.isReady()) {
                                    lineTool.drawAndReset(currentLineMode);
                                    gNeedsRebuild = true;
                                }
                            }
                            else if (currentTool == TOOL_TRIANGLE) {
                                triangleTool.addPoint(p);
                                if (triangleTool.isReady()) {
                                    triangleTool.drawAndReset(currentLineMode, fillEnabled);
                                    gNeedsRebuild = true;
                                }
                            }
                            else if (currentTool == TOOL_SQUARE) {
                                squareTool.addPoint(p);
                                if (squareTool.isReady()) {
                                    squareTool.drawAndReset(currentLineMode, fillEnabled);
                                    gNeedsRebuild = true;
                                }
                            }
                            else if (currentTool == TOOL_CIRCLE) {
                                circleTool.addPoint(p);
                                if (circleTool.isReady()) {
                                    circleTool.drawAndReset(currentLineMode);
                                    gNeedsRebuild = true;
                                }
                            }
                            else if (currentTool == TOOL_OVAL) {
                                ovalTool.addPoint(p);
                                if (ovalTool.isReady()) {
                                    ovalTool.drawAndReset(currentLineMode);
                                    gNeedsRebuild = true;
                                }
                            }
                            mouseReleased = false;
                        }
                    }
                }
            }
        }
        else {
            if (eraserDown) { eraserTool.endStroke(); eraserDown = false; }
            if (currentTool == TOOL_FREEHAND) { lastPoint = POINT{ -1, -1 }; }
            mouseReleased = true;
        }

        // --------- Render pass ----------
        POINT mouse;
        GetCursorPos(&mouse);
        ScreenToClient(GetHWnd(), &mouse);

        if (gNeedsRebuild) rebuildCanvas();

        if (!ImageReady(&gCanvas)) {
            // Failsafe: never blit an invalid image
            getimage(&gCanvas, 0, 0, 800, 600);
            SetWorkingImage(&gCanvas);
            setbkcolor(WHITE);
            cleardevice();
            SetWorkingImage();
        }

        putimage(0, 0, &gCanvas);
        drawToolbarAndResetState();
        setrop2(R2_COPYPEN);
        setlinecolor(BLACK);

        switch (currentTool) {
        case TOOL_LINE:     lineTool.drawPreview(mouse, currentLineMode);     break;
        case TOOL_TRIANGLE: triangleTool.drawPreview(mouse, currentLineMode); break;
        case TOOL_SQUARE:   squareTool.drawPreview(mouse, currentLineMode);   break;
        case TOOL_CIRCLE:   circleTool.drawPreview(mouse, currentLineMode);   break;
        case TOOL_OVAL:     ovalTool.drawPreview(mouse, currentLineMode);     break;
        default: break;
        }

        FlushBatchDraw();
        Sleep(10);
    }

    EndBatchDraw();
    closegraph();
    return 0;
}
