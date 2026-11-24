#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "floating_capsule.h"
#include "d2d_renderer.h"
#include "stats_manager.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CAPSULE_WND_CLASS     L"ProxyBridgeModernCapsuleWnd"
#define CAPSULE_TIMER_BREATHE 101
#define CAPSULE_BREATHE_MS    125

// Context menu command IDs
#define IDM_CAP_TOGGLE_PROXY  5001
#define IDM_CAP_TOGGLE_TOP    5002
#define IDM_CAP_TOGGLE_OSD    5003
#define IDM_CAP_OSD_SETTINGS  5004
#define IDM_CAP_SHOW_MAIN     5005
#define IDM_CAP_HIDE          5006
#define IDM_CAP_EXIT          5007

static HINSTANCE                     g_capInst = NULL;
static BOOL                          g_capClassReg = FALSE;
static HWND                          g_capHwnd = NULL;
static D2dLayeredSurface             g_capSurf;
static ModernUiConfig                g_capCfg;
static BOOL                          g_capVisible = FALSE;
static BOOL                          g_capProxyEnabled = TRUE;
static uint64_t                      g_capRxBps = 0;
static uint64_t                      g_capTxBps = 0;
static uint32_t                      g_capRttMs = 0;
static int                           g_capX = 0;
static int                           g_capY = 0;
static int                           g_capW = MODERN_CAPSULE_WIDTH;
static int                           g_capH = MODERN_CAPSULE_HEIGHT;
static uint32_t                      g_breatheStep = 0;

// Dragging state
static BOOL                          g_dragging = FALSE;
static BOOL                          g_dragMoved = FALSE;
static POINT                         g_dragStartScreen = { 0, 0 };
static int                           g_dragWinStartX = 0;
static int                           g_dragWinStartY = 0;

static FloatingCapsuleActionCallback g_actionCb = NULL;
static FloatingCapsulePosCallback    g_posCb = NULL;
static void                         *g_cbUser = NULL;

void FloatingCapsule_ComputeSnapPosition(int x, int y, int width, int height,
                                         const RECT *workArea, int *outX, int *outY)
{
    int nx = x;
    int ny = y;
    if (workArea && (workArea->right - workArea->left) >= width &&
        (workArea->bottom - workArea->top) >= height) {
        // Clamp horizontally/vertically inside the target monitor's work area first
        if (nx < workArea->left)          nx = workArea->left;
        if (nx + width > workArea->right) nx = workArea->right - width;
        if (ny < workArea->top)           ny = workArea->top;
        if (ny + height > workArea->bottom) ny = workArea->bottom - height;

        // Auto edge-snap when within < MODERN_CAPSULE_SNAP_PX (16px) of left or right screen edge
        int distLeft  = nx - workArea->left;
        int distRight = workArea->right - (nx + width);
        if (distLeft < MODERN_CAPSULE_SNAP_PX) {
            nx = workArea->left;
        } else if (distRight < MODERN_CAPSULE_SNAP_PX) {
            nx = workArea->right - width;
        }
    }
    if (outX) *outX = nx;
    if (outY) *outY = ny;
}

static void GetWorkAreaForPoint(int x, int y, RECT *outWork)
{
    POINT pt = { x + g_capW / 2, y + g_capH / 2 };
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoW(mon, &mi)) {
        *outWork = mi.rcWork;
    } else if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, outWork, 0)) {
        outWork->left = 0; outWork->top = 0;
        outWork->right = GetSystemMetrics(SM_CXSCREEN);
        outWork->bottom = GetSystemMetrics(SM_CYSCREEN);
    }
}

static void ResolveInitialPosition(const ModernUiConfig *cfg, int *outX, int *outY)
{
    RECT wa;
    if (cfg && cfg->capsuleX != MODERN_POS_UNSET && cfg->capsuleY != MODERN_POS_UNSET) {
        GetWorkAreaForPoint(cfg->capsuleX, cfg->capsuleY, &wa);
        FloatingCapsule_ComputeSnapPosition(cfg->capsuleX, cfg->capsuleY, g_capW, g_capH, &wa, outX, outY);
        return;
    }

    // Default: top-right of primary monitor work area with comfortable margin
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
        wa.left = 0; wa.top = 0;
        wa.right = GetSystemMetrics(SM_CXSCREEN);
        wa.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    *outX = wa.right - g_capW - 24;
    *outY = wa.top + 24;
    if (*outX < wa.left) *outX = wa.left;
}

static void FormatCapsuleSpeed(uint64_t bps, wchar_t *out, size_t cch)
{
    // Compact yet crystal-clear format guaranteed to fit cleanly inside 184px fixed slots
    if (bps < 1024ULL) {
        _snwprintf_s(out, cch, _TRUNCATE, L"%llu B/s", (unsigned long long)bps);
    } else if (bps < 1024ULL * 1024ULL) {
        double kb = (double)bps / 1024.0;
        if (kb >= 100.0) _snwprintf_s(out, cch, _TRUNCATE, L"%.0f KB/s", kb);
        else             _snwprintf_s(out, cch, _TRUNCATE, L"%.1f KB/s", kb);
    } else if (bps < 1024ULL * 1024ULL * 1024ULL) {
        double mb = (double)bps / (1024.0 * 1024.0);
        if (mb >= 100.0) _snwprintf_s(out, cch, _TRUNCATE, L"%.0f MB/s", mb);
        else             _snwprintf_s(out, cch, _TRUNCATE, L"%.1f MB/s", mb);
    } else {
        double gb = (double)bps / (1024.0 * 1024.0 * 1024.0);
        _snwprintf_s(out, cch, _TRUNCATE, L"%.1f GB/s", gb);
    }
}

static void RenderCapsule(void)
{
    if (!g_capHwnd || !g_capVisible) return;
    if (!D2dLayered_BeginDraw(&g_capSurf)) return;

    BOOL dark = D2d_ResolveDarkMode(g_capCfg.themeMode);
    uint32_t bgArgb     = dark ? MODERN_COLOR_BG_DARK_ARGB     : MODERN_COLOR_BG_LIGHT_ARGB;
    uint32_t borderArgb = dark ? MODERN_COLOR_BORDER_DARK_ARGB : MODERN_COLOR_BORDER_LIGHT_ARGB;
    uint32_t dotArgb    = ModernStatusColorArgb(g_capProxyEnabled, g_capRttMs);

    float fw  = (float)g_capW;
    float fh  = (float)g_capH;
    float rad = (float)MODERN_CAPSULE_RADIUS;

    // Rounded 184x32 capsule (radius 16px) + 1px subtle inner stroke
    D2d_FillRoundedRect(&g_capSurf.canvas, 0.5f, 0.5f, fw - 1.0f, fh - 1.0f, rad, rad, bgArgb);
    D2d_DrawRoundedRect(&g_capSurf.canvas, 0.5f, 0.5f, fw - 1.0f, fh - 1.0f, rad, rad, 1.0f, borderArgb);

    // 8px breathing status dot (center x=15, y=16, core radius=4.0px)
    float cx = 15.0f;
    float cy = fh * 0.5f;
    if (g_capProxyEnabled) {
        // Gentle sine pulse halo (alpha 0x18..0x55)
        float phase = (float)(g_breatheStep % 24u) * (6.2831853f / 24.0f);
        float pulse = 0.5f + 0.5f * sinf(phase);
        uint32_t haloAlpha = (uint32_t)(24.0f + pulse * 64.0f);
        uint32_t haloArgb  = (haloAlpha << 24) | (dotArgb & 0x00FFFFFFu);
        D2d_FillCircle(&g_capSurf.canvas, cx, cy, 4.0f + pulse * 2.5f, haloArgb);
    }
    D2d_FillCircle(&g_capSurf.canvas, cx, cy, 4.0f, dotArgb);

    // Fixed-width columns with DirectWrite tabular figures (tnum) so digits NEVER jitter horizontally:
    // Layout: [dot]  [↓ 12.4 MB/s (73px)]  [↑ 840 KB/s (73px)]
    wchar_t dnVal[24], upVal[24], dnText[32], upText[32];
    FormatCapsuleSpeed(g_capRxBps, dnVal, ARRAYSIZE(dnVal));
    FormatCapsuleSpeed(g_capTxBps, upVal, ARRAYSIZE(upVal));
    _snwprintf_s(dnText, ARRAYSIZE(dnText), _TRUNCATE, L"\x2193 %s", dnVal);
    _snwprintf_s(upText, ARRAYSIZE(upText), _TRUNCATE, L"\x2191 %s", upVal);

    D2d_DrawText(&g_capSurf.canvas, D2D_FONT_MONO_11, dnText, 25.0f, 0.0f, 75.0f, fh,
                 MODERN_COLOR_DL_ARGB, D2D_ALIGN_LEFT, TRUE);
    D2d_DrawText(&g_capSurf.canvas, D2D_FONT_MONO_11, upText, 102.0f, 0.0f, 75.0f, fh,
                 MODERN_COLOR_UL_ARGB, D2D_ALIGN_LEFT, TRUE);

    D2dLayered_EndDrawAndPresent(&g_capSurf, g_capHwnd, g_capX, g_capY, 255);
}

static void ApplyZOrderAndPos(void)
{
    if (!g_capHwnd) return;
    HWND insertAfter = g_capCfg.capsuleTopmost ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(g_capHwnd, insertAfter, g_capX, g_capY, g_capW, g_capH,
                 SWP_NOACTIVATE | (g_capVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
}

static void ShowCapsuleContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU pm = CreatePopupMenu();
    if (!pm) return;

    AppendMenuW(pm, MF_STRING | (g_capProxyEnabled ? MF_CHECKED : MF_UNCHECKED),
                IDM_CAP_TOGGLE_PROXY, g_capProxyEnabled ? L"暂停代理 (运行中)" : L"恢复代理 (已暂停)");
    AppendMenuW(pm, MF_STRING | (g_capCfg.capsuleTopmost ? MF_CHECKED : MF_UNCHECKED),
                IDM_CAP_TOGGLE_TOP, L"悬浮胶囊置顶");
    AppendMenuW(pm, MF_STRING | (g_capCfg.osdEnabled ? MF_CHECKED : MF_UNCHECKED),
                IDM_CAP_TOGGLE_OSD, L"应用伴随 OSD");
    AppendMenuW(pm, MF_STRING, IDM_CAP_OSD_SETTINGS, L"统计设置...");
    AppendMenuW(pm, MF_SEPARATOR, 0, NULL);
    AppendMenuW(pm, MF_STRING, IDM_CAP_SHOW_MAIN, L"打开主界面");
    AppendMenuW(pm, MF_STRING, IDM_CAP_HIDE, L"隐藏悬浮胶囊");
    AppendMenuW(pm, MF_SEPARATOR, 0, NULL);
    AppendMenuW(pm, MF_STRING, IDM_CAP_EXIT, L"退出 ProxyBridge");

    SetForegroundWindow(hwnd);
    UINT cmd = (UINT)TrackPopupMenu(pm, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                    pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(pm);

    if (!g_actionCb || cmd == 0) return;
    switch (cmd) {
    case IDM_CAP_TOGGLE_PROXY: g_actionCb(CAPSULE_ACTION_TOGGLE_PROXY, g_cbUser); break;
    case IDM_CAP_TOGGLE_TOP:   g_actionCb(CAPSULE_ACTION_TOGGLE_TOPMOST, g_cbUser); break;
    case IDM_CAP_TOGGLE_OSD:   g_actionCb(CAPSULE_ACTION_TOGGLE_OSD, g_cbUser); break;
    case IDM_CAP_OSD_SETTINGS: g_actionCb(CAPSULE_ACTION_OPEN_OSD_SETTINGS, g_cbUser); break;
    case IDM_CAP_SHOW_MAIN:    g_actionCb(CAPSULE_ACTION_SHOW_MAIN, g_cbUser); break;
    case IDM_CAP_HIDE:         g_actionCb(CAPSULE_ACTION_HIDE_CAPSULE, g_cbUser); break;
    case IDM_CAP_EXIT:         g_actionCb(CAPSULE_ACTION_EXIT_APP, g_cbUser); break;
    default: break;
    }
}

static void OnStatsUpdated(const ModernStatsView *view, void *user)
{
    (void)user;
    if (!view || !g_capHwnd) return;
    g_capProxyEnabled = view->proxyEnabled;
    g_capRxBps        = view->globalRxBps;
    g_capTxBps        = view->globalTxBps;
    g_capRttMs        = view->globalRttMs;
    if (g_capVisible) {
        RenderCapsule();
    }
}

static LRESULT CALLBACK CapsuleWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_LBUTTONDOWN:
    {
        GetCursorPos(&g_dragStartScreen);
        g_dragWinStartX = g_capX;
        g_dragWinStartY = g_capY;
        g_dragging      = TRUE;
        g_dragMoved     = FALSE;
        SetCapture(hwnd);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        if (g_dragging) {
            POINT cur;
            GetCursorPos(&cur);
            int dx = cur.x - g_dragStartScreen.x;
            int dy = cur.y - g_dragStartScreen.y;
            if (!g_dragMoved && (abs(dx) > 2 || abs(dy) > 2)) {
                g_dragMoved = TRUE;
            }
            if (g_dragMoved) {
                g_capX = g_dragWinStartX + dx;
                g_capY = g_dragWinStartY + dy;
                SetWindowPos(hwnd, NULL, g_capX, g_capY, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        if (g_dragging) {
            ReleaseCapture();
            g_dragging = FALSE;
            if (g_dragMoved) {
                RECT wa;
                GetWorkAreaForPoint(g_capX, g_capY, &wa);
                int snappedX = g_capX, snappedY = g_capY;
                FloatingCapsule_ComputeSnapPosition(g_capX, g_capY, g_capW, g_capH, &wa, &snappedX, &snappedY);
                g_capX = snappedX;
                g_capY = snappedY;
                g_capCfg.capsuleX = g_capX;
                g_capCfg.capsuleY = g_capY;
                SetWindowPos(hwnd, NULL, g_capX, g_capY, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                if (g_posCb) {
                    g_posCb(g_capX, g_capY, g_cbUser);
                }
            }
            g_dragMoved = FALSE;
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        g_dragging = FALSE;
        g_dragMoved = FALSE;
        return 0;
    case WM_LBUTTONDBLCLK:
        g_dragging = FALSE;
        g_dragMoved = FALSE;
        if (g_actionCb) {
            g_actionCb(CAPSULE_ACTION_SHOW_MAIN, g_cbUser);
        }
        return 0;
    case WM_RBUTTONUP:
        ShowCapsuleContextMenu(hwnd);
        return 0;
    case WM_TIMER:
        if (wp == CAPSULE_TIMER_BREATHE) {
            if (g_capVisible && g_capProxyEnabled && !g_dragging) {
                g_breatheStep++;
                RenderCapsule();
            }
            return 0;
        }
        break;
    case WM_SETTINGCHANGE:
    case WM_DISPLAYCHANGE:
    {
        // Re-clamp onto visible monitor if display topology changed, and re-render on theme change
        RECT wa;
        GetWorkAreaForPoint(g_capX, g_capY, &wa);
        FloatingCapsule_ComputeSnapPosition(g_capX, g_capY, g_capW, g_capH, &wa, &g_capX, &g_capY);
        ApplyZOrderAndPos();
        if (g_capVisible) RenderCapsule();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

BOOL FloatingCapsule_Init(HINSTANCE hInst, const ModernUiConfig *initialCfg,
                          FloatingCapsuleActionCallback onAction,
                          FloatingCapsulePosCallback onPosChanged,
                          void *user)
{
    if (g_capHwnd != NULL) return TRUE;
    g_capInst  = hInst;
    g_actionCb = onAction;
    g_posCb    = onPosChanged;
    g_cbUser   = user;

    if (initialCfg) {
        g_capCfg = *initialCfg;
        ModernUiConfig_Sanitize(&g_capCfg);
    } else {
        ModernUiConfig_Defaults(&g_capCfg);
    }

    if (!D2d_IsReady() && !D2d_Init()) return FALSE;

    if (!g_capClassReg) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_DBLCLKS;
        wc.lpfnWndProc   = CapsuleWndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_SIZEALL);
        wc.lpszClassName = CAPSULE_WND_CLASS;
        if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            g_capClassReg = TRUE;
        } else {
            return FALSE;
        }
    }

    ResolveInitialPosition(&g_capCfg, &g_capX, &g_capY);

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    if (g_capCfg.capsuleTopmost) exStyle |= WS_EX_TOPMOST;

    g_capHwnd = CreateWindowExW(
        exStyle,
        CAPSULE_WND_CLASS,
        L"ProxyBridge Capsule",
        WS_POPUP,
        g_capX, g_capY, g_capW, g_capH,
        NULL, NULL, hInst, NULL);
    if (!g_capHwnd) return FALSE;

    if (!D2dLayered_Create(&g_capSurf, g_capW, g_capH)) {
        DestroyWindow(g_capHwnd);
        g_capHwnd = NULL;
        return FALSE;
    }

    g_capVisible = g_capCfg.capsuleEnabled ? TRUE : FALSE;
    SetTimer(g_capHwnd, CAPSULE_TIMER_BREATHE, CAPSULE_BREATHE_MS, NULL);
    StatsManager_AddListener(OnStatsUpdated, NULL);

    if (g_capVisible) {
        RenderCapsule();
        ApplyZOrderAndPos();
    } else {
        ShowWindow(g_capHwnd, SW_HIDE);
    }
    return TRUE;
}

void FloatingCapsule_Shutdown(void)
{
    StatsManager_RemoveListener(OnStatsUpdated, NULL);
    if (g_capHwnd) {
        KillTimer(g_capHwnd, CAPSULE_TIMER_BREATHE);
        ShowWindow(g_capHwnd, SW_HIDE);
    }
    D2dLayered_Destroy(&g_capSurf);
    if (g_capHwnd) {
        DestroyWindow(g_capHwnd);
        g_capHwnd = NULL;
    }
    if (g_capClassReg && g_capInst) {
        UnregisterClassW(CAPSULE_WND_CLASS, g_capInst);
        g_capClassReg = FALSE;
    }
    g_capVisible = FALSE;
    g_dragging   = FALSE;
    g_actionCb   = NULL;
    g_posCb      = NULL;
    g_cbUser     = NULL;
}

void FloatingCapsule_ApplyConfig(const ModernUiConfig *cfg)
{
    if (!cfg || !g_capHwnd) return;
    g_capCfg = *cfg;
    ModernUiConfig_Sanitize(&g_capCfg);

    if (g_capCfg.capsuleX != MODERN_POS_UNSET && g_capCfg.capsuleY != MODERN_POS_UNSET && !g_dragging) {
        RECT wa;
        GetWorkAreaForPoint(g_capCfg.capsuleX, g_capCfg.capsuleY, &wa);
        FloatingCapsule_ComputeSnapPosition(g_capCfg.capsuleX, g_capCfg.capsuleY, g_capW, g_capH, &wa, &g_capX, &g_capY);
    }

    g_capVisible = g_capCfg.capsuleEnabled ? TRUE : FALSE;
    if (g_capVisible) {
        RenderCapsule();
        ApplyZOrderAndPos();
    } else {
        ShowWindow(g_capHwnd, SW_HIDE);
    }
}

void FloatingCapsule_SetProxyEnabled(BOOL enabled)
{
    g_capProxyEnabled = enabled ? TRUE : FALSE;
    if (g_capVisible) RenderCapsule();
}

BOOL FloatingCapsule_IsVisible(void)
{
    return g_capVisible;
}

HWND FloatingCapsule_GetHwnd(void)
{
    return g_capHwnd;
}

BOOL FloatingCapsule_GetPosition(int *outX, int *outY)
{
    if (!g_capHwnd) return FALSE;
    if (outX) *outX = g_capX;
    if (outY) *outY = g_capY;
    return TRUE;
}
