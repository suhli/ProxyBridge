#ifndef D2D_RENDERER_H
#define D2D_RENDERER_H

#include "modern_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    D2D_FONT_UI_12 = 0,
    D2D_FONT_UI_13,
    D2D_FONT_UI_14_BOLD,
    D2D_FONT_MONO_11_LIGHT,
    D2D_FONT_MONO_11,
    D2D_FONT_MONO_11_MEDIUM,
    D2D_FONT_MONO_11_SEMIBOLD,
    D2D_FONT_MONO_11_BOLD,
    D2D_FONT_MONO_12,
    D2D_FONT_MONO_12_BOLD,
    D2D_FONT_MONO_16_BOLD,
    D2D_FONT_COUNT
} D2dFontSlot;

typedef enum {
    D2D_ALIGN_LEFT = 0,
    D2D_ALIGN_CENTER,
    D2D_ALIGN_RIGHT
} D2dAlign;

// Opaque internal Direct2D / DirectWrite handles (pure-C safe)
typedef struct D2dCanvas {
    void *rt;     // ID2D1RenderTarget*
    void *brush;  // ID2D1SolidColorBrush*
} D2dCanvas;

typedef struct D2dLayeredSurface {
    HDC       memDC;
    HBITMAP   dib;
    HBITMAP   oldBmp;
    void     *bits;
    int       width;
    int       height;
    void     *dcRt;   // ID2D1DCRenderTarget*
    D2dCanvas canvas;
} D2dLayeredSurface;

typedef struct D2dHwndSurface {
    HWND      hwnd;
    int       width;
    int       height;
    void     *hwndRt; // ID2D1HwndRenderTarget*
    D2dCanvas canvas;
} D2dHwndSurface;

// Global Direct2D / DirectWrite factory lifecycle
BOOL D2d_Init(void);
void D2d_Shutdown(void);
BOOL D2d_IsReady(void);

// Theme & Win11 DWM visual helpers
BOOL D2d_IsSystemDarkMode(void);
BOOL D2d_ResolveDarkMode(int themeMode);
void D2d_ApplyWindowDwm(HWND hwnd, BOOL darkMode, BOOL acrylicBackdrop, BOOL roundCorners);

// Layered per-pixel alpha Direct2D surface (used by window_osd & floating_capsule)
BOOL D2dLayered_Create(D2dLayeredSurface *s, int width, int height);
BOOL D2dLayered_Resize(D2dLayeredSurface *s, int width, int height);
void D2dLayered_Destroy(D2dLayeredSurface *s);
BOOL D2dLayered_BeginDraw(D2dLayeredSurface *s);
BOOL D2dLayered_EndDrawAndPresent(D2dLayeredSurface *s, HWND hwnd, int screenX, int screenY, BYTE masterAlpha);

// HWND Direct2D surface (used by main_window_modern)
BOOL D2dHwnd_Ensure(D2dHwndSurface *s, HWND hwnd, int width, int height);
void D2dHwnd_Destroy(D2dHwndSurface *s);
BOOL D2dHwnd_BeginDraw(D2dHwndSurface *s);
BOOL D2dHwnd_EndDraw(D2dHwndSurface *s);

// Immediate GDI HDC Direct2D helper (anti-aliased rounded SPA buttons, segmented tabs, cards, menu pills)
BOOL D2dDc_Begin(HDC hdc, const RECT *rc, D2dCanvas *outCanvas);
void D2dDc_End(void);

// Drawing primitives (shared by layered & HWND surfaces)
void  D2d_SetScale(D2dCanvas *c, float scale);
void  D2d_Clear(D2dCanvas *c, uint32_t argb);
void  D2d_FillRect(D2dCanvas *c, float x, float y, float w, float h, uint32_t argb);
void  D2d_FillRoundedRect(D2dCanvas *c, float x, float y, float w, float h, float rx, float ry, uint32_t argb);
void  D2d_DrawRoundedRect(D2dCanvas *c, float x, float y, float w, float h, float rx, float ry, float strokeWidth, uint32_t argb);
void  D2d_FillCircle(D2dCanvas *c, float cx, float cy, float r, uint32_t argb);
void  D2d_DrawLine(D2dCanvas *c, float x0, float y0, float x1, float y1, float strokeWidth, uint32_t argb);
void  D2d_DrawText(D2dCanvas *c, D2dFontSlot font, const wchar_t *text,
                   float x, float y, float w, float h, uint32_t argb,
                   D2dAlign align, BOOL tabularFigures);
float D2d_MeasureTextWidth(D2dFontSlot font, const wchar_t *text, BOOL tabularFigures);
D2dFontSlot D2d_Mono11ForWeight(int weight);

#ifdef __cplusplus
}
#endif

#endif // D2D_RENDERER_H
