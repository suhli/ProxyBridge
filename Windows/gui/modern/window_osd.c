#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "window_osd.h"
#include "d2d_renderer.h"
#include <dwmapi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define OSD_WND_CLASS   L"ProxyBridgeModernOsdWnd"
#define OSD_EX_STYLES   (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW)

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

static HWINEVENTHOOK               g_hookFg = NULL;
static HWINEVENTHOOK               g_hookMin = NULL;
static HWINEVENTHOOK               g_hookLoc = NULL;
static WindowOsdForegroundCallback g_fgCb = NULL;
static void                       *g_fgUser = NULL;

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
    g_targetHwnd = NULL;
    g_fgCb       = NULL;
    g_fgUser     = NULL;
}

void WindowOsd_Hide(void)
{
    if (g_osdHwnd && g_osdVisible) {
        ShowWindow(g_osdHwnd, SW_HIDE);
    }
    g_osdVisible = FALSE;
    g_targetHwnd = NULL;
}

void WindowOsd_Update(HWND targetHwnd, const ModernAppTraffic *app, BOOL proxyEnabled, const ModernUiConfig *cfg)
{
    if (!g_osdHwnd || !app || !cfg) {
        WindowOsd_Hide();
        return;
    }

    // Never attach companion OSD to our own process or an inactive/minimized window
    DWORD targetPid = 0;
    if (targetHwnd) GetWindowThreadProcessId(targetHwnd, &targetPid);
    if (!targetHwnd || targetPid == GetCurrentProcessId() || GetForegroundWindow() != targetHwnd) {
        WindowOsd_Hide();
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
    const D2dFontSlot font = D2D_FONT_MONO_11;
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

    float scale = (float)safeCfg.osdScalePct / 100.0f;
    int pillW = (int)ceilf(baseW * scale);
    int pillH = (int)ceilf(baseH * scale);
    if (pillW < 100) pillW = 100;
    if (pillH < 16)  pillH = 16;

    int anchorX = 0, anchorY = 0;
    if (!ComputeTargetAnchor(targetHwnd, g_anchor, g_offsetX, g_offsetY, pillW, pillH, &anchorX, &anchorY)) {
        WindowOsd_Hide();
        return;
    }

    if (!D2dLayered_Resize(&g_osdSurf, pillW, pillH)) return;

    BOOL dark = D2d_ResolveDarkMode(safeCfg.themeMode);
    uint32_t bgArgb     = dark ? MODERN_COLOR_BG_DARK_ARGB     : MODERN_COLOR_BG_LIGHT_ARGB;
    uint32_t borderArgb = dark ? MODERN_COLOR_BORDER_DARK_ARGB : MODERN_COLOR_BORDER_LIGHT_ARGB;
    uint32_t textArgb   = dark ? MODERN_COLOR_TEXT_DARK_ARGB   : MODERN_COLOR_TEXT_LIGHT_ARGB;
    uint32_t mutedArgb  = dark ? MODERN_COLOR_MUTED_DARK_ARGB  : MODERN_COLOR_MUTED_LIGHT_ARGB;
    uint32_t dotArgb    = ModernStatusColorArgb(proxyEnabled, app->rttMs);

    if (!D2dLayered_BeginDraw(&g_osdSurf)) return;
    D2d_SetScale(&g_osdSurf.canvas, scale);

    float fw = baseW;
    float fh = baseH;
    float rad = fh * 0.5f;

    // Pill background + 1px crisp inner stroke
    D2d_FillRoundedRect(&g_osdSurf.canvas, 0.5f, 0.5f, fw - 1.0f, fh - 1.0f, rad, rad, bgArgb);
    D2d_DrawRoundedRect(&g_osdSurf.canvas, 0.5f, 0.5f, fw - 1.0f, fh - 1.0f, rad, rad, 1.0f, borderArgb);

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

    D2d_DrawText(&g_osdSurf.canvas, font, dnSeg, x, 0.0f, wDn + 2.0f, fh, MODERN_COLOR_DL_ARGB, D2D_ALIGN_LEFT, TRUE);
    x += wDn + gap;

    D2d_DrawText(&g_osdSurf.canvas, font, upSeg, x, 0.0f, wUp + 2.0f, fh, MODERN_COLOR_UL_ARGB, D2D_ALIGN_LEFT, TRUE);
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
