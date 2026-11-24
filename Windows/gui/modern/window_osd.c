#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "window_osd.h"
#include "d2d_renderer.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OSD_WND_CLASS     L"ProxyBridgeModernOsdWnd"
#define OSD_SET_CLASS     L"ProxyBridgeModernOsdSettingsWnd"
#define OSD_EX_STYLES     (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW)

#define IDC_OSD_CHK_ENABLE   4101
#define IDC_OSD_CHK_RTSS     4102
#define IDC_OSD_CHK_COMPACT  4103
#define IDC_OSD_CMB_ANCHOR   4104
#define IDC_OSD_SLD_OPACITY  4105
#define IDC_OSD_LBL_OPACITY  4106
#define IDC_OSD_SLD_SCALE    4107
#define IDC_OSD_LBL_SCALE    4108
#define IDC_OSD_EDT_OFFX     4109
#define IDC_OSD_EDT_OFFY     4110
#define IDC_OSD_BTN_RESET    4111
#define IDC_OSD_BTN_CLOSE    4112

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

static void SyncControlsFromConfig(HWND hwnd)
{
    if (!g_setCfgPtr) return;
    ModernUiConfig_Sanitize(g_setCfgPtr);
    g_setSyncing = TRUE;

    CheckDlgButton(hwnd, IDC_OSD_CHK_ENABLE,  g_setCfgPtr->osdEnabled     ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_OSD_CHK_RTSS,    g_setCfgPtr->rtssEnabled    ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_OSD_CHK_COMPACT, g_setCfgPtr->osdCompactMode ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(hwnd, IDC_OSD_CMB_ANCHOR, CB_SETCURSEL, (WPARAM)g_setCfgPtr->osdAnchor, 0);
    SendDlgItemMessageW(hwnd, IDC_OSD_SLD_OPACITY, TBM_SETPOS, TRUE, (LPARAM)g_setCfgPtr->osdOpacity);
    SendDlgItemMessageW(hwnd, IDC_OSD_SLD_SCALE,   TBM_SETPOS, TRUE, (LPARAM)g_setCfgPtr->osdScalePct);

    wchar_t buf[32];
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d%%", g_setCfgPtr->osdOpacity);
    SetDlgItemTextW(hwnd, IDC_OSD_LBL_OPACITY, buf);
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d%%", g_setCfgPtr->osdScalePct);
    SetDlgItemTextW(hwnd, IDC_OSD_LBL_SCALE, buf);
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdOffsetX);
    SetDlgItemTextW(hwnd, IDC_OSD_EDT_OFFX, buf);
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d", g_setCfgPtr->osdOffsetY);
    SetDlgItemTextW(hwnd, IDC_OSD_EDT_OFFY, buf);

    g_setSyncing = FALSE;
}

static void ReadConfigAndApplyLive(HWND hwnd)
{
    if (g_setSyncing || !g_setCfgPtr) return;

    g_setCfgPtr->osdEnabled     = (IsDlgButtonChecked(hwnd, IDC_OSD_CHK_ENABLE)  == BST_CHECKED);
    g_setCfgPtr->rtssEnabled    = (IsDlgButtonChecked(hwnd, IDC_OSD_CHK_RTSS)    == BST_CHECKED);
    g_setCfgPtr->osdCompactMode = (IsDlgButtonChecked(hwnd, IDC_OSD_CHK_COMPACT) == BST_CHECKED);

    int sel = (int)SendDlgItemMessageW(hwnd, IDC_OSD_CMB_ANCHOR, CB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < MODERN_OSD_ANCHOR_COUNT) g_setCfgPtr->osdAnchor = sel;

    g_setCfgPtr->osdOpacity  = (int)SendDlgItemMessageW(hwnd, IDC_OSD_SLD_OPACITY, TBM_GETPOS, 0, 0);
    g_setCfgPtr->osdScalePct = (int)SendDlgItemMessageW(hwnd, IDC_OSD_SLD_SCALE,   TBM_GETPOS, 0, 0);

    wchar_t buf[32];
    GetDlgItemTextW(hwnd, IDC_OSD_EDT_OFFX, buf, ARRAYSIZE(buf));
    g_setCfgPtr->osdOffsetX = _wtoi(buf);
    GetDlgItemTextW(hwnd, IDC_OSD_EDT_OFFY, buf, ARRAYSIZE(buf));
    g_setCfgPtr->osdOffsetY = _wtoi(buf);

    ModernUiConfig_Sanitize(g_setCfgPtr);

    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d%%", g_setCfgPtr->osdOpacity);
    SetDlgItemTextW(hwnd, IDC_OSD_LBL_OPACITY, buf);
    _snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, L"%d%%", g_setCfgPtr->osdScalePct);
    SetDlgItemTextW(hwnd, IDC_OSD_LBL_SCALE, buf);

    if (g_setChangedCb) {
        g_setChangedCb(g_setCfgPtr, g_setChangedUser);
    }
    RefreshPreviewOnSettings();
}

static HWND MakeCtrl(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style,
                     DWORD exStyle, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, parent, (HMENU)(UINT_PTR)id, g_osdInst, NULL);
    if (c && g_setFont) SendMessageW(c, WM_SETFONT, (WPARAM)g_setFont, TRUE);
    return c;
}

static LRESULT CALLBACK OsdSettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
    {
        g_setFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_setBgBrush = CreateSolidBrush(MODERN_RGB_BG_DARK);
        D2d_ApplyWindowDwm(hwnd, TRUE, FALSE, TRUE);

        int y = 14;
        MakeCtrl(hwnd, L"BUTTON", L"启用左上角状态叠加 (OSD)", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 16, y, 220, 22, IDC_OSD_CHK_ENABLE);
        MakeCtrl(hwnd, L"BUTTON", L"精简模式", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 250, y, 110, 22, IDC_OSD_CHK_COMPACT);
        y += 28;
        MakeCtrl(hwnd, L"BUTTON", L"3D/全屏应用优先启用 RTSS 通道 A", BS_AUTOCHECKBOX | WS_TABSTOP, 0, 16, y, 320, 22, IDC_OSD_CHK_RTSS);
        y += 34;

        MakeCtrl(hwnd, L"STATIC", L"锚点位置:", 0, 0, 16, y + 3, 76, 20, 0);
        HWND cmb = MakeCtrl(hwnd, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 96, y, 254, 180, IDC_OSD_CMB_ANCHOR);
        static const wchar_t *const kAnchors[MODERN_OSD_ANCHOR_COUNT] = {
            L"应用外左上角 (默认，不遮菜单)",
            L"应用外右上角",
            L"应用内左上角",
            L"应用内右上角",
            L"应用内左下角",
            L"应用内右下角"
        };
        for (int i = 0; i < MODERN_OSD_ANCHOR_COUNT; i++) {
            SendMessageW(cmb, CB_ADDSTRING, 0, (LPARAM)kAnchors[i]);
        }
        y += 36;

        MakeCtrl(hwnd, L"STATIC", L"不透明度:", 0, 0, 16, y + 4, 76, 20, 0);
        HWND sldOp = MakeCtrl(hwnd, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 0, 94, y, 204, 26, IDC_OSD_SLD_OPACITY);
        SendMessageW(sldOp, TBM_SETRANGE, TRUE, MAKELPARAM(MODERN_OSD_MIN_OPACITY, MODERN_OSD_MAX_OPACITY));
        MakeCtrl(hwnd, L"STATIC", L"90%", SS_RIGHT, 0, 304, y + 4, 46, 20, IDC_OSD_LBL_OPACITY);
        y += 34;

        MakeCtrl(hwnd, L"STATIC", L"缩放大小:", 0, 0, 16, y + 4, 76, 20, 0);
        HWND sldSc = MakeCtrl(hwnd, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 0, 94, y, 204, 26, IDC_OSD_SLD_SCALE);
        SendMessageW(sldSc, TBM_SETRANGE, TRUE, MAKELPARAM(MODERN_OSD_MIN_SCALE_PCT, MODERN_OSD_MAX_SCALE_PCT));
        MakeCtrl(hwnd, L"STATIC", L"100%", SS_RIGHT, 0, 304, y + 4, 46, 20, IDC_OSD_LBL_SCALE);
        y += 36;

        MakeCtrl(hwnd, L"STATIC", L"X 偏移 (px):", 0, 0, 16, y + 3, 76, 20, 0);
        MakeCtrl(hwnd, L"EDIT", L"0", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 96, y, 70, 24, IDC_OSD_EDT_OFFX);
        MakeCtrl(hwnd, L"STATIC", L"Y 间距 (px):", 0, 0, 186, y + 3, 78, 20, 0);
        MakeCtrl(hwnd, L"EDIT", L"4", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 268, y, 82, 24, IDC_OSD_EDT_OFFY);
        y += 40;

        MakeCtrl(hwnd, L"STATIC", L"提示：调整参数即刻实时生效并自动保存至当前配置。", 0, 0, 16, y, 340, 20, 0);
        y += 30;

        MakeCtrl(hwnd, L"BUTTON", L"恢复默认", BS_PUSHBUTTON | WS_TABSTOP, 0, 16, y, 96, 28, IDC_OSD_BTN_RESET);
        MakeCtrl(hwnd, L"BUTTON", L"保存并关闭", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 244, y, 106, 28, IDC_OSD_BTN_CLOSE);

        SyncControlsFromConfig(hwnd);
        return 0;
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
        ReadConfigAndApplyLive(hwnd);
        return 0;
    case WM_COMMAND:
    {
        WORD id = LOWORD(wp);
        WORD code = HIWORD(wp);
        if (id == IDC_OSD_BTN_CLOSE) {
            ReadConfigAndApplyLive(hwnd);
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_OSD_BTN_RESET && g_setCfgPtr) {
            g_setCfgPtr->osdEnabled     = TRUE;
            g_setCfgPtr->rtssEnabled    = TRUE;
            g_setCfgPtr->osdCompactMode = FALSE;
            g_setCfgPtr->osdAnchor      = MODERN_OSD_ANCHOR_OUTSIDE_TOP_LEFT;
            g_setCfgPtr->osdOffsetX     = MODERN_OSD_DEFAULT_OFFSET_X;
            g_setCfgPtr->osdOffsetY     = MODERN_OSD_DEFAULT_OFFSET_Y;
            g_setCfgPtr->osdOpacity     = MODERN_OSD_DEFAULT_OPACITY;
            g_setCfgPtr->osdScalePct    = MODERN_OSD_DEFAULT_SCALE_PCT;
            SyncControlsFromConfig(hwnd);
            if (g_setChangedCb) g_setChangedCb(g_setCfgPtr, g_setChangedUser);
            RefreshPreviewOnSettings();
            return 0;
        }
        if ((id == IDC_OSD_CHK_ENABLE || id == IDC_OSD_CHK_RTSS || id == IDC_OSD_CHK_COMPACT) && code == BN_CLICKED) {
            ReadConfigAndApplyLive(hwnd);
            return 0;
        }
        if (id == IDC_OSD_CMB_ANCHOR && code == CBN_SELCHANGE) {
            ReadConfigAndApplyLive(hwnd);
            return 0;
        }
        if ((id == IDC_OSD_EDT_OFFX || id == IDC_OSD_EDT_OFFY) && code == EN_CHANGE) {
            ReadConfigAndApplyLive(hwnd);
            return 0;
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
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC dc = (HDC)wp;
        SetTextColor(dc, MODERN_RGB_TEXT_DARK);
        SetBkColor(dc, MODERN_RGB_BG_DARK);
        return (LRESULT)g_setBgBrush;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_previewMode = FALSE;
        WindowOsd_Hide();
        if (g_setFont)    { DeleteObject(g_setFont);    g_setFont = NULL; }
        if (g_setBgBrush) { DeleteObject(g_setBgBrush); g_setBgBrush = NULL; }
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

    int w = 382, h = 328;
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
        L"OSD 伴随微胶囊实时设置",
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
