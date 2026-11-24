#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include "d2d_renderer.h"
#pragma warning(push)
#pragma warning(disable: 4201)
#include <d2d1.h>
#pragma warning(pop)
#include <dwmapi.h>
#include <objbase.h>
#include <string.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")

// ============================================================================
// Pure-C COM vtable definitions matching Windows SDK 10.0.26100.0 d2d1.h & dwrite.h
// (Windows SDK omits C vtables inside #ifndef D2D_USE_C_DEFINITIONS and dwrite.h)
// ============================================================================

static const GUID kIID_ID2D1Factory =
    { 0x06152247u, 0x6f50u, 0x465au, { 0x92u, 0x45u, 0x11u, 0x8bu, 0xfdu, 0x3bu, 0x60u, 0x07u } };

static const GUID kIID_IDWriteFactory =
    { 0xb859ee5au, 0xd838u, 0x4b5bu, { 0xa2u, 0xe8u, 0x1au, 0xdcu, 0x7du, 0x93u, 0xdbu, 0x48u } };

// ---- DirectWrite C ABI types & import ----
typedef enum {
    PB_DWRITE_FACTORY_TYPE_SHARED = 0,
    PB_DWRITE_FACTORY_TYPE_ISOLATED = 1
} PB_DWRITE_FACTORY_TYPE;

typedef enum {
    PB_DWRITE_FONT_WEIGHT_LIGHT = 300,
    PB_DWRITE_FONT_WEIGHT_NORMAL = 400,
    PB_DWRITE_FONT_WEIGHT_MEDIUM = 500,
    PB_DWRITE_FONT_WEIGHT_SEMI_BOLD = 600,
    PB_DWRITE_FONT_WEIGHT_BOLD = 700
} PB_DWRITE_FONT_WEIGHT;

typedef enum {
    PB_DWRITE_FONT_STYLE_NORMAL = 0
} PB_DWRITE_FONT_STYLE;

typedef enum {
    PB_DWRITE_FONT_STRETCH_NORMAL = 5
} PB_DWRITE_FONT_STRETCH;

typedef enum {
    PB_DWRITE_TEXT_ALIGNMENT_LEADING = 0,
    PB_DWRITE_TEXT_ALIGNMENT_TRAILING = 1,
    PB_DWRITE_TEXT_ALIGNMENT_CENTER = 2
} PB_DWRITE_TEXT_ALIGNMENT;

typedef enum {
    PB_DWRITE_PARAGRAPH_ALIGNMENT_NEAR = 0,
    PB_DWRITE_PARAGRAPH_ALIGNMENT_FAR = 1,
    PB_DWRITE_PARAGRAPH_ALIGNMENT_CENTER = 2
} PB_DWRITE_PARAGRAPH_ALIGNMENT;

typedef enum {
    PB_DWRITE_WORD_WRAPPING_WRAP = 0,
    PB_DWRITE_WORD_WRAPPING_NO_WRAP = 1
} PB_DWRITE_WORD_WRAPPING;

#define PB_DWRITE_MAKE_TAG(a,b,c,d) \
    (((UINT32)(UINT8)(d) << 24) | ((UINT32)(UINT8)(c) << 16) | ((UINT32)(UINT8)(b) << 8) | (UINT32)(UINT8)(a))
#define PB_DWRITE_TAG_TABULAR_FIGURES PB_DWRITE_MAKE_TAG('t','n','u','m')

typedef struct {
    UINT32 nameTag;
    UINT32 parameter;
} PB_DWRITE_FONT_FEATURE;

typedef struct {
    UINT32 startPosition;
    UINT32 length;
} PB_DWRITE_TEXT_RANGE;

typedef struct {
    FLOAT  left;
    FLOAT  top;
    FLOAT  width;
    FLOAT  widthIncludingTrailingWhitespace;
    FLOAT  height;
    FLOAT  layoutWidth;
    FLOAT  layoutHeight;
    UINT32 maxBidiReorderingDepth;
    UINT32 lineCount;
} PB_DWRITE_TEXT_METRICS;

typedef struct PB_IDWriteTypography PB_IDWriteTypography;
typedef struct PB_IDWriteTextFormat PB_IDWriteTextFormat;
typedef struct PB_IDWriteTextLayout PB_IDWriteTextLayout;
typedef struct PB_IDWriteFactory    PB_IDWriteFactory;

typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(PB_IDWriteTypography*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(PB_IDWriteTypography*);
    ULONG   (STDMETHODCALLTYPE *Release)(PB_IDWriteTypography*);
    HRESULT (STDMETHODCALLTYPE *AddFontFeature)(PB_IDWriteTypography*, PB_DWRITE_FONT_FEATURE);
    UINT32  (STDMETHODCALLTYPE *GetFontFeatureCount)(PB_IDWriteTypography*);
    HRESULT (STDMETHODCALLTYPE *GetFontFeature)(PB_IDWriteTypography*, UINT32, PB_DWRITE_FONT_FEATURE*);
} PB_IDWriteTypographyVtbl;
struct PB_IDWriteTypography { const PB_IDWriteTypographyVtbl *lpVtbl; };

typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(PB_IDWriteTextFormat*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(PB_IDWriteTextFormat*);
    ULONG   (STDMETHODCALLTYPE *Release)(PB_IDWriteTextFormat*);
    HRESULT (STDMETHODCALLTYPE *SetTextAlignment)(PB_IDWriteTextFormat*, PB_DWRITE_TEXT_ALIGNMENT);
    HRESULT (STDMETHODCALLTYPE *SetParagraphAlignment)(PB_IDWriteTextFormat*, PB_DWRITE_PARAGRAPH_ALIGNMENT);
    HRESULT (STDMETHODCALLTYPE *SetWordWrapping)(PB_IDWriteTextFormat*, PB_DWRITE_WORD_WRAPPING);
    void   *_pad[22];
} PB_IDWriteTextFormatVtbl;
struct PB_IDWriteTextFormat { const PB_IDWriteTextFormatVtbl *lpVtbl; };

typedef struct {
    // IUnknown (3) + IDWriteTextFormat (25) = 28 slots
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(PB_IDWriteTextLayout*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(PB_IDWriteTextLayout*);
    ULONG   (STDMETHODCALLTYPE *Release)(PB_IDWriteTextLayout*);
    HRESULT (STDMETHODCALLTYPE *SetTextAlignment)(PB_IDWriteTextLayout*, PB_DWRITE_TEXT_ALIGNMENT);
    HRESULT (STDMETHODCALLTYPE *SetParagraphAlignment)(PB_IDWriteTextLayout*, PB_DWRITE_PARAGRAPH_ALIGNMENT);
    HRESULT (STDMETHODCALLTYPE *SetWordWrapping)(PB_IDWriteTextLayout*, PB_DWRITE_WORD_WRAPPING);
    void   *_fmtPad[22];
    // IDWriteTextLayout slots [0..38]
    void   *_layPad0[12]; // [0..11]
    HRESULT (STDMETHODCALLTYPE *SetTypography)(PB_IDWriteTextLayout*, PB_IDWriteTypography*, PB_DWRITE_TEXT_RANGE); // [12]
    void   *_layPad1[19]; // [13..31]
    HRESULT (STDMETHODCALLTYPE *GetMetrics)(PB_IDWriteTextLayout*, PB_DWRITE_TEXT_METRICS*); // [32]
    void   *_layPad2[6];  // [33..38]
} PB_IDWriteTextLayoutVtbl;
struct PB_IDWriteTextLayout { const PB_IDWriteTextLayoutVtbl *lpVtbl; };

typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(PB_IDWriteFactory*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(PB_IDWriteFactory*);
    ULONG   (STDMETHODCALLTYPE *Release)(PB_IDWriteFactory*);
    void   *_pad0[12]; // [0..11]
    HRESULT (STDMETHODCALLTYPE *CreateTextFormat)(
        PB_IDWriteFactory*, const WCHAR*, void*, PB_DWRITE_FONT_WEIGHT,
        PB_DWRITE_FONT_STYLE, PB_DWRITE_FONT_STRETCH, FLOAT, const WCHAR*, PB_IDWriteTextFormat**); // [12]
    HRESULT (STDMETHODCALLTYPE *CreateTypography)(PB_IDWriteFactory*, PB_IDWriteTypography**);       // [13]
    void   *_pad1[1];  // [14]
    HRESULT (STDMETHODCALLTYPE *CreateTextLayout)(
        PB_IDWriteFactory*, const WCHAR*, UINT32, PB_IDWriteTextFormat*, FLOAT, FLOAT, PB_IDWriteTextLayout**); // [15]
    void   *_pad2[5];  // [16..20]
} PB_IDWriteFactoryVtbl;
struct PB_IDWriteFactory { const PB_IDWriteFactoryVtbl *lpVtbl; };

HRESULT WINAPI DWriteCreateFactory(PB_DWRITE_FACTORY_TYPE factoryType, REFIID iid, IUnknown **factory);

// ---- Direct2D C ABI vtables ----
typedef struct PB_ID2D1SolidColorBrush PB_ID2D1SolidColorBrush;
typedef struct PB_ID2D1RenderTarget    PB_ID2D1RenderTarget;
typedef struct PB_ID2D1HwndRenderTarget PB_ID2D1HwndRenderTarget;
typedef struct PB_ID2D1DCRenderTarget  PB_ID2D1DCRenderTarget;
typedef struct PB_ID2D1Factory         PB_ID2D1Factory;

typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(PB_ID2D1SolidColorBrush*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(PB_ID2D1SolidColorBrush*);
    ULONG   (STDMETHODCALLTYPE *Release)(PB_ID2D1SolidColorBrush*);
    void    (STDMETHODCALLTYPE *GetFactory)(PB_ID2D1SolidColorBrush*, void**);
    void    (STDMETHODCALLTYPE *SetOpacity)(PB_ID2D1SolidColorBrush*, FLOAT);
    void    (STDMETHODCALLTYPE *SetTransform)(PB_ID2D1SolidColorBrush*, const D2D1_MATRIX_3X2_F*);
    FLOAT   (STDMETHODCALLTYPE *GetOpacity)(PB_ID2D1SolidColorBrush*);
    void    (STDMETHODCALLTYPE *GetTransform)(PB_ID2D1SolidColorBrush*, D2D1_MATRIX_3X2_F*);
    void    (STDMETHODCALLTYPE *SetColor)(PB_ID2D1SolidColorBrush*, const D2D1_COLOR_F*);
    void   *_padGetColor;
} PB_ID2D1SolidColorBrushVtbl;
struct PB_ID2D1SolidColorBrush { const PB_ID2D1SolidColorBrushVtbl *lpVtbl; };

#define PB_D2D1_RT_METHODS(T) \
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(T*, REFIID, void**); \
    ULONG   (STDMETHODCALLTYPE *AddRef)(T*); \
    ULONG   (STDMETHODCALLTYPE *Release)(T*); \
    void    (STDMETHODCALLTYPE *GetFactory)(T*, void**); \
    void   *_rtPad0[4]; /* [0..3] */ \
    HRESULT (STDMETHODCALLTYPE *CreateSolidColorBrush)(T*, const D2D1_COLOR_F*, const D2D1_BRUSH_PROPERTIES*, PB_ID2D1SolidColorBrush**); /* [4] */ \
    void   *_rtPad1[6]; /* [5..10] */ \
    void    (STDMETHODCALLTYPE *DrawLine)(T*, D2D1_POINT_2F, D2D1_POINT_2F, void*, FLOAT, void*); /* [11] */ \
    void    (STDMETHODCALLTYPE *DrawRectangle)(T*, const D2D1_RECT_F*, void*, FLOAT, void*);      /* [12] */ \
    void    (STDMETHODCALLTYPE *FillRectangle)(T*, const D2D1_RECT_F*, void*);                    /* [13] */ \
    void    (STDMETHODCALLTYPE *DrawRoundedRectangle)(T*, const D2D1_ROUNDED_RECT*, void*, FLOAT, void*); /* [14] */ \
    void    (STDMETHODCALLTYPE *FillRoundedRectangle)(T*, const D2D1_ROUNDED_RECT*, void*);       /* [15] */ \
    void    (STDMETHODCALLTYPE *DrawEllipse)(T*, const D2D1_ELLIPSE*, void*, FLOAT, void*);       /* [16] */ \
    void    (STDMETHODCALLTYPE *FillEllipse)(T*, const D2D1_ELLIPSE*, void*);                     /* [17] */ \
    void   *_rtPad2[5]; /* [18..22] */ \
    void    (STDMETHODCALLTYPE *DrawText)(T*, const WCHAR*, UINT32, void*, const D2D1_RECT_F*, void*, D2D1_DRAW_TEXT_OPTIONS, DWRITE_MEASURING_MODE); /* [23] */ \
    void    (STDMETHODCALLTYPE *DrawTextLayout)(T*, D2D1_POINT_2F, void*, void*, D2D1_DRAW_TEXT_OPTIONS); /* [24] */ \
    void   *_rtPad3[1]; /* [25] */ \
    void    (STDMETHODCALLTYPE *SetTransform)(T*, const D2D1_MATRIX_3X2_F*);                      /* [26] */ \
    void   *_rtPad3b[1]; /* [27] */ \
    void    (STDMETHODCALLTYPE *SetAntialiasMode)(T*, D2D1_ANTIALIAS_MODE);                       /* [28] */ \
    void   *_rtPad4[1]; /* [29] */ \
    void    (STDMETHODCALLTYPE *SetTextAntialiasMode)(T*, D2D1_TEXT_ANTIALIAS_MODE);              /* [30] */ \
    void   *_rtPad5[12]; /* [31..42] */ \
    void    (STDMETHODCALLTYPE *Clear)(T*, const D2D1_COLOR_F*);                                  /* [43] */ \
    void    (STDMETHODCALLTYPE *BeginDraw)(T*);                                                   /* [44] */ \
    HRESULT (STDMETHODCALLTYPE *EndDraw)(T*, D2D1_TAG*, D2D1_TAG*);                               /* [45] */ \
    void   *_rtPad6[1]; /* [46] */ \
    void    (STDMETHODCALLTYPE *SetDpi)(T*, FLOAT, FLOAT);                                        /* [47] */ \
    void   *_rtPad7[5]; /* [48..52] */

typedef struct {
    PB_D2D1_RT_METHODS(PB_ID2D1RenderTarget)
} PB_ID2D1RenderTargetVtbl;
struct PB_ID2D1RenderTarget { const PB_ID2D1RenderTargetVtbl *lpVtbl; };

typedef struct {
    PB_D2D1_RT_METHODS(PB_ID2D1HwndRenderTarget)
    D2D1_WINDOW_STATE (STDMETHODCALLTYPE *CheckWindowState)(PB_ID2D1HwndRenderTarget*);
    HRESULT (STDMETHODCALLTYPE *Resize)(PB_ID2D1HwndRenderTarget*, const D2D1_SIZE_U*);
    HWND    (STDMETHODCALLTYPE *GetHwnd)(PB_ID2D1HwndRenderTarget*);
} PB_ID2D1HwndRenderTargetVtbl;
struct PB_ID2D1HwndRenderTarget { const PB_ID2D1HwndRenderTargetVtbl *lpVtbl; };

typedef struct {
    PB_D2D1_RT_METHODS(PB_ID2D1DCRenderTarget)
    HRESULT (STDMETHODCALLTYPE *BindDC)(PB_ID2D1DCRenderTarget*, const HDC, const RECT*);
} PB_ID2D1DCRenderTargetVtbl;
struct PB_ID2D1DCRenderTarget { const PB_ID2D1DCRenderTargetVtbl *lpVtbl; };

typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(PB_ID2D1Factory*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(PB_ID2D1Factory*);
    ULONG   (STDMETHODCALLTYPE *Release)(PB_ID2D1Factory*);
    void   *_facPad0[11]; // [0..10]
    HRESULT (STDMETHODCALLTYPE *CreateHwndRenderTarget)(
        PB_ID2D1Factory*, const D2D1_RENDER_TARGET_PROPERTIES*,
        const D2D1_HWND_RENDER_TARGET_PROPERTIES*, PB_ID2D1HwndRenderTarget**); // [11]
    void   *_facPad1[1];  // [12]
    HRESULT (STDMETHODCALLTYPE *CreateDCRenderTarget)(
        PB_ID2D1Factory*, const D2D1_RENDER_TARGET_PROPERTIES*,
        PB_ID2D1DCRenderTarget**); // [13]
} PB_ID2D1FactoryVtbl;
struct PB_ID2D1Factory { const PB_ID2D1FactoryVtbl *lpVtbl; };

// Safe release helper
#define SAFE_COM_RELEASE(p) do { if (p) { IUnknown_Release((IUnknown*)(p)); (p) = NULL; } } while (0)

// ============================================================================
// Global factory & typography state
// ============================================================================

static BOOL                     g_d2dReady = FALSE;
static BOOL                     g_comOwned = FALSE;
static PB_ID2D1Factory         *g_d2dFactory = NULL;
static PB_IDWriteFactory       *g_dwFactory = NULL;
static PB_IDWriteTypography    *g_tabularTypo = NULL;
static PB_IDWriteTextFormat    *g_fonts[D2D_FONT_COUNT];
static PB_ID2D1DCRenderTarget  *g_sharedDcRt = NULL;
static PB_ID2D1SolidColorBrush *g_sharedDcBrush = NULL;

static D2D1_COLOR_F ArgbToD2dColor(uint32_t argb)
{
    D2D1_COLOR_F c;
    c.a = (float)((argb >> 24) & 0xFFu) / 255.0f;
    c.r = (float)((argb >> 16) & 0xFFu) / 255.0f;
    c.g = (float)((argb >>  8) & 0xFFu) / 255.0f;
    c.b = (float)( argb        & 0xFFu) / 255.0f;
    return c;
}

static BOOL IsFontInstalled(const wchar_t *familyName)
{
    HDC dc = GetDC(NULL);
    if (!dc) return FALSE;
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, familyName, LF_FACESIZE);

    BOOL found = FALSE;
    // EnumFontFamiliesExW with callback via minimal static thunk
    HFONT hf = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, DEFAULT_PITCH, familyName);
    if (hf) {
        HFONT old = (HFONT)SelectObject(dc, hf);
        wchar_t actual[LF_FACESIZE] = {0};
        if (GetTextFaceW(dc, LF_FACESIZE, actual) > 0 && _wcsicmp(actual, familyName) == 0) {
            found = TRUE;
        }
        SelectObject(dc, old);
        DeleteObject(hf);
    }
    ReleaseDC(NULL, dc);
    return found;
}

static PB_IDWriteTextFormat *CreateFontSlot(const wchar_t *primary, const wchar_t *fallback,
                                            PB_DWRITE_FONT_WEIGHT weight, float sizePx)
{
    if (!g_dwFactory) return NULL;
    const wchar_t *chosen = IsFontInstalled(primary) ? primary : fallback;
    PB_IDWriteTextFormat *fmt = NULL;
    HRESULT hr = g_dwFactory->lpVtbl->CreateTextFormat(
        g_dwFactory, chosen, NULL, weight,
        PB_DWRITE_FONT_STYLE_NORMAL, PB_DWRITE_FONT_STRETCH_NORMAL,
        sizePx, L"", &fmt);
    if (FAILED(hr) && chosen != fallback) {
        hr = g_dwFactory->lpVtbl->CreateTextFormat(
            g_dwFactory, fallback, NULL, weight,
            PB_DWRITE_FONT_STYLE_NORMAL, PB_DWRITE_FONT_STRETCH_NORMAL,
            sizePx, L"", &fmt);
    }
    if (SUCCEEDED(hr) && fmt) {
        fmt->lpVtbl->SetParagraphAlignment(fmt, PB_DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        fmt->lpVtbl->SetWordWrapping(fmt, PB_DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    return fmt;
}

BOOL D2d_Init(void)
{
    if (g_d2dReady) return TRUE;

    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hrCom)) g_comOwned = TRUE;

    D2D1_FACTORY_OPTIONS opts;
    opts.debugLevel = D2D1_DEBUG_LEVEL_NONE;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &kIID_ID2D1Factory, &opts, (void **)&g_d2dFactory);
    if (FAILED(hr) || !g_d2dFactory) {
        D2d_Shutdown();
        return FALSE;
    }

    hr = DWriteCreateFactory(PB_DWRITE_FACTORY_TYPE_SHARED, &kIID_IDWriteFactory, (IUnknown **)&g_dwFactory);
    if (FAILED(hr) || !g_dwFactory) {
        D2d_Shutdown();
        return FALSE;
    }

    if (SUCCEEDED(g_dwFactory->lpVtbl->CreateTypography(g_dwFactory, &g_tabularTypo)) && g_tabularTypo) {
        PB_DWRITE_FONT_FEATURE feat;
        feat.nameTag   = PB_DWRITE_TAG_TABULAR_FIGURES;
        feat.parameter = 1;
        g_tabularTypo->lpVtbl->AddFontFeature(g_tabularTypo, feat);
    }

    g_fonts[D2D_FONT_UI_12]            = CreateFontSlot(MODERN_FONT_UI,   MODERN_FONT_UI_FALLBACK,   PB_DWRITE_FONT_WEIGHT_NORMAL,    12.0f);
    g_fonts[D2D_FONT_UI_13]            = CreateFontSlot(MODERN_FONT_UI,   MODERN_FONT_UI_FALLBACK,   PB_DWRITE_FONT_WEIGHT_NORMAL,    13.0f);
    g_fonts[D2D_FONT_UI_14_BOLD]       = CreateFontSlot(MODERN_FONT_UI,   MODERN_FONT_UI_FALLBACK,   PB_DWRITE_FONT_WEIGHT_SEMI_BOLD, 14.0f);
    g_fonts[D2D_FONT_MONO_11_LIGHT]    = CreateFontSlot(MODERN_FONT_MONO, MODERN_FONT_MONO_FALLBACK, PB_DWRITE_FONT_WEIGHT_LIGHT,     11.0f);
    g_fonts[D2D_FONT_MONO_11]          = CreateFontSlot(MODERN_FONT_MONO, MODERN_FONT_MONO_FALLBACK, PB_DWRITE_FONT_WEIGHT_NORMAL,    11.0f);
    g_fonts[D2D_FONT_MONO_11_MEDIUM]   = CreateFontSlot(MODERN_FONT_MONO, MODERN_FONT_MONO_FALLBACK, PB_DWRITE_FONT_WEIGHT_MEDIUM,    11.0f);
    g_fonts[D2D_FONT_MONO_11_SEMIBOLD] = CreateFontSlot(MODERN_FONT_MONO, MODERN_FONT_MONO_FALLBACK, PB_DWRITE_FONT_WEIGHT_SEMI_BOLD, 11.0f);
    g_fonts[D2D_FONT_MONO_11_BOLD]     = CreateFontSlot(MODERN_FONT_MONO, MODERN_FONT_MONO_FALLBACK, PB_DWRITE_FONT_WEIGHT_BOLD,      11.0f);
    g_fonts[D2D_FONT_MONO_12]          = CreateFontSlot(MODERN_FONT_MONO, MODERN_FONT_MONO_FALLBACK, PB_DWRITE_FONT_WEIGHT_NORMAL,    12.0f);
    g_fonts[D2D_FONT_MONO_12_BOLD]     = CreateFontSlot(MODERN_FONT_MONO, MODERN_FONT_MONO_FALLBACK, PB_DWRITE_FONT_WEIGHT_SEMI_BOLD, 12.0f);
    g_fonts[D2D_FONT_MONO_16_BOLD]     = CreateFontSlot(MODERN_FONT_MONO, MODERN_FONT_MONO_FALLBACK, PB_DWRITE_FONT_WEIGHT_BOLD,      16.0f);

    g_d2dReady = TRUE;
    return TRUE;
}

void D2d_Shutdown(void)
{
    SAFE_COM_RELEASE(g_sharedDcBrush);
    SAFE_COM_RELEASE(g_sharedDcRt);
    for (int i = 0; i < D2D_FONT_COUNT; i++) {
        SAFE_COM_RELEASE(g_fonts[i]);
    }
    SAFE_COM_RELEASE(g_tabularTypo);
    SAFE_COM_RELEASE(g_dwFactory);
    SAFE_COM_RELEASE(g_d2dFactory);
    if (g_comOwned) {
        CoUninitialize();
        g_comOwned = FALSE;
    }
    g_d2dReady = FALSE;
}

BOOL D2d_IsReady(void)
{
    return g_d2dReady;
}

// ============================================================================
// Theme & Win11 DWM visual helpers
// ============================================================================

BOOL D2d_IsSystemDarkMode(void)
{
    HKEY hk = NULL;
    DWORD light = 0;
    DWORD cb = sizeof(light);
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD type = 0;
        if (RegQueryValueExW(hk, L"AppsUseLightTheme", NULL, &type, (LPBYTE)&light, &cb) == ERROR_SUCCESS &&
            type == REG_DWORD) {
            RegCloseKey(hk);
            return light == 0;
        }
        RegCloseKey(hk);
    }
    return TRUE; // default dark
}

BOOL D2d_ResolveDarkMode(int themeMode)
{
    if (themeMode == MODERN_THEME_DARK)  return TRUE;
    if (themeMode == MODERN_THEME_LIGHT) return FALSE;
    return D2d_IsSystemDarkMode();
}

void D2d_ApplyWindowDwm(HWND hwnd, BOOL darkMode, BOOL acrylicBackdrop, BOOL roundCorners)
{
    if (!hwnd || !IsWindow(hwnd)) return;

    BOOL dark = darkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    if (roundCorners) {
        DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    }

    if (acrylicBackdrop) {
        DWORD backdrop = (DWORD)DWMSBT_TRANSIENTWINDOW;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    } else {
        DWORD backdrop = (DWORD)DWMSBT_NONE;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    }
}

// ============================================================================
// Layered Direct2D Surface (ID2D1DCRenderTarget + 32-bit premultiplied DIB)
// ============================================================================

static BOOL D2dLayered_AllocDib(D2dLayeredSurface *s, int width, int height)
{
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;

    if (s->memDC && s->oldBmp) {
        SelectObject(s->memDC, s->oldBmp);
        s->oldBmp = NULL;
    }
    if (s->dib) {
        DeleteObject(s->dib);
        s->dib = NULL;
        s->bits = NULL;
    }
    if (!s->memDC) {
        HDC screen = GetDC(NULL);
        s->memDC = CreateCompatibleDC(screen);
        ReleaseDC(NULL, screen);
        if (!s->memDC) return FALSE;
    }

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height; // top-down DIB
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    s->dib = CreateDIBSection(s->memDC, &bmi, DIB_RGB_COLORS, &s->bits, NULL, 0);
    if (!s->dib || !s->bits) return FALSE;
    s->oldBmp = (HBITMAP)SelectObject(s->memDC, s->dib);
    s->width  = width;
    s->height = height;
    return TRUE;
}

BOOL D2dLayered_Create(D2dLayeredSurface *s, int width, int height)
{
    if (!s) return FALSE;
    ZeroMemory(s, sizeof(*s));
    if (!g_d2dReady && !D2d_Init()) return FALSE;

    if (!D2dLayered_AllocDib(s, width, height)) {
        D2dLayered_Destroy(s);
        return FALSE;
    }

    D2D1_RENDER_TARGET_PROPERTIES props;
    ZeroMemory(&props, sizeof(props));
    props.type                  = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX                  = 96.0f;
    props.dpiY                  = 96.0f;
    props.usage                 = D2D1_RENDER_TARGET_USAGE_NONE;
    props.minLevel              = D2D1_FEATURE_LEVEL_DEFAULT;

    PB_ID2D1DCRenderTarget *dcRt = NULL;
    HRESULT hr = g_d2dFactory->lpVtbl->CreateDCRenderTarget(g_d2dFactory, &props, &dcRt);
    if (FAILED(hr) || !dcRt) {
        D2dLayered_Destroy(s);
        return FALSE;
    }

    D2D1_COLOR_F white = { 1.0f, 1.0f, 1.0f, 1.0f };
    PB_ID2D1SolidColorBrush *brush = NULL;
    hr = dcRt->lpVtbl->CreateSolidColorBrush(dcRt, &white, NULL, &brush);
    if (FAILED(hr) || !brush) {
        SAFE_COM_RELEASE(dcRt);
        D2dLayered_Destroy(s);
        return FALSE;
    }

    dcRt->lpVtbl->SetAntialiasMode(dcRt, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    dcRt->lpVtbl->SetTextAntialiasMode(dcRt, D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    s->dcRt         = dcRt;
    s->canvas.rt    = dcRt;
    s->canvas.brush = brush;
    return TRUE;
}

BOOL D2dLayered_Resize(D2dLayeredSurface *s, int width, int height)
{
    if (!s) return FALSE;
    if (s->width == width && s->height == height && s->dib != NULL) return TRUE;
    if (!s->dcRt) return D2dLayered_Create(s, width, height);
    return D2dLayered_AllocDib(s, width, height);
}

void D2dLayered_Destroy(D2dLayeredSurface *s)
{
    if (!s) return;
    SAFE_COM_RELEASE(s->canvas.brush);
    SAFE_COM_RELEASE(s->dcRt);
    s->canvas.rt = NULL;
    if (s->memDC && s->oldBmp) {
        SelectObject(s->memDC, s->oldBmp);
        s->oldBmp = NULL;
    }
    if (s->dib) {
        DeleteObject(s->dib);
        s->dib = NULL;
    }
    if (s->memDC) {
        DeleteDC(s->memDC);
        s->memDC = NULL;
    }
    s->bits = NULL;
    s->width = 0;
    s->height = 0;
}

BOOL D2dLayered_BeginDraw(D2dLayeredSurface *s)
{
    if (!s || !s->dcRt || !s->memDC || !s->dib) return FALSE;
    RECT rc = { 0, 0, s->width, s->height };
    PB_ID2D1DCRenderTarget *rt = (PB_ID2D1DCRenderTarget *)s->dcRt;
    if (FAILED(rt->lpVtbl->BindDC(rt, s->memDC, &rc))) return FALSE;
    rt->lpVtbl->BeginDraw(rt);
    D2D1_MATRIX_3X2_F ident = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    rt->lpVtbl->SetTransform(rt, &ident);
    D2D1_COLOR_F clear = { 0.0f, 0.0f, 0.0f, 0.0f };
    rt->lpVtbl->Clear(rt, &clear);
    return TRUE;
}

BOOL D2dLayered_EndDrawAndPresent(D2dLayeredSurface *s, HWND hwnd, int screenX, int screenY, BYTE masterAlpha)
{
    if (!s || !s->dcRt || !hwnd) return FALSE;
    PB_ID2D1DCRenderTarget *rt = (PB_ID2D1DCRenderTarget *)s->dcRt;
    HRESULT hr = rt->lpVtbl->EndDraw(rt, NULL, NULL);
    if (hr == (HRESULT)D2DERR_RECREATE_TARGET) {
        int w = s->width, h = s->height;
        D2dLayered_Destroy(s);
        D2dLayered_Create(s, w, h);
        return FALSE;
    }
    if (FAILED(hr)) return FALSE;

    POINT ptDst = { screenX, screenY };
    POINT ptSrc = { 0, 0 };
    SIZE  sz    = { s->width, s->height };
    BLENDFUNCTION bf;
    bf.BlendOp             = AC_SRC_OVER;
    bf.BlendFlags          = 0;
    bf.SourceConstantAlpha = masterAlpha;
    bf.AlphaFormat         = AC_SRC_ALPHA;

    HDC screen = GetDC(NULL);
    BOOL ok = UpdateLayeredWindow(hwnd, screen, &ptDst, &sz, s->memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, screen);
    return ok;
}

// ============================================================================
// HWND Direct2D Surface (for main window rendering in Stage 4)
// ============================================================================

BOOL D2dHwnd_Ensure(D2dHwndSurface *s, HWND hwnd, int width, int height)
{
    if (!s || !hwnd) return FALSE;
    if (!g_d2dReady && !D2d_Init()) return FALSE;
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;

    if (s->hwndRt && s->hwnd == hwnd) {
        if (s->width != width || s->height != height) {
            D2D1_SIZE_U sz = { (UINT32)width, (UINT32)height };
            PB_ID2D1HwndRenderTarget *rt = (PB_ID2D1HwndRenderTarget *)s->hwndRt;
            if (SUCCEEDED(rt->lpVtbl->Resize(rt, &sz))) {
                s->width  = width;
                s->height = height;
            }
        }
        return TRUE;
    }

    D2dHwnd_Destroy(s);

    D2D1_RENDER_TARGET_PROPERTIES props;
    ZeroMemory(&props, sizeof(props));
    props.type                  = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX                  = 96.0f;
    props.dpiY                  = 96.0f;

    D2D1_HWND_RENDER_TARGET_PROPERTIES hprops;
    hprops.hwnd           = hwnd;
    hprops.pixelSize.width  = (UINT32)width;
    hprops.pixelSize.height = (UINT32)height;
    hprops.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

    PB_ID2D1HwndRenderTarget *rt = NULL;
    HRESULT hr = g_d2dFactory->lpVtbl->CreateHwndRenderTarget(g_d2dFactory, &props, &hprops, &rt);
    if (FAILED(hr) || !rt) return FALSE;

    D2D1_COLOR_F white = { 1.0f, 1.0f, 1.0f, 1.0f };
    PB_ID2D1SolidColorBrush *brush = NULL;
    hr = rt->lpVtbl->CreateSolidColorBrush(rt, &white, NULL, &brush);
    if (FAILED(hr) || !brush) {
        SAFE_COM_RELEASE(rt);
        return FALSE;
    }

    rt->lpVtbl->SetAntialiasMode(rt, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    rt->lpVtbl->SetTextAntialiasMode(rt, D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    s->hwnd         = hwnd;
    s->width        = width;
    s->height       = height;
    s->hwndRt       = rt;
    s->canvas.rt    = rt;
    s->canvas.brush = brush;
    return TRUE;
}

void D2dHwnd_Destroy(D2dHwndSurface *s)
{
    if (!s) return;
    SAFE_COM_RELEASE(s->canvas.brush);
    SAFE_COM_RELEASE(s->hwndRt);
    s->canvas.rt = NULL;
    s->hwnd = NULL;
    s->width = 0;
    s->height = 0;
}

BOOL D2dHwnd_BeginDraw(D2dHwndSurface *s)
{
    if (!s || !s->hwndRt) return FALSE;
    PB_ID2D1HwndRenderTarget *rt = (PB_ID2D1HwndRenderTarget *)s->hwndRt;
    rt->lpVtbl->BeginDraw(rt);
    return TRUE;
}

BOOL D2dHwnd_EndDraw(D2dHwndSurface *s)
{
    if (!s || !s->hwndRt) return FALSE;
    PB_ID2D1HwndRenderTarget *rt = (PB_ID2D1HwndRenderTarget *)s->hwndRt;
    HRESULT hr = rt->lpVtbl->EndDraw(rt, NULL, NULL);
    if (hr == (HRESULT)D2DERR_RECREATE_TARGET) {
        D2dHwnd_Destroy(s);
        return FALSE;
    }
    return SUCCEEDED(hr);
}

// ============================================================================
// Immediate GDI HDC Direct2D helper (opaque DC bind for SPA controls & cards)
// ============================================================================

static BOOL EnsureSharedDcRt(void)
{
    if (g_sharedDcRt && g_sharedDcBrush) return TRUE;
    if (!g_d2dReady && !D2d_Init()) return FALSE;

    D2D1_RENDER_TARGET_PROPERTIES props;
    ZeroMemory(&props, sizeof(props));
    props.type                  = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    props.dpiX                  = 96.0f;
    props.dpiY                  = 96.0f;
    props.usage                 = D2D1_RENDER_TARGET_USAGE_NONE;
    props.minLevel              = D2D1_FEATURE_LEVEL_DEFAULT;

    PB_ID2D1DCRenderTarget *rt = NULL;
    HRESULT hr = g_d2dFactory->lpVtbl->CreateDCRenderTarget(g_d2dFactory, &props, &rt);
    if (FAILED(hr) || !rt) return FALSE;

    D2D1_COLOR_F white = { 1.0f, 1.0f, 1.0f, 1.0f };
    PB_ID2D1SolidColorBrush *brush = NULL;
    hr = rt->lpVtbl->CreateSolidColorBrush(rt, &white, NULL, &brush);
    if (FAILED(hr) || !brush) {
        SAFE_COM_RELEASE(rt);
        return FALSE;
    }

    rt->lpVtbl->SetAntialiasMode(rt, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    rt->lpVtbl->SetTextAntialiasMode(rt, D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    g_sharedDcRt    = rt;
    g_sharedDcBrush = brush;
    return TRUE;
}

BOOL D2dDc_Begin(HDC hdc, const RECT *rc, D2dCanvas *outCanvas)
{
    if (!hdc || !rc || !outCanvas) return FALSE;
    if ((rc->right - rc->left) <= 0 || (rc->bottom - rc->top) <= 0) return FALSE;
    if (!EnsureSharedDcRt()) return FALSE;

    if (FAILED(g_sharedDcRt->lpVtbl->BindDC(g_sharedDcRt, hdc, rc))) return FALSE;
    g_sharedDcRt->lpVtbl->BeginDraw(g_sharedDcRt);
    D2D1_MATRIX_3X2_F ident = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    g_sharedDcRt->lpVtbl->SetTransform(g_sharedDcRt, &ident);
    outCanvas->rt    = g_sharedDcRt;
    outCanvas->brush = g_sharedDcBrush;
    return TRUE;
}

void D2dDc_End(void)
{
    if (!g_sharedDcRt) return;
    HRESULT hr = g_sharedDcRt->lpVtbl->EndDraw(g_sharedDcRt, NULL, NULL);
    if (hr == (HRESULT)D2DERR_RECREATE_TARGET) {
        SAFE_COM_RELEASE(g_sharedDcBrush);
        SAFE_COM_RELEASE(g_sharedDcRt);
    }
}

// ============================================================================
// Shared Drawing Primitives
// ============================================================================

static void SetBrushColor(D2dCanvas *c, uint32_t argb)
{
    PB_ID2D1SolidColorBrush *b = (PB_ID2D1SolidColorBrush *)c->brush;
    D2D1_COLOR_F col = ArgbToD2dColor(argb);
    b->lpVtbl->SetColor(b, &col);
}

void D2d_SetScale(D2dCanvas *c, float scale)
{
    if (!c || !c->rt) return;
    if (scale <= 0.1f) scale = 1.0f;
    PB_ID2D1RenderTarget *rt = (PB_ID2D1RenderTarget *)c->rt;
    D2D1_MATRIX_3X2_F m = { scale, 0.0f, 0.0f, scale, 0.0f, 0.0f };
    rt->lpVtbl->SetTransform(rt, &m);
}

void D2d_Clear(D2dCanvas *c, uint32_t argb)
{
    if (!c || !c->rt) return;
    PB_ID2D1RenderTarget *rt = (PB_ID2D1RenderTarget *)c->rt;
    D2D1_COLOR_F col = ArgbToD2dColor(argb);
    rt->lpVtbl->Clear(rt, &col);
}

void D2d_FillRect(D2dCanvas *c, float x, float y, float w, float h, uint32_t argb)
{
    if (!c || !c->rt || !c->brush) return;
    PB_ID2D1RenderTarget *rt = (PB_ID2D1RenderTarget *)c->rt;
    D2D1_RECT_F r = { x, y, x + w, y + h };
    SetBrushColor(c, argb);
    rt->lpVtbl->FillRectangle(rt, &r, c->brush);
}

void D2d_FillRoundedRect(D2dCanvas *c, float x, float y, float w, float h, float rx, float ry, uint32_t argb)
{
    if (!c || !c->rt || !c->brush) return;
    PB_ID2D1RenderTarget *rt = (PB_ID2D1RenderTarget *)c->rt;
    D2D1_ROUNDED_RECT rr;
    rr.rect.left   = x;
    rr.rect.top    = y;
    rr.rect.right  = x + w;
    rr.rect.bottom = y + h;
    rr.radiusX     = rx;
    rr.radiusY     = ry;
    SetBrushColor(c, argb);
    rt->lpVtbl->FillRoundedRectangle(rt, &rr, c->brush);
}

void D2d_DrawRoundedRect(D2dCanvas *c, float x, float y, float w, float h, float rx, float ry, float strokeWidth, uint32_t argb)
{
    if (!c || !c->rt || !c->brush) return;
    PB_ID2D1RenderTarget *rt = (PB_ID2D1RenderTarget *)c->rt;
    D2D1_ROUNDED_RECT rr;
    rr.rect.left   = x;
    rr.rect.top    = y;
    rr.rect.right  = x + w;
    rr.rect.bottom = y + h;
    rr.radiusX     = rx;
    rr.radiusY     = ry;
    SetBrushColor(c, argb);
    rt->lpVtbl->DrawRoundedRectangle(rt, &rr, c->brush, strokeWidth, NULL);
}

void D2d_FillCircle(D2dCanvas *c, float cx, float cy, float r, uint32_t argb)
{
    if (!c || !c->rt || !c->brush) return;
    PB_ID2D1RenderTarget *rt = (PB_ID2D1RenderTarget *)c->rt;
    D2D1_ELLIPSE el;
    el.point.x = cx;
    el.point.y = cy;
    el.radiusX = r;
    el.radiusY = r;
    SetBrushColor(c, argb);
    rt->lpVtbl->FillEllipse(rt, &el, c->brush);
}

void D2d_DrawLine(D2dCanvas *c, float x0, float y0, float x1, float y1, float strokeWidth, uint32_t argb)
{
    if (!c || !c->rt || !c->brush) return;
    PB_ID2D1RenderTarget *rt = (PB_ID2D1RenderTarget *)c->rt;
    D2D1_POINT_2F p0 = { x0, y0 };
    D2D1_POINT_2F p1 = { x1, y1 };
    SetBrushColor(c, argb);
    rt->lpVtbl->DrawLine(rt, p0, p1, c->brush, strokeWidth, NULL);
}

static PB_DWRITE_TEXT_ALIGNMENT ToDwAlign(D2dAlign align)
{
    if (align == D2D_ALIGN_CENTER) return PB_DWRITE_TEXT_ALIGNMENT_CENTER;
    if (align == D2D_ALIGN_RIGHT)  return PB_DWRITE_TEXT_ALIGNMENT_TRAILING;
    return PB_DWRITE_TEXT_ALIGNMENT_LEADING;
}

void D2d_DrawText(D2dCanvas *c, D2dFontSlot font, const wchar_t *text,
                  float x, float y, float w, float h, uint32_t argb,
                  D2dAlign align, BOOL tabularFigures)
{
    if (!c || !c->rt || !c->brush || !text || !text[0]) return;
    if (font < 0 || font >= D2D_FONT_COUNT || !g_fonts[font] || !g_dwFactory) return;

    UINT32 len = (UINT32)wcslen(text);
    PB_ID2D1RenderTarget *rt = (PB_ID2D1RenderTarget *)c->rt;
    SetBrushColor(c, argb);

    PB_IDWriteTextLayout *layout = NULL;
    HRESULT hr = g_dwFactory->lpVtbl->CreateTextLayout(
        g_dwFactory, text, len, g_fonts[font], w > 1.0f ? w : 1.0f, h > 1.0f ? h : 1.0f, &layout);
    if (SUCCEEDED(hr) && layout) {
        layout->lpVtbl->SetTextAlignment(layout, ToDwAlign(align));
        layout->lpVtbl->SetParagraphAlignment(layout, PB_DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        layout->lpVtbl->SetWordWrapping(layout, PB_DWRITE_WORD_WRAPPING_NO_WRAP);
        if (tabularFigures && g_tabularTypo) {
            PB_DWRITE_TEXT_RANGE range = { 0, len };
            layout->lpVtbl->SetTypography(layout, g_tabularTypo, range);
        }
        D2D1_POINT_2F origin = { x, y };
        rt->lpVtbl->DrawTextLayout(rt, origin, layout, c->brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        SAFE_COM_RELEASE(layout);
    } else {
        D2D1_RECT_F rect = { x, y, x + w, y + h };
        g_fonts[font]->lpVtbl->SetTextAlignment(g_fonts[font], ToDwAlign(align));
        rt->lpVtbl->DrawText(rt, text, len, g_fonts[font], &rect, c->brush,
                             D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
    }
}

float D2d_MeasureTextWidth(D2dFontSlot font, const wchar_t *text, BOOL tabularFigures)
{
    if (!text || !text[0] || !g_dwFactory) return 0.0f;
    if (font < 0 || font >= D2D_FONT_COUNT || !g_fonts[font]) return 0.0f;

    UINT32 len = (UINT32)wcslen(text);
    PB_IDWriteTextLayout *layout = NULL;
    HRESULT hr = g_dwFactory->lpVtbl->CreateTextLayout(
        g_dwFactory, text, len, g_fonts[font], 2000.0f, 64.0f, &layout);
    if (FAILED(hr) || !layout) return 0.0f;

    if (tabularFigures && g_tabularTypo) {
        PB_DWRITE_TEXT_RANGE range = { 0, len };
        layout->lpVtbl->SetTypography(layout, g_tabularTypo, range);
    }
    PB_DWRITE_TEXT_METRICS m;
    ZeroMemory(&m, sizeof(m));
    float width = 0.0f;
    if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &m))) {
        width = m.widthIncludingTrailingWhitespace;
    }
    SAFE_COM_RELEASE(layout);
    return width;
}

D2dFontSlot D2d_Mono11ForWeight(int weight)
{
    if (weight <= 300) return D2D_FONT_MONO_11_LIGHT;
    if (weight <= 400) return D2D_FONT_MONO_11;
    if (weight <= 500) return D2D_FONT_MONO_11_MEDIUM;
    if (weight <= 600) return D2D_FONT_MONO_11_SEMIBOLD;
    return D2D_FONT_MONO_11_BOLD;
}
