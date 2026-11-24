#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "window_osd.h"
#include "d2d_renderer.h"
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comdlg32.lib")

#define OSD_WND_CLASS     L"ProxyBridgeModernOsdWnd"
#define OSD_SET_CLASS     L"ProxyBridgeModernOsdSettingsWnd"
#define OSD_EX_STYLES     (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW)

#define IDC_OSD_CMB_MODE      4100
#define IDC_OSD_CMB_CAP_TOP   4101
#define IDC_OSD_CMB_COMPACT   4102
#define IDC_OSD_CMB_ANCHOR    4103
#define IDC_OSD_CMB_WEIGHT    4104
#define IDC_OSD_SLD_OPACITY   4105
#define IDC_OSD_EDT_OPACITY   4106
#define IDC_OSD_SLD_SIZE      4107
#define IDC_OSD_EDT_SIZE      4108
#define IDC_OSD_EDT_OFFX      4109
#define IDC_OSD_EDT_OFFY      4110
#define IDC_OSD_CMB_BG_MODE   4111
#define IDC_OSD_SWATCH_BG     4112
#define IDC_OSD_BTN_BG_PICK   4113
#define IDC_OSD_CMB_TXT_MODE  4114
#define IDC_OSD_SWATCH_TXT    4115
#define IDC_OSD_BTN_TXT_PICK  4116
#define IDC_OSD_BTN_RESET     4117
#define IDC_OSD_BTN_CLOSE     4118

static HINSTANCE                   g_osdInst = NULL;
static BOOL                        g_osdClassReg = FALSE;
static HWND                        g_osdHwnd = NULL;
static D2dLayeredSurface           g_osdSurf;
static BOOL                        g_osdVisible = FALSE;
static HWND                        g_targetHwnd = NULL;
static int                         g_anchor = MODERN_OSD_ANCHOR_OUTSIDE_TOP_LEFT;
static int                         g_offsetX = MODERN_OSD_DEFAULT_OFFSET_X;
static int                         g_offsetY = MODERN_OSD_DEFAULT_OFFSET_Y;
static int                         g_lastX = 0;
static int                         g_lastY = 0;
static int                         g_lastW = 220;
static int                         g_lastH = MODERN_OSD_HEIGHT;

static HWINEVENTHOOK                  g_hookFg = NULL;
static HWINEVENTHOOK                  g_hookMin = NULL;
static HWINEVENTHOOK                  g_hookLoc = NULL;
static WindowOsdForegroundCallback    g_fgCb = NULL;
static void                          *g_fgUser = NULL;

// Live-preview settings window state
static BOOL                           g_setClassReg = FALSE;
static HWND                           g_setHwnd = NULL;
static HFONT                          g_setFont = NULL;
static HBRUSH                         g_setBgBrush = NULL;
static HBRUSH                         g_setEditBrush = NULL;
static ModernUiConfig                *g_setCfgPtr = NULL;
static WindowOsdConfigChangedCallback g_setChangedCb = NULL;
static void                          *g_setChangedUser = NULL;
static BOOL                           g_setSyncing = FALSE;
static BOOL                           g_previewMode = FALSE;

static BOOL GetVisibleFrameRect(HWND target, RECT *outRect)
{
    if (SUCCEEDED(DwmGetWindowAttribute(target, DWMWA_EXTENDED_FRAME_BOUNDS, outRect, sizeof(*outRect)))) {
        if ((outRect->right - outRect->left) > 0 && (outRect->bottom - outRect->top) > 0)
            return TRUE;
    }
    return GetWindowRect(target, outRect);
}

static BOOL ComputeTargetAnchor(HWND target, int anchor, int offX, int offY, int pillW, int pillH, int *outX, int *outY)
{
    if (!target || !IsWindow(target) || !IsWindowVisible(target) || IsIconic(target))
        return FALSE;

    RECT frame;
    if (!GetVisibleFrameRect(target, &frame)) return FALSE;
    if ((frame.right - frame.left) < 40 || (frame.bottom - frame.top) < 24)
        return FALSE;

    RECT clientRc = { 0, 0, 0, 0 };
    POINT clientOrigin = { frame.left, frame.top };
    if (GetClientRect(target, &clientRc) && ClientToScreen(target, &clientOrigin)) {
        // valid client geometry
    } else {
        clientRc.right  = frame.right - frame.left;
        clientRc.bottom = frame.bottom - frame.top;
    }

    int x = frame.left;
    int y = frame.top;

    switch (anchor) {
    case MODERN_OSD_ANCHOR_OUTSIDE_TOP_RIGHT:
        x = frame.right - pillW - offX;
        y = frame.top - pillH - offY;
        break;
    case MODERN_OSD_ANCHOR_INSIDE_TOP_LEFT:
        x = clientOrigin.x + offX;
        y = clientOrigin.y + offY;
        break;
    case MODERN_OSD_ANCHOR_INSIDE_TOP_RIGHT:
        x = clientOrigin.x + (clientRc.right - clientRc.left) - pillW - offX;
        y = clientOrigin.y + offY;
        break;
    case MODERN_OSD_ANCHOR_INSIDE_BOTTOM_LEFT:
        x = clientOrigin.x + offX;
        y = clientOrigin.y + (clientRc.bottom - clientRc.top) - pillH - offY;
        break;
    case MODERN_OSD_ANCHOR_INSIDE_BOTTOM_RIGHT:
        x = clientOrigin.x + (clientRc.right - clientRc.left) - pillW - offX;
        y = clientOrigin.y + (clientRc.bottom - clientRc.top) - pillH - offY;
        break;
    case MODERN_OSD_ANCHOR_OUTSIDE_TOP_LEFT:
    default:
        x = frame.left + offX;
        y = frame.top - pillH - offY;
        break;
    }

    // Clamp cleanly inside target window's monitor so maximized or top-edge windows
    // never push the capsule off-screen.
    HMONITOR mon = MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi)) {
        const RECT *mr = &mi.rcMonitor;
        if (x < mr->left + 2)          x = mr->left + 2;
        if (x + pillW > mr->right - 2) x = mr->right - pillW - 2;
        if (y < mr->top + 2)           y = mr->top + 2;
        if (y + pillH > mr->bottom - 2) y = mr->bottom - pillH - 2;
    }

    *outX = x;
    *outY = y;
    return TRUE;
}

static LRESULT CALLBACK OsdWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void CALLBACK OsdWinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                     LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    (void)hook; (void)idChild; (void)dwEventThread; (void)dwmsEventTime;

    if (event == EVENT_SYSTEM_FOREGROUND) {
        if (g_osdVisible && hwnd != g_targetHwnd) {
            WindowOsd_Hide();
        }
        if (g_fgCb) {
            g_fgCb(hwnd, g_fgUser);
        }
        return;
    }

    if (event == EVENT_SYSTEM_MINIMIZESTART) {
        if (hwnd == g_targetHwnd) {
            WindowOsd_Hide();
        }
        return;
    }

    if (event == EVENT_OBJECT_LOCATIONCHANGE) {
        if (idObject != OBJID_WINDOW || hwnd != g_targetHwnd || !g_osdVisible || !g_osdHwnd)
            return;
        int x = 0, y = 0;
        if (!ComputeTargetAnchor(g_targetHwnd, g_anchor, g_offsetX, g_offsetY, g_lastW, g_lastH, &x, &y) ||
            GetForegroundWindow() != g_targetHwnd) {
            WindowOsd_Hide();
            return;
        }
        if (x != g_lastX || y != g_lastY) {
            g_lastX = x;
            g_lastY = y;
            SetWindowPos(g_osdHwnd, HWND_TOPMOST, x, y, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
    }
}

BOOL WindowOsd_Init(HINSTANCE hInst, WindowOsdForegroundCallback fgCb, void *fgUser)
{
    if (g_osdHwnd != NULL) return TRUE;
    g_osdInst = hInst;
    g_fgCb    = fgCb;
    g_fgUser  = fgUser;

    if (!D2d_IsReady() && !D2d_Init()) return FALSE;

    if (!g_osdClassReg) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = OsdWndProc;
        wc.hInstance     = hInst;
        wc.lpszClassName = OSD_WND_CLASS;
        if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            g_osdClassReg = TRUE;
        } else {
            return FALSE;
        }
    }

    g_osdHwnd = CreateWindowExW(
        OSD_EX_STYLES,
        OSD_WND_CLASS,
        L"",
        WS_POPUP,
        0, 0, 220, MODERN_OSD_HEIGHT,
        NULL, NULL, hInst, NULL);
    if (!g_osdHwnd) return FALSE;

    if (!D2dLayered_Create(&g_osdSurf, 220, MODERN_OSD_HEIGHT)) {
        DestroyWindow(g_osdHwnd);
        g_osdHwnd = NULL;
        return FALSE;
    }

    g_hookFg = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        NULL, OsdWinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    g_hookMin = SetWinEventHook(
        EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND,
        NULL, OsdWinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    g_hookLoc = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
        NULL, OsdWinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    return TRUE;
}

void WindowOsd_Shutdown(void)
{
    WindowOsd_CloseSettingsWindow();
    WindowOsd_Hide();

    if (g_hookLoc) { UnhookWinEvent(g_hookLoc); g_hookLoc = NULL; }
    if (g_hookMin) { UnhookWinEvent(g_hookMin); g_hookMin = NULL; }
    if (g_hookFg)  { UnhookWinEvent(g_hookFg);  g_hookFg  = NULL; }

    D2dLayered_Destroy(&g_osdSurf);

    if (g_osdHwnd) {
        DestroyWindow(g_osdHwnd);
        g_osdHwnd = NULL;
    }
    if (g_osdClassReg && g_osdInst) {
        UnregisterClassW(OSD_WND_CLASS, g_osdInst);
        g_osdClassReg = FALSE;
    }
    if (g_setClassReg && g_osdInst) {
        UnregisterClassW(OSD_SET_CLASS, g_osdInst);
        g_setClassReg = FALSE;
    }
    g_targetHwnd = NULL;
    g_fgCb       = NULL;
    g_fgUser     = NULL;
}

void WindowOsd_Hide(void)
{
    if (g_previewMode && g_setHwnd && IsWindow(g_setHwnd) && GetForegroundWindow() == g_setHwnd) {
        return; // keep live preview visible while tuning inside the settings window
    }
    if (g_osdHwnd && g_osdVisible) {
        ShowWindow(g_osdHwnd, SW_HIDE);
    }
    g_osdVisible = FALSE;
    g_targetHwnd = NULL;
}

static void RenderOsdForWindow(HWND targetHwnd, const ModernAppTraffic *app, BOOL proxyEnabled,
                               const ModernUiConfig *cfg, BOOL allowOwnProcess)
{
    if (!g_osdHwnd || !app || !cfg) {
        if (!allowOwnProcess) WindowOsd_Hide();
        return;
    }

    DWORD targetPid = 0;
    if (targetHwnd) GetWindowThreadProcessId(targetHwnd, &targetPid);
    if (!targetHwnd || (!allowOwnProcess && targetPid == GetCurrentProcessId()) ||
        GetForegroundWindow() != targetHwnd) {
        if (!allowOwnProcess) WindowOsd_Hide();
        return;
    }

    ModernUiConfig safeCfg = *cfg;
    ModernUiConfig_Sanitize(&safeCfg);

    g_anchor  = safeCfg.osdAnchor;
    g_offsetX = safeCfg.osdOffsetX;
    g_offsetY = safeCfg.osdOffsetY;

    // Format telemetry segments with tabular figures
    wchar_t rttStr[24], dnStr[32], upStr[32], sumStr[32];
    wchar_t dnSeg[40], upSeg[40], sumSeg[40];

    if (!proxyEnabled) {
        lstrcpynW(rttStr, L"PAUSED", ARRAYSIZE(rttStr));
    } else if (app->rttMs > 0) {
        _snwprintf_s(rttStr, ARRAYSIZE(rttStr), _TRUNCATE, L"%ums", app->rttMs);
    } else {
        lstrcpynW(rttStr, L"--ms", ARRAYSIZE(rttStr));
    }

    ModernFormatSpeedW(app->rxBytesPerSec, safeCfg.osdCompactMode, dnStr, ARRAYSIZE(dnStr));
    ModernFormatSpeedW(app->txBytesPerSec, safeCfg.osdCompactMode, upStr, ARRAYSIZE(upStr));
    ModernFormatBytesW(app->rxBytesTotal + app->txBytesTotal, sumStr, ARRAYSIZE(sumStr));

    if (safeCfg.osdCompactMode) {
        _snwprintf_s(dnSeg, ARRAYSIZE(dnSeg), _TRUNCATE, L"\x2193%s", dnStr);
        _snwprintf_s(upSeg, ARRAYSIZE(upSeg), _TRUNCATE, L"\x2191%s", upStr);
        sumSeg[0] = L'\0';
    } else {
        _snwprintf_s(dnSeg,  ARRAYSIZE(dnSeg),  _TRUNCATE, L"\x2193 %s", dnStr);
        _snwprintf_s(upSeg,  ARRAYSIZE(upSeg),  _TRUNCATE, L"\x2191 %s", upStr);
        _snwprintf_s(sumSeg, ARRAYSIZE(sumSeg), _TRUNCATE, L"\x03A3 %s", sumStr);
    }

    // Measure stable segment widths in DIPs (minimum widths avoid pill horizontal jitter)
    const D2dFontSlot font = D2d_Mono11ForWeight(safeCfg.osdFontWeight);
    float wRtt = D2d_MeasureTextWidth(font, rttStr, TRUE);
    float wDn  = D2d_MeasureTextWidth(font, dnSeg, TRUE);
    float wUp  = D2d_MeasureTextWidth(font, upSeg, TRUE);
    float wSum = safeCfg.osdCompactMode ? 0.0f : D2d_MeasureTextWidth(font, sumSeg, TRUE);
    float wSep = safeCfg.osdCompactMode ? 0.0f : D2d_MeasureTextWidth(font, L"|", TRUE);

    if (wRtt < 34.0f) wRtt = 34.0f;
    if (wDn  < (safeCfg.osdCompactMode ? 58.0f : 72.0f)) wDn = safeCfg.osdCompactMode ? 58.0f : 72.0f;
    if (wUp  < (safeCfg.osdCompactMode ? 58.0f : 72.0f)) wUp = safeCfg.osdCompactMode ? 58.0f : 72.0f;
    if (!safeCfg.osdCompactMode && wSum < 62.0f) wSum = 62.0f;

    float padL = 9.0f;
    float dotD = 7.0f;
    float gap  = safeCfg.osdCompactMode ? 7.0f : 8.0f;
    float padR = 10.0f;

    float totalW = padL + dotD + 6.0f + wRtt;
    if (safeCfg.osdCompactMode) {
        totalW += gap + wDn + gap + wUp + padR;
    } else {
        totalW += gap + wSep + gap + wDn + gap + wUp + gap + wSep + gap + wSum + padR;
    }

    float baseW = ceilf(totalW);
    if (baseW < 140.0f) baseW = 140.0f;
    if (baseW > 520.0f) baseW = 520.0f;
    float baseH = (float)MODERN_OSD_HEIGHT;

    float scale = (float)safeCfg.osdSizePx / baseH;
    int pillW = (int)ceilf(baseW * scale);
    int pillH = safeCfg.osdSizePx;
    if (pillW < 80) pillW = 80;
    if (pillH < MODERN_OSD_MIN_SIZE_PX) pillH = MODERN_OSD_MIN_SIZE_PX;

    int anchorX = 0, anchorY = 0;
    if (!ComputeTargetAnchor(targetHwnd, g_anchor, g_offsetX, g_offsetY, pillW, pillH, &anchorX, &anchorY)) {
        WindowOsd_Hide();
        return;
    }

    if (!D2dLayered_Resize(&g_osdSurf, pillW, pillH)) return;

    BOOL dark = D2d_ResolveDarkMode(safeCfg.themeMode);
    uint32_t bgArgb     = safeCfg.osdUseCustomBg
                            ? (0xD9000000u | (safeCfg.osdBgColor & 0x00FFFFFFu))
                            : (dark ? MODERN_COLOR_BG_DARK_ARGB : MODERN_COLOR_BG_LIGHT_ARGB);
    uint32_t borderArgb = dark ? MODERN_COLOR_BORDER_DARK_ARGB : MODERN_COLOR_BORDER_LIGHT_ARGB;
    uint32_t textArgb   = safeCfg.osdUseCustomText
                            ? (0xFF000000u | (safeCfg.osdTextColor & 0x00FFFFFFu))
                            : (dark ? MODERN_COLOR_TEXT_DARK_ARGB : MODERN_COLOR_TEXT_LIGHT_ARGB);
    uint32_t mutedArgb  = safeCfg.osdUseCustomText
                            ? (0xAA000000u | (safeCfg.osdTextColor & 0x00FFFFFFu))
                            : (dark ? MODERN_COLOR_MUTED_DARK_ARGB : MODERN_COLOR_MUTED_LIGHT_ARGB);
    uint32_t dlArgb     = safeCfg.osdUseCustomText ? textArgb : MODERN_COLOR_DL_ARGB;
    uint32_t ulArgb     = safeCfg.osdUseCustomText ? textArgb : MODERN_COLOR_UL_ARGB;
    uint32_t dotArgb    = ModernStatusColorArgb(proxyEnabled, app->rttMs);

    if (!D2dLayered_BeginDraw(&g_osdSurf)) return;
    D2d_SetScale(&g_osdSurf.canvas, scale);

    float fw = baseW;
    float fh = baseH;
    float rad = fh * 0.5f;

    // Pill background + 1px crisp inner stroke (omitted when user selects completely transparent background)
    if (!safeCfg.osdBgTransparent) {
        D2d_FillRoundedRect(&g_osdSurf.canvas, 0.5f, 0.5f, fw - 1.0f, fh - 1.0f, rad, rad, bgArgb);
        D2d_DrawRoundedRect(&g_osdSurf.canvas, 0.5f, 0.5f, fw - 1.0f, fh - 1.0f, rad, rad, 1.0f, borderArgb);
    }

    // Status dot
    float cx = padL + dotD * 0.5f;
    float cy = fh * 0.5f;
    D2d_FillCircle(&g_osdSurf.canvas, cx, cy, dotD * 0.5f, dotArgb);

    float x = padL + dotD + 6.0f;
    D2d_DrawText(&g_osdSurf.canvas, font, rttStr, x, 0.0f, wRtt + 2.0f, fh, textArgb, D2D_ALIGN_LEFT, TRUE);
    x += wRtt + gap;

    if (!safeCfg.osdCompactMode) {
        D2d_DrawText(&g_osdSurf.canvas, font, L"|", x, 0.0f, wSep + 2.0f, fh, mutedArgb, D2D_ALIGN_CENTER, FALSE);
        x += wSep + gap;
    }

    D2d_DrawText(&g_osdSurf.canvas, font, dnSeg, x, 0.0f, wDn + 2.0f, fh, dlArgb, D2D_ALIGN_LEFT, TRUE);
    x += wDn + gap;

    D2d_DrawText(&g_osdSurf.canvas, font, upSeg, x, 0.0f, wUp + 2.0f, fh, ulArgb, D2D_ALIGN_LEFT, TRUE);
    x += wUp + gap;

    if (!safeCfg.osdCompactMode) {
        D2d_DrawText(&g_osdSurf.canvas, font, L"|", x, 0.0f, wSep + 2.0f, fh, mutedArgb, D2D_ALIGN_CENTER, FALSE);
        x += wSep + gap;
        D2d_DrawText(&g_osdSurf.canvas, font, sumSeg, x, 0.0f, wSum + 4.0f, fh, textArgb, D2D_ALIGN_LEFT, TRUE);
    }

    BYTE masterAlpha = (BYTE)((safeCfg.osdOpacity * 255 + 50) / 100);
    if (D2dLayered_EndDrawAndPresent(&g_osdSurf, g_osdHwnd, anchorX, anchorY, masterAlpha)) {
        g_targetHwnd = targetHwnd;
        g_lastX = anchorX;
        g_lastY = anchorY;
        g_lastW = pillW;
        g_lastH = pillH;

        if (!g_osdVisible) {
            ShowWindow(g_osdHwnd, SW_SHOWNOACTIVATE);
            g_osdVisible = TRUE;
        }
        SetWindowPos(g_osdHwnd, HWND_TOPMOST, anchorX, anchorY, pillW, pillH,
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
}

void WindowOsd_Update(HWND targetHwnd, const ModernAppTraffic *app, BOOL proxyEnabled, const ModernUiConfig *cfg)
{
    if (g_previewMode && g_setHwnd && IsWindow(g_setHwnd) && GetForegroundWindow() == g_setHwnd) {
        return;
    }
    RenderOsdForWindow(targetHwnd, app, proxyEnabled, cfg, FALSE);
}

// ============================================================================
// Modeless In-App Live-Preview OSD Settings Window
// ============================================================================

static void RefreshPreviewOnSettings(void)
{
    if (!g_setHwnd || !IsWindow(g_setHwnd) || !g_setCfgPtr) return;
    if (!g_setCfgPtr->osdEnabled) {
        g_previewMode = FALSE;
        WindowOsd_Hide();
        return;
    }
    ModernAppTraffic sample;
    ZeroMemory(&sample, sizeof(sample));
    sample.hostPid       = GetCurrentProcessId();
    sample.rxBytesPerSec = 2516582ULL;   // ~2.4 MB/s
    sample.txBytesPerSec = 122880ULL;    // 120.0 KB/s
    sample.rxBytesTotal  = 438304768ULL; // 418.0 MB
    sample.rttMs         = 32;
    g_previewMode = TRUE;
    RenderOsdForWindow(g_setHwnd, &sample, TRUE, g_setCfgPtr, TRUE);
}

static COLORREF Rgb24ToColorRef(uint32_t rgb24)
{
    return RGB((rgb24 >> 16) & 0xFFu, (rgb24 >> 8) & 0xFFu, rgb24 & 0xFFu);
}

static uint32_t ColorRefToRgb24(COLORREF cr)
{
    return ((uint32_t)GetRValue(cr) << 16) | ((uint32_t)GetGValue(cr) << 8) | (uint32_t)GetBValue(cr);
}

static int ModeIndexFromCfg(const ModernUiConfig *c)
{
    int mask = (c->capsuleEnabled ? 1 : 0) | (c->osdEnabled ? 2 : 0) | (c->rtssEnabled ? 4 : 0);
    // Ordered intuitively for the user:
    // 0: None (0), 1: Capsule (1), 2: OSD (2), 3: RTSS (4),
    // 4: Capsule+OSD (3), 5: OSD+RTSS (6), 6: Capsule+RTSS (5), 7: All 3 (7)
    static const int kMaskToIdx[8] = { 0, 1, 2, 4, 3, 6, 5, 7 };
    return kMaskToIdx[mask & 7];
}

static void ApplyModeIndexToCfg(int idx, ModernUiConfig *c)
{
    static const int kIdxToMask[8] = { 0, 1, 2, 4, 3, 6, 5, 7 };
    if (idx < 0 || idx > 7) idx = 7;
    int mask = kIdxToMask[idx];
    c->capsuleEnabled = (mask & 1) != 0;
    c->osdEnabled     = (mask & 2) != 0;
    c->rtssEnabled    = (mask & 4) != 0;
}

static int WeightToIndex(int w)
{
    if (w <= 300) return 0;
    if (w <= 400) return 1;
    if (w <= 500) return 2;
    if (w <= 600) return 3;
    return 4;
}

static int IndexToWeight(int idx)
{
    static const int kWeights[5] = { 300, 400, 500, 600, 700 };
    if (idx < 0 || idx > 4) return 400;
    return kWeights[idx];
}

static void UpdateOsdControlsEnabledState(HWND hwnd)
{
    if (!g_setCfgPtr) return;
    BOOL capOn   = g_setCfgPtr->capsuleEnabled ? TRUE : FALSE;
    BOOL osdOn   = g_setCfgPtr->osdEnabled ? TRUE : FALSE;
    BOOL bgCust  = (!g_setCfgPtr->osdBgTransparent && g_setCfgPtr->osdUseCustomBg);
    BOOL txtCust = g_setCfgPtr->osdUseCustomText;

    EnableWindow(GetDlgItem(hwnd, IDC_OSD_CMB_CAP_TOP), capOn);

    static const int kOsdIds[] = {
        IDC_OSD_CMB_COMPACT, IDC_OSD_CMB_ANCHOR, IDC_OSD_CMB_WEIGHT,
        IDC_OSD_SLD_OPACITY, IDC_OSD_EDT_OPACITY,
        IDC_OSD_SLD_SIZE,    IDC_OSD_EDT_SIZE,
        IDC_OSD_EDT_OFFX,    IDC_OSD_EDT_OFFY,
        IDC_OSD_CMB_BG_MODE, IDC_OSD_SWATCH_BG,
        IDC_OSD_CMB_TXT_MODE, IDC_OSD_SWATCH_TXT
    };
    for (size_t i = 0; i < ARRAYSIZE(kOsdIds); i++) {
        EnableWindow(GetDlgItem(hwnd, kOsdIds[i]), osdOn);
    }
    EnableWindow(GetDlgItem(hwnd, IDC_OSD_BTN_BG_PICK),  osdOn && bgCust);
    EnableWindow(GetDlgItem(hwnd, IDC_OSD_BTN_TXT_PICK), osdOn && txtCust);

    InvalidateRect(GetDlgItem(hwnd, IDC_OSD_SWATCH_BG), NULL, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_OSD_SWATCH_TXT), NULL, TRUE);
}

static void SyncControlsFromConfig(HWND hwnd)
{
    if (!g_setCfgPtr) return;
    ModernUiConfig_Sanitize(g_setCfgPtr);
    g_setSyncing = TRUE;

    SendDlgItemMessageW(hwnd, IDC_OSD_CMB_MODE,    CB_SETCURSEL, (WPARAM)ModeIndexFromCfg(g_setCfgPtr), 0);
    SendDlgItemMessageW(hwnd, IDC_OSD_CMB_CAP_TOP, CB_SETCURSEL, (WPARAM)(g_setCfgPtr->capsuleTopmost ? 0 : 1), 0);
    SendDlgItemMessageW(hwnd, IDC_OSD_CMB_COMPACT, CB_SETCURSEL, (WPARAM)(g_setCfgPtr->osdCompactMode ? 1 : 0), 0);
    SendDlgItemMessageW(hwnd, IDC_OSD_CMB_ANCHOR,  CB_SETCURSEL, (WPARAM)g_setCfgPtr->osdAnchor, 0);
    SendDlgItemMessageW(hwnd, IDC_OSD_CMB_WEIGHT,  CB_SETCURSEL, (WPARAM)WeightToIndex(g_setCfgPtr->osdFontWeight), 0);
    SendDlgItemMessageW(hwnd, IDC_OSD_SLD_OPACITY, TBM_SETPOS, TRUE, (LPARAM)g_setCfgPtr->osdOpacity);
    SendDlgItemMessageW(hwnd, IDC_OSD_SLD_SIZE,    TBM_SETPOS, TRUE, (LPARAM)g_setCfgPtr->osdSizePx);

    int bgMode = g_setCfgPtr->osdBgTransparent ? 2 : (g_setCfgPtr->osdUseCustomBg ? 1 : 0);
    int txtMode = g_setCfgPtr->osdUseCustomText ? 1 : 0;
    SendDlgItemMessageW(hwnd, IDC_OSD_CMB_BG_MODE,  CB_SETCURSEL, (WPARAM)bgMode, 0);
    SendDlgItemMessageW(hwnd, IDC_OSD_CMB_TXT_MODE, CB_SETCURSEL, (WPARAM)txtMode, 0);

    wchar_t buf[32];
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdOpacity);
    SetDlgItemTextW(hwnd, IDC_OSD_EDT_OPACITY, buf);
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdSizePx);
    SetDlgItemTextW(hwnd, IDC_OSD_EDT_SIZE, buf);
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdOffsetX);
    SetDlgItemTextW(hwnd, IDC_OSD_EDT_OFFX, buf);
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdOffsetY);
    SetDlgItemTextW(hwnd, IDC_OSD_EDT_OFFY, buf);

    UpdateOsdControlsEnabledState(hwnd);
    g_setSyncing = FALSE;
}

static void ApplyAndPersistLive(void)
{
    if (!g_setCfgPtr) return;
    ModernUiConfig_Sanitize(g_setCfgPtr);
    if (g_setChangedCb) {
        g_setChangedCb(g_setCfgPtr, g_setChangedUser);
    }
    RefreshPreviewOnSettings();
}

static void ReadConfigAndApplyLive(HWND hwnd, BOOL fromSliders, BOOL clampInputs)
{
    if (g_setSyncing || !g_setCfgPtr) return;

    int modeIdx = (int)SendDlgItemMessageW(hwnd, IDC_OSD_CMB_MODE, CB_GETCURSEL, 0, 0);
    ApplyModeIndexToCfg(modeIdx, g_setCfgPtr);

    g_setCfgPtr->capsuleTopmost = ((int)SendDlgItemMessageW(hwnd, IDC_OSD_CMB_CAP_TOP, CB_GETCURSEL, 0, 0) == 0);
    g_setCfgPtr->osdCompactMode = ((int)SendDlgItemMessageW(hwnd, IDC_OSD_CMB_COMPACT, CB_GETCURSEL, 0, 0) == 1);

    int anchorSel = (int)SendDlgItemMessageW(hwnd, IDC_OSD_CMB_ANCHOR, CB_GETCURSEL, 0, 0);
    if (anchorSel >= 0 && anchorSel < MODERN_OSD_ANCHOR_COUNT) g_setCfgPtr->osdAnchor = anchorSel;

    int weightSel = (int)SendDlgItemMessageW(hwnd, IDC_OSD_CMB_WEIGHT, CB_GETCURSEL, 0, 0);
    g_setCfgPtr->osdFontWeight = IndexToWeight(weightSel);

    int bgMode = (int)SendDlgItemMessageW(hwnd, IDC_OSD_CMB_BG_MODE, CB_GETCURSEL, 0, 0);
    g_setCfgPtr->osdBgTransparent = (bgMode == 2);
    g_setCfgPtr->osdUseCustomBg   = (bgMode == 1);

    int txtMode = (int)SendDlgItemMessageW(hwnd, IDC_OSD_CMB_TXT_MODE, CB_GETCURSEL, 0, 0);
    g_setCfgPtr->osdUseCustomText = (txtMode == 1);

    wchar_t buf[32];
    if (fromSliders) {
        g_setCfgPtr->osdOpacity = (int)SendDlgItemMessageW(hwnd, IDC_OSD_SLD_OPACITY, TBM_GETPOS, 0, 0);
        g_setCfgPtr->osdSizePx  = (int)SendDlgItemMessageW(hwnd, IDC_OSD_SLD_SIZE,    TBM_GETPOS, 0, 0);
        g_setSyncing = TRUE;
        _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdOpacity);
        SetDlgItemTextW(hwnd, IDC_OSD_EDT_OPACITY, buf);
        _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdSizePx);
        SetDlgItemTextW(hwnd, IDC_OSD_EDT_SIZE, buf);
        g_setSyncing = FALSE;
    } else {
        GetDlgItemTextW(hwnd, IDC_OSD_EDT_OPACITY, buf, ARRAYSIZE(buf));
        if (buf[0] != L'\0') {
            int v = _wtoi(buf);
            if (v >= MODERN_OSD_MIN_OPACITY && v <= MODERN_OSD_MAX_OPACITY) {
                g_setCfgPtr->osdOpacity = v;
                SendDlgItemMessageW(hwnd, IDC_OSD_SLD_OPACITY, TBM_SETPOS, TRUE, (LPARAM)v);
            } else if (clampInputs) {
                g_setCfgPtr->osdOpacity = v;
                ModernUiConfig_Sanitize(g_setCfgPtr);
                g_setSyncing = TRUE;
                SendDlgItemMessageW(hwnd, IDC_OSD_SLD_OPACITY, TBM_SETPOS, TRUE, (LPARAM)g_setCfgPtr->osdOpacity);
                _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdOpacity);
                SetDlgItemTextW(hwnd, IDC_OSD_EDT_OPACITY, buf);
                g_setSyncing = FALSE;
            }
        }
        GetDlgItemTextW(hwnd, IDC_OSD_EDT_SIZE, buf, ARRAYSIZE(buf));
        if (buf[0] != L'\0') {
            int v = _wtoi(buf);
            if (v >= MODERN_OSD_MIN_SIZE_PX && v <= MODERN_OSD_MAX_SIZE_PX) {
                g_setCfgPtr->osdSizePx = v;
                SendDlgItemMessageW(hwnd, IDC_OSD_SLD_SIZE, TBM_SETPOS, TRUE, (LPARAM)v);
            } else if (clampInputs) {
                g_setCfgPtr->osdSizePx = v;
                ModernUiConfig_Sanitize(g_setCfgPtr);
                g_setSyncing = TRUE;
                SendDlgItemMessageW(hwnd, IDC_OSD_SLD_SIZE, TBM_SETPOS, TRUE, (LPARAM)g_setCfgPtr->osdSizePx);
                _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdSizePx);
                SetDlgItemTextW(hwnd, IDC_OSD_EDT_SIZE, buf);
                g_setSyncing = FALSE;
            }
        }
    }

    GetDlgItemTextW(hwnd, IDC_OSD_EDT_OFFX, buf, ARRAYSIZE(buf));
    if (buf[0] != L'\0' && !(buf[0] == L'-' && buf[1] == L'\0')) g_setCfgPtr->osdOffsetX = _wtoi(buf);
    GetDlgItemTextW(hwnd, IDC_OSD_EDT_OFFY, buf, ARRAYSIZE(buf));
    if (buf[0] != L'\0' && !(buf[0] == L'-' && buf[1] == L'\0')) g_setCfgPtr->osdOffsetY = _wtoi(buf);

    UpdateOsdControlsEnabledState(hwnd);
    ApplyAndPersistLive();
}

static BOOL PickColorDialog(HWND owner, uint32_t *inOutRgb24)
{
    static COLORREF customColors[16] = {
        RGB(0x18,0x18,0x1B), RGB(0xF4,0xF4,0xF5), RGB(0x10,0xB9,0x81), RGB(0x38,0xBD,0xF8),
        RGB(0xA7,0x8B,0xFA), RGB(0xF5,0x9E,0x0B), RGB(0xEF,0x44,0x44), RGB(0x00,0xFF,0x88),
        RGB(0x00,0x00,0x00), RGB(0x22,0x22,0x27), RGB(0x33,0x41,0x55), RGB(0xFF,0xFF,0xFF),
        RGB(0xFF,0xD7,0x00), RGB(0x00,0xE5,0xFF), RGB(0xFF,0x6B,0x6B), RGB(0xC0,0x84,0xFC)
    };
    CHOOSECOLORW cc;
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = owner;
    cc.lpCustColors = customColors;
    cc.rgbResult    = Rgb24ToColorRef(*inOutRgb24);
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) return FALSE;
    *inOutRgb24 = ColorRefToRgb24(cc.rgbResult);
    return TRUE;
}

static BOOL IsSetDark(void)
{
    return D2d_ResolveDarkMode(g_setCfgPtr ? g_setCfgPtr->themeMode : MODERN_THEME_SYSTEM);
}

static HWND MakeCtrl(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style,
                     DWORD exStyle, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, parent, (HMENU)(UINT_PTR)id, g_osdInst, NULL);
    if (c) {
        if (g_setFont) SendMessageW(c, WM_SETFONT, (WPARAM)g_setFont, TRUE);
        BOOL dark = IsSetDark();
        if (_wcsicmp(cls, L"BUTTON") == 0) {
            DWORD bt = style & BS_TYPEMASK;
            if (bt != BS_OWNERDRAW) {
                SetWindowTheme(c, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
            }
        } else if (_wcsicmp(cls, L"EDIT") == 0 || _wcsicmp(cls, WC_COMBOBOXW) == 0) {
            SetWindowTheme(c, dark ? L"DarkMode_CFD" : L"CFD", NULL);
        }
    }
    return c;
}

static void PopulateCombo(HWND cmb, const wchar_t *const *items, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        SendMessageW(cmb, CB_ADDSTRING, 0, (LPARAM)items[i]);
    }
}

static LRESULT CALLBACK OsdSettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
    {
        BOOL dark = IsSetDark();
        g_setFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_setBgBrush   = CreateSolidBrush(dark ? MODERN_RGB_BG_DARK   : MODERN_RGB_BG_LIGHT);
        g_setEditBrush = CreateSolidBrush(dark ? MODERN_RGB_CARD_DARK : MODERN_RGB_CARD_LIGHT);
        D2d_ApplyWindowDwm(hwnd, dark, FALSE, TRUE);

        int y = 14;
        MakeCtrl(hwnd, L"STATIC", L"显示方式:", 0, 0, 16, y + 3, 76, 20, 0);
        HWND cmbMode = MakeCtrl(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 96, y, 280, 220, IDC_OSD_CMB_MODE);
        static const wchar_t *const kModes[] = {
            L"关闭统计显示",
            L"仅桌面悬浮",
            L"仅 OSD (应用伴随)",
            L"仅 RTSS (3D/全屏)",
            L"桌面悬浮 + OSD",
            L"OSD + RTSS (自动互斥)",
            L"桌面悬浮 + RTSS",
            L"全部启用 (桌面悬浮 + OSD + RTSS)"
        };
        PopulateCombo(cmbMode, kModes, ARRAYSIZE(kModes));
        y += 32;

        MakeCtrl(hwnd, L"STATIC", L"悬浮置顶:", 0, 0, 16, y + 3, 76, 20, 0);
        HWND cmbTop = MakeCtrl(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 96, y, 120, 100, IDC_OSD_CMB_CAP_TOP);
        static const wchar_t *const kTops[] = { L"始终置顶", L"普通窗口层级" };
        PopulateCombo(cmbTop, kTops, ARRAYSIZE(kTops));

        MakeCtrl(hwnd, L"STATIC", L"OSD 模板:", 0, 0, 228, y + 3, 66, 20, 0);
        HWND cmbTpl = MakeCtrl(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 296, y, 80, 100, IDC_OSD_CMB_COMPACT);
        static const wchar_t *const kTpls[] = { L"标准", L"精简" };
        PopulateCombo(cmbTpl, kTpls, ARRAYSIZE(kTpls));
        y += 34;

        MakeCtrl(hwnd, L"STATIC", L"─── OSD 详细配置（启用 OSD 时可调整）───────────", 0, 0, 16, y, 366, 18, 0);
        y += 26;

        MakeCtrl(hwnd, L"STATIC", L"锚点位置:", 0, 0, 16, y + 3, 76, 20, 0);
        HWND cmbAnchor = MakeCtrl(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 96, y, 168, 180, IDC_OSD_CMB_ANCHOR);
        static const wchar_t *const kAnchors[MODERN_OSD_ANCHOR_COUNT] = {
            L"应用外左上角 (默认)",
            L"应用外右上角",
            L"应用内左上角",
            L"应用内右上角",
            L"应用内左下角",
            L"应用内右下角"
        };
        PopulateCombo(cmbAnchor, kAnchors, ARRAYSIZE(kAnchors));

        MakeCtrl(hwnd, L"STATIC", L"字重:", 0, 0, 270, y + 3, 38, 20, 0);
        HWND cmbWeight = MakeCtrl(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 310, y, 66, 160, IDC_OSD_CMB_WEIGHT);
        static const wchar_t *const kWeights[] = { L"细体", L"常规", L"中等", L"半粗", L"粗体" };
        PopulateCombo(cmbWeight, kWeights, ARRAYSIZE(kWeights));
        y += 34;

        MakeCtrl(hwnd, L"STATIC", L"不透明度:", 0, 0, 16, y + 4, 76, 20, 0);
        HWND sldOp = MakeCtrl(hwnd, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 0, 94, y, 204, 26, IDC_OSD_SLD_OPACITY);
        SendMessageW(sldOp, TBM_SETRANGE, TRUE, MAKELPARAM(MODERN_OSD_MIN_OPACITY, MODERN_OSD_MAX_OPACITY));
        MakeCtrl(hwnd, L"EDIT", L"90", ES_NUMBER | ES_RIGHT | WS_BORDER | WS_TABSTOP, 0, 304, y + 1, 48, 24, IDC_OSD_EDT_OPACITY);
        MakeCtrl(hwnd, L"STATIC", L"%", 0, 0, 356, y + 4, 20, 20, 0);
        y += 32;

        MakeCtrl(hwnd, L"STATIC", L"胶囊高度:", 0, 0, 16, y + 4, 76, 20, 0);
        HWND sldSz = MakeCtrl(hwnd, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 0, 94, y, 204, 26, IDC_OSD_SLD_SIZE);
        SendMessageW(sldSz, TBM_SETRANGE, TRUE, MAKELPARAM(MODERN_OSD_MIN_SIZE_PX, MODERN_OSD_MAX_SIZE_PX));
        MakeCtrl(hwnd, L"EDIT", L"22", ES_NUMBER | ES_RIGHT | WS_BORDER | WS_TABSTOP, 0, 304, y + 1, 48, 24, IDC_OSD_EDT_SIZE);
        MakeCtrl(hwnd, L"STATIC", L"px", 0, 0, 356, y + 4, 22, 20, 0);
        y += 34;

        MakeCtrl(hwnd, L"STATIC", L"X 偏移 (px):", 0, 0, 16, y + 3, 76, 20, 0);
        MakeCtrl(hwnd, L"EDIT", L"0", ES_AUTOHSCROLL | ES_RIGHT | WS_BORDER | WS_TABSTOP, 0, 96, y, 80, 24, IDC_OSD_EDT_OFFX);
        MakeCtrl(hwnd, L"STATIC", L"Y 间距 (px):", 0, 0, 210, y + 3, 78, 20, 0);
        MakeCtrl(hwnd, L"EDIT", L"4", ES_AUTOHSCROLL | ES_RIGHT | WS_BORDER | WS_TABSTOP, 0, 294, y, 82, 24, IDC_OSD_EDT_OFFY);
        y += 34;

        // Background mode dropdown + color swatch + color picker
        MakeCtrl(hwnd, L"STATIC", L"背景色:", 0, 0, 16, y + 4, 76, 20, 0);
        HWND cmbBg = MakeCtrl(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 96, y + 1, 128, 120, IDC_OSD_CMB_BG_MODE);
        static const wchar_t *const kBgModes[] = { L"跟随主题", L"自定义颜色", L"完全透明" };
        PopulateCombo(cmbBg, kBgModes, ARRAYSIZE(kBgModes));
        MakeCtrl(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, 232, y + 1, 28, 24, IDC_OSD_SWATCH_BG);
        MakeCtrl(hwnd, L"BUTTON", L"选择颜色...", BS_PUSHBUTTON | WS_TABSTOP, 0, 268, y, 108, 26, IDC_OSD_BTN_BG_PICK);
        y += 34;

        // Font color mode dropdown + color swatch + color picker
        MakeCtrl(hwnd, L"STATIC", L"字体颜色:", 0, 0, 16, y + 4, 76, 20, 0);
        HWND cmbTxt = MakeCtrl(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, 96, y + 1, 128, 100, IDC_OSD_CMB_TXT_MODE);
        static const wchar_t *const kTxtModes[] = { L"跟随主题", L"自定义颜色" };
        PopulateCombo(cmbTxt, kTxtModes, ARRAYSIZE(kTxtModes));
        MakeCtrl(hwnd, L"BUTTON", L"", BS_OWNERDRAW, 0, 232, y + 1, 28, 24, IDC_OSD_SWATCH_TXT);
        MakeCtrl(hwnd, L"BUTTON", L"选择颜色...", BS_PUSHBUTTON | WS_TABSTOP, 0, 268, y, 108, 26, IDC_OSD_BTN_TXT_PICK);
        y += 36;

        MakeCtrl(hwnd, L"STATIC", L"提示：支持滑块或手输数值 (14–48px)，修改实时生效并自动保存。", 0, 0, 16, y, 366, 20, 0);
        y += 28;

        MakeCtrl(hwnd, L"BUTTON", L"全部恢复默认", BS_PUSHBUTTON | WS_TABSTOP, 0, 16, y, 110, 28, IDC_OSD_BTN_RESET);
        MakeCtrl(hwnd, L"BUTTON", L"保存并关闭", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 268, y, 108, 28, IDC_OSD_BTN_CLOSE);

        SyncControlsFromConfig(hwnd);
        return 0;
    }
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
        if (dis && g_setCfgPtr && (dis->CtlID == IDC_OSD_SWATCH_BG || dis->CtlID == IDC_OSD_SWATCH_TXT)) {
            RECT rc = dis->rcItem;
            if (dis->CtlID == IDC_OSD_SWATCH_BG && g_setCfgPtr->osdBgTransparent) {
                FillRect(dis->hDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
                HBRUSH gray = CreateSolidBrush(RGB(180, 180, 180));
                int step = 6;
                for (int py = rc.top; py < rc.bottom; py += step) {
                    for (int px = rc.left; px < rc.right; px += step) {
                        if ((((px - rc.left) / step) + ((py - rc.top) / step)) & 1) {
                            RECT cell = { px, py, min(px + step, rc.right), min(py + step, rc.bottom) };
                            FillRect(dis->hDC, &cell, gray);
                        }
                    }
                }
                DeleteObject(gray);
            } else {
                uint32_t col24 = (dis->CtlID == IDC_OSD_SWATCH_BG)
                    ? (g_setCfgPtr->osdUseCustomBg ? g_setCfgPtr->osdBgColor : (IsSetDark() ? 0x0018181Bu : 0x00F4F4F5u))
                    : (g_setCfgPtr->osdUseCustomText ? g_setCfgPtr->osdTextColor : (IsSetDark() ? 0x00F4F4F5u : 0x0018181Bu));
                HBRUSH br = CreateSolidBrush(Rgb24ToColorRef(col24));
                FillRect(dis->hDC, &rc, br);
                DeleteObject(br);
            }
            HBRUSH frameBr = CreateSolidBrush(RGB(120, 120, 128));
            FrameRect(dis->hDC, &rc, frameBr);
            DeleteObject(frameBr);
            return TRUE;
        }
        break;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE) {
            RefreshPreviewOnSettings();
        } else {
            g_previewMode = FALSE;
            WindowOsd_Hide();
        }
        return 0;
    case WM_MOVE:
    case WM_MOVING:
        if (g_previewMode) RefreshPreviewOnSettings();
        break;
    case WM_HSCROLL:
        ReadConfigAndApplyLive(hwnd, TRUE, FALSE);
        return 0;
    case WM_COMMAND:
    {
        WORD id = LOWORD(wp);
        WORD code = HIWORD(wp);
        if (id == IDC_OSD_BTN_CLOSE) {
            ReadConfigAndApplyLive(hwnd, FALSE, TRUE);
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_OSD_BTN_RESET && g_setCfgPtr) {
            int savedX = g_setCfgPtr->capsuleX;
            int savedY = g_setCfgPtr->capsuleY;
            int savedTheme = g_setCfgPtr->themeMode;
            ModernUiConfig_Defaults(g_setCfgPtr);
            g_setCfgPtr->capsuleX  = savedX;
            g_setCfgPtr->capsuleY  = savedY;
            g_setCfgPtr->themeMode = savedTheme;
            SyncControlsFromConfig(hwnd);
            ApplyAndPersistLive();
            return 0;
        }
        if ((id == IDC_OSD_BTN_BG_PICK || id == IDC_OSD_SWATCH_BG) && g_setCfgPtr && code == BN_CLICKED) {
            uint32_t c = g_setCfgPtr->osdUseCustomBg ? g_setCfgPtr->osdBgColor : 0x0018181Bu;
            if (PickColorDialog(hwnd, &c)) {
                g_setCfgPtr->osdBgColor       = c;
                g_setCfgPtr->osdUseCustomBg   = TRUE;
                g_setCfgPtr->osdBgTransparent = FALSE;
                SyncControlsFromConfig(hwnd);
                ApplyAndPersistLive();
            }
            return 0;
        }
        if ((id == IDC_OSD_BTN_TXT_PICK || id == IDC_OSD_SWATCH_TXT) && g_setCfgPtr && code == BN_CLICKED) {
            uint32_t c = g_setCfgPtr->osdUseCustomText ? g_setCfgPtr->osdTextColor : 0x00F4F4F5u;
            if (PickColorDialog(hwnd, &c)) {
                g_setCfgPtr->osdTextColor     = c;
                g_setCfgPtr->osdUseCustomText = TRUE;
                SyncControlsFromConfig(hwnd);
                ApplyAndPersistLive();
            }
            return 0;
        }
        if ((id == IDC_OSD_CMB_MODE || id == IDC_OSD_CMB_CAP_TOP || id == IDC_OSD_CMB_COMPACT ||
             id == IDC_OSD_CMB_ANCHOR || id == IDC_OSD_CMB_WEIGHT ||
             id == IDC_OSD_CMB_BG_MODE || id == IDC_OSD_CMB_TXT_MODE) && code == CBN_SELCHANGE) {
            ReadConfigAndApplyLive(hwnd, FALSE, FALSE);
            return 0;
        }
        if (id == IDC_OSD_EDT_OPACITY || id == IDC_OSD_EDT_SIZE || id == IDC_OSD_EDT_OFFX || id == IDC_OSD_EDT_OFFY) {
            if (code == EN_CHANGE) {
                ReadConfigAndApplyLive(hwnd, FALSE, FALSE);
                return 0;
            }
            if (code == EN_KILLFOCUS) {
                ReadConfigAndApplyLive(hwnd, FALSE, TRUE);
                return 0;
            }
        }
        break;
    }
    case WM_ERASEBKGND:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_setBgBrush);
        return 1;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    {
        HDC dc = (HDC)wp;
        BOOL dark = IsSetDark();
        SetTextColor(dc, dark ? MODERN_RGB_TEXT_DARK : MODERN_RGB_TEXT_LIGHT);
        SetBkColor(dc,   dark ? MODERN_RGB_CARD_DARK : MODERN_RGB_CARD_LIGHT);
        return (LRESULT)g_setEditBrush;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC dc = (HDC)wp;
        BOOL dark = IsSetDark();
        SetTextColor(dc, dark ? MODERN_RGB_TEXT_DARK : MODERN_RGB_TEXT_LIGHT);
        SetBkColor(dc,   dark ? MODERN_RGB_BG_DARK   : MODERN_RGB_BG_LIGHT);
        return (LRESULT)g_setBgBrush;
    }
    case WM_CLOSE:
        ReadConfigAndApplyLive(hwnd, FALSE, TRUE);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_previewMode = FALSE;
        WindowOsd_Hide();
        if (g_setFont)      { DeleteObject(g_setFont);      g_setFont = NULL; }
        if (g_setBgBrush)   { DeleteObject(g_setBgBrush);   g_setBgBrush = NULL; }
        if (g_setEditBrush) { DeleteObject(g_setEditBrush); g_setEditBrush = NULL; }
        g_setHwnd = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void WindowOsd_ShowSettingsWindow(HWND owner, ModernUiConfig *cfg,
                                  WindowOsdConfigChangedCallback onChanged, void *user)
{
    if (!cfg || !g_osdInst) return;
    g_setCfgPtr      = cfg;
    g_setChangedCb   = onChanged;
    g_setChangedUser = user;

    if (g_setHwnd && IsWindow(g_setHwnd)) {
        SyncControlsFromConfig(g_setHwnd);
        ShowWindow(g_setHwnd, SW_SHOW);
        SetForegroundWindow(g_setHwnd);
        RefreshPreviewOnSettings();
        return;
    }

    if (!g_setClassReg) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = OsdSettingsWndProc;
        wc.hInstance     = g_osdInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = OSD_SET_CLASS;
        if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            g_setClassReg = TRUE;
        } else {
            return;
        }
    }

    int w = 412, h = 448;
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    if (owner && IsWindow(owner)) {
        RECT r;
        if (GetWindowRect(owner, &r)) {
            x = r.left + ((r.right - r.left) - w) / 2;
            y = r.top  + ((r.bottom - r.top) - h) / 2;
            if (y < 60) y = 60; // leave room above the window to preview outside-top-left
        }
    }

    g_setHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        OSD_SET_CLASS,
        L"统计",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, w, h,
        owner, NULL, g_osdInst, NULL);
    if (g_setHwnd) {
        ShowWindow(g_setHwnd, SW_SHOW);
        SetForegroundWindow(g_setHwnd);
        RefreshPreviewOnSettings();
    }
}

void WindowOsd_CloseSettingsWindow(void)
{
    if (g_setHwnd && IsWindow(g_setHwnd)) {
        DestroyWindow(g_setHwnd);
    }
    g_setHwnd = NULL;
    g_previewMode = FALSE;
}

BOOL WindowOsd_IsVisible(void)
{
    return g_osdVisible;
}

HWND WindowOsd_GetHwnd(void)
{
    return g_osdHwnd;
}

DWORD WindowOsd_GetExStyle(void)
{
    if (!g_osdHwnd) return 0;
    return (DWORD)GetWindowLongPtrW(g_osdHwnd, GWL_EXSTYLE);
}
