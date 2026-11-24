#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "main_window_modern.h"
#include "d2d_renderer.h"
#include "stats_manager.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>

#define MODERN_HEADER_CLASS  L"ProxyBridgeModernHeaderWnd"

#define IDC_MOD_APPS_LV      4201
#define IDC_MOD_RULES_LV     4202
#define IDC_MOD_RULE_ADD     4210
#define IDC_MOD_RULE_EDIT    4211
#define IDC_MOD_RULE_TOGGLE  4212
#define IDC_MOD_RULE_UP      4213
#define IDC_MOD_RULE_DOWN    4214
#define IDC_MOD_RULE_DEL     4215
#define IDC_MOD_RULE_SERVERS 4216
#define IDC_MOD_LOGS_STATS   4217

typedef enum {
    HIT_NONE = 0,
    HIT_TOGGLE_PROXY,
    HIT_BTN_CAPSULE,
    HIT_BTN_OSD,
    HIT_BTN_TELEMETRY,
    HIT_BTN_THEME,
    HIT_TAB_APPS,
    HIT_TAB_RULES,
    HIT_TAB_LOGS
} HeaderHitId;

typedef struct {
    HeaderHitId id;
    RECT        rc;
} HitZone;

static HWND                     g_mainHwnd = NULL;
static HINSTANCE                g_hInst = NULL;
static BOOL                     g_hdrClassReg = FALSE;
static HWND                     g_hHeader = NULL;
static D2dHwndSurface           g_hdrSurf;
static int                      g_dpi = 96;

static HWND                     g_hAppsLv = NULL;
static HWND                     g_hRulesLv = NULL;
static RECT                     g_rcAppsCard = {0};
static RECT                     g_rcRulesCard = {0};
static HWND                     g_hRuleBtns[7]; // add, edit, toggle, up, down, del, servers
static HWND                     g_hLogsStatsBtn = NULL;
static HIMAGELIST               g_hRowImgList = NULL;
static HFONT                    g_hUiFont = NULL;
static HFONT                    g_hMonoFont = NULL;

static ModernMainTab            g_curTab = MODERN_MAIN_TAB_APPS;
static ModernUiConfig           g_cfg;
static BOOL                     g_proxyEnabled = TRUE;
static BOOL                     g_isDark = TRUE;
static BOOL                     g_zhLang = FALSE;
static wchar_t                  g_profileName[PB_NAME_MAX] = L"Default";
static wchar_t                  g_versionStr[32] = L"4.3.0";

static ModernStatsView          g_statsCopy;
static const PBProfile         *g_lastProfile = NULL;
static int                      g_ruleActions[PB_MAX_RULE];
static int                      g_ruleEnabled[PB_MAX_RULE];
static int                      g_ruleCountCached = 0;

static HitZone                  g_hits[12];
static int                      g_hitCount = 0;
static HeaderHitId              g_hotHit = HIT_NONE;

static ModernMainActionCallback g_actionCb = NULL;
static void                    *g_actionUser = NULL;

static int MSc(int px) { return MulDiv(px, g_dpi, 96); }

static void AddHit(HeaderHitId id, float x, float y, float w, float h)
{
    if (g_hitCount >= (int)ARRAYSIZE(g_hits)) return;
    g_hits[g_hitCount].id = id;
    g_hits[g_hitCount].rc.left   = (LONG)x;
    g_hits[g_hitCount].rc.top    = (LONG)y;
    g_hits[g_hitCount].rc.right  = (LONG)(x + w);
    g_hits[g_hitCount].rc.bottom = (LONG)(y + h);
    g_hitCount++;
}

static HeaderHitId HitTestHeader(int x, int y)
{
    POINT pt = { x, y };
    for (int i = 0; i < g_hitCount; i++) {
        if (PtInRect(&g_hits[i].rc, pt)) return g_hits[i].id;
    }
    return HIT_NONE;
}

static void DrawSparkline(D2dCanvas *c, float x, float y, float w, float h,
                          const uint64_t *rxHist, const uint64_t *txHist, BOOL isDark)
{
    if (w < 24.0f || h < 16.0f) return;

    // Subtle baseline & mid-grid
    uint32_t gridCol = isDark ? 0x1AFFFFFFu : 0x18000000u;
    D2d_DrawLine(c, x, y + h - 1.0f, x + w, y + h - 1.0f, 1.0f, gridCol);
    D2d_DrawLine(c, x, y + h * 0.5f, x + w, y + h * 0.5f, 1.0f, gridCol);

    uint64_t maxVal = 1024ULL; // floor at 1 KB/s so idle line sits cleanly at bottom
    for (int i = 0; i < MODERN_HISTORY_LEN; i++) {
        if (rxHist[i] > maxVal) maxVal = rxHist[i];
        if (txHist[i] > maxVal) maxVal = txHist[i];
    }

    float dx = w / (float)(MODERN_HISTORY_LEN - 1);
    for (int i = 0; i < MODERN_HISTORY_LEN - 1; i++) {
        float x0 = x + (float)i * dx;
        float x1 = x + (float)(i + 1) * dx;

        float ry0 = y + h - 2.0f - (float)((double)rxHist[i]     / (double)maxVal) * (h - 4.0f);
        float ry1 = y + h - 2.0f - (float)((double)rxHist[i + 1] / (double)maxVal) * (h - 4.0f);
        D2d_DrawLine(c, x0, ry0, x1, ry1, 1.6f, MODERN_COLOR_DL_ARGB);

        float ty0 = y + h - 2.0f - (float)((double)txHist[i]     / (double)maxVal) * (h - 4.0f);
        float ty1 = y + h - 2.0f - (float)((double)txHist[i + 1] / (double)maxVal) * (h - 4.0f);
        D2d_DrawLine(c, x0, ty0, x1, ty1, 1.3f, MODERN_COLOR_UL_ARGB);
    }
}

static void DrawPillButton(D2dCanvas *c, HeaderHitId id, const wchar_t *label,
                           float x, float y, float w, float h,
                           BOOL active, BOOL isDark)
{
    BOOL hot = (g_hotHit == id);
    uint32_t bg, border, text;
    if (active) {
        bg     = isDark ? 0x3310B981u : 0x2610B981u;
        border = 0x9910B981u;
        text   = isDark ? 0xFF6EE7B7u : 0xFF047857u;
    } else {
        bg     = hot ? (isDark ? 0x26FFFFFFu : 0x1A000000u) : (isDark ? 0x12FFFFFFu : 0x0D000000u);
        border = isDark ? MODERN_COLOR_BORDER_DARK_ARGB : MODERN_COLOR_BORDER_LIGHT_ARGB;
        text   = isDark ? MODERN_COLOR_TEXT_DARK_ARGB   : MODERN_COLOR_TEXT_LIGHT_ARGB;
    }
    D2d_FillRoundedRect(c, x, y, w, h, h * 0.5f, h * 0.5f, bg);
    D2d_DrawRoundedRect(c, x, y, w, h, h * 0.5f, h * 0.5f, 1.0f, border);
    D2d_DrawText(c, D2D_FONT_UI_12, label, x, y, w, h, text, D2D_ALIGN_CENTER, FALSE);
    AddHit(id, x, y, w, h);
}

static void RenderHeader(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    if (!D2dHwnd_Ensure(&g_hdrSurf, hwnd, w, h)) return;
    if (!D2dHwnd_BeginDraw(&g_hdrSurf)) return;

    g_hitCount = 0;
    BOOL dark = g_isDark;
    uint32_t bgCol     = dark ? 0xFF18181Bu : 0xFFF4F4F5u;
    uint32_t cardCol   = dark ? 0xFF222227u : 0xFFFFFFFFu;
    uint32_t borderCol = dark ? MODERN_COLOR_BORDER_DARK_ARGB : MODERN_COLOR_BORDER_LIGHT_ARGB;
    uint32_t textCol   = dark ? MODERN_COLOR_TEXT_DARK_ARGB   : MODERN_COLOR_TEXT_LIGHT_ARGB;
    uint32_t mutedCol  = dark ? MODERN_COLOR_MUTED_DARK_ARGB  : MODERN_COLOR_MUTED_LIGHT_ARGB;

    D2dCanvas *c = &g_hdrSurf.canvas;
    D2d_Clear(c, bgCol);

    float fw = (float)w;
    float padX = 14.0f;

    // ==== 1. Top 48px Status Bar ====
    float barH = 48.0f;
    uint32_t statusDot = ModernStatusColorArgb(g_proxyEnabled, g_statsCopy.globalRttMs);
    D2d_FillCircle(c, padX + 6.0f, barH * 0.5f, 5.0f, statusDot);

    wchar_t titleBuf[128];
    _snwprintf_s(titleBuf, ARRAYSIZE(titleBuf), _TRUNCATE, L"ProxyBridge v%s", g_versionStr);
    D2d_DrawText(c, D2D_FONT_UI_14_BOLD, titleBuf, padX + 18.0f, 0.0f, 175.0f, barH, textCol, D2D_ALIGN_LEFT, FALSE);

    // Active profile badge
    wchar_t profBadge[96];
    _snwprintf_s(profBadge, ARRAYSIZE(profBadge), _TRUNCATE, L"%s", g_profileName);
    float profW = D2d_MeasureTextWidth(D2D_FONT_UI_12, profBadge, FALSE) + 18.0f;
    if (profW < 58.0f)  profW = 58.0f;
    if (profW > 140.0f) profW = 140.0f;
    D2d_FillRoundedRect(c, padX + 182.0f, 13.0f, profW, 22.0f, 11.0f, 11.0f, dark ? 0x16FFFFFFu : 0x10000000u);
    D2d_DrawText(c, D2D_FONT_UI_12, profBadge, padX + 182.0f, 13.0f, profW, 22.0f, mutedCol, D2D_ALIGN_CENTER, FALSE);

    // Right-aligned quick action pills + Master Toggle Switch
    float rx = fw - padX;
    float btnH = 26.0f;
    float btnY = (barH - btnH) * 0.5f;

    // Theme button
    const wchar_t *themeLbl = g_zhLang
        ? (g_cfg.themeMode == MODERN_THEME_SYSTEM ? L"主题: 自动" : (g_cfg.themeMode == MODERN_THEME_DARK ? L"主题: 暗黑" : L"主题: 浅色"))
        : (g_cfg.themeMode == MODERN_THEME_SYSTEM ? L"Theme: Auto" : (g_cfg.themeMode == MODERN_THEME_DARK ? L"Theme: Dark" : L"Theme: Light"));
    float wTheme = 88.0f;
    rx -= wTheme;
    DrawPillButton(c, HIT_BTN_THEME, themeLbl, rx, btnY, wTheme, btnH, FALSE, dark);

    // Telemetry settings button
    float wTel = 68.0f;
    rx -= (wTel + 8.0f);
    DrawPillButton(c, HIT_BTN_TELEMETRY, g_zhLang ? L"统计..." : L"Stats...", rx, btnY, wTel, btnH, FALSE, dark);

    // OSD quick toggle
    float wOsd = 68.0f;
    rx -= (wOsd + 8.0f);
    DrawPillButton(c, HIT_BTN_OSD, L"OSD", rx, btnY, wOsd, btnH, g_cfg.osdEnabled, dark);

    // Floating Capsule quick toggle
    float wCap = 84.0f;
    rx -= (wCap + 8.0f);
    DrawPillButton(c, HIT_BTN_CAPSULE, g_zhLang ? L"悬浮胶囊" : L"Capsule", rx, btnY, wCap, btnH, g_cfg.capsuleEnabled, dark);

    // Master Proxy Toggle Switch (modern pill switch + label)
    float swW = 44.0f, swH = 22.0f;
    float swLabelW = 68.0f;
    rx -= (swW + swLabelW + 14.0f);
    float swY = (barH - swH) * 0.5f;
    uint32_t swTrack = g_proxyEnabled ? MODERN_COLOR_OK_ARGB : MODERN_COLOR_ERR_ARGB;
    D2d_FillRoundedRect(c, rx, swY, swW, swH, swH * 0.5f, swH * 0.5f, swTrack);
    float knobX = g_proxyEnabled ? (rx + swW - swH * 0.5f) : (rx + swH * 0.5f);
    D2d_FillCircle(c, knobX, swY + swH * 0.5f, 8.0f, 0xFFFFFFFFu);
    D2d_DrawText(c, D2D_FONT_UI_12,
                 g_proxyEnabled ? (g_zhLang ? L"代理运行" : L"Active") : (g_zhLang ? L"已暂停" : L"Paused"),
                 rx + swW + 6.0f, 0.0f, swLabelW, barH,
                 g_proxyEnabled ? MODERN_COLOR_OK_ARGB : MODERN_COLOR_ERR_ARGB,
                 D2D_ALIGN_LEFT, FALSE);
    AddHit(HIT_TOGGLE_PROXY, rx - 4.0f, btnY, swW + swLabelW + 8.0f, btnH);

    // Subtle divider under 48px status bar
    D2d_DrawLine(c, padX, barH, fw - padX, barH, 1.0f, borderCol);

    // ==== 2. Real-time Overview Cards (64px, 3 equal columns) ====
    float cardY = barH + 10.0f;
    float cardH = 64.0f;
    float cardGap = 12.0f;
    float cardW = (fw - padX * 2.0f - cardGap * 2.0f) / 3.0f;
    if (cardW < 120.0f) cardW = 120.0f;

    wchar_t dnStr[32], upStr[32], totStr[32], subBuf[64], rttBuf[32];
    ModernFormatSpeedW(g_statsCopy.globalRxBps, FALSE, dnStr, ARRAYSIZE(dnStr));
    ModernFormatSpeedW(g_statsCopy.globalTxBps, FALSE, upStr, ARRAYSIZE(upStr));
    ModernFormatBytesW(g_statsCopy.globalRxTotal + g_statsCopy.globalTxTotal, totStr, ARRAYSIZE(totStr));

    // Card 1: Global Real-time Speed + 30s Sparkline
    float c1x = padX;
    D2d_FillRoundedRect(c, c1x, cardY, cardW, cardH, 10.0f, 10.0f, cardCol);
    D2d_DrawRoundedRect(c, c1x, cardY, cardW, cardH, 10.0f, 10.0f, 1.0f, borderCol);
    D2d_DrawText(c, D2D_FONT_UI_12, g_zhLang ? L"全局实时速率 (30s)" : L"Real-time Speed (30s)",
                 c1x + 12.0f, cardY + 6.0f, cardW - 24.0f, 16.0f, mutedCol, D2D_ALIGN_LEFT, FALSE);

    wchar_t dnLine[40], upLine[40];
    _snwprintf_s(dnLine, ARRAYSIZE(dnLine), _TRUNCATE, L"\x2193 %s", dnStr);
    _snwprintf_s(upLine, ARRAYSIZE(upLine), _TRUNCATE, L"\x2191 %s", upStr);
    D2d_DrawText(c, D2D_FONT_MONO_12_BOLD, dnLine, c1x + 12.0f, cardY + 23.0f, 115.0f, 18.0f, MODERN_COLOR_DL_ARGB, D2D_ALIGN_LEFT, TRUE);
    D2d_DrawText(c, D2D_FONT_MONO_12,      upLine, c1x + 12.0f, cardY + 41.0f, 115.0f, 16.0f, MODERN_COLOR_UL_ARGB, D2D_ALIGN_LEFT, TRUE);

    float sparkX = c1x + 128.0f;
    float sparkW = cardW - 140.0f;
    if (sparkW > 30.0f) {
        DrawSparkline(c, sparkX, cardY + 22.0f, sparkW, 34.0f,
                      g_statsCopy.rxHistory, g_statsCopy.txHistory, dark);
    }

    // Card 2: Session Cumulative Traffic + Active Connections
    float c2x = c1x + cardW + cardGap;
    D2d_FillRoundedRect(c, c2x, cardY, cardW, cardH, 10.0f, 10.0f, cardCol);
    D2d_DrawRoundedRect(c, c2x, cardY, cardW, cardH, 10.0f, 10.0f, 1.0f, borderCol);
    D2d_DrawText(c, D2D_FONT_UI_12, g_zhLang ? L"本次会话累计 / 活跃连接" : L"Session Traffic / Connections",
                 c2x + 12.0f, cardY + 6.0f, cardW - 24.0f, 16.0f, mutedCol, D2D_ALIGN_LEFT, FALSE);
    D2d_DrawText(c, D2D_FONT_MONO_16_BOLD, totStr,
                 c2x + 12.0f, cardY + 24.0f, cardW * 0.58f, 26.0f, textCol, D2D_ALIGN_LEFT, TRUE);
    _snwprintf_s(subBuf, ARRAYSIZE(subBuf), _TRUNCATE,
                 g_zhLang ? L"%u 个活跃连接" : L"%u active conns", g_statsCopy.totalActiveConns);
    D2d_DrawText(c, D2D_FONT_MONO_12, subBuf,
                 c2x + cardW * 0.55f, cardY + 28.0f, cardW * 0.45f - 12.0f, 20.0f,
                 g_statsCopy.totalActiveConns > 0 ? MODERN_COLOR_OK_ARGB : mutedCol,
                 D2D_ALIGN_RIGHT, TRUE);

    // Card 3: Foreground Focused App & Live Proxy RTT
    float c3x = c2x + cardW + cardGap;
    D2d_FillRoundedRect(c, c3x, cardY, cardW, cardH, 10.0f, 10.0f, cardCol);
    D2d_DrawRoundedRect(c, c3x, cardY, cardW, cardH, 10.0f, 10.0f, 1.0f, borderCol);
    D2d_DrawText(c, D2D_FONT_UI_12, g_zhLang ? L"前台焦点应用 & 代理延迟" : L"Foreground App & Latency",
                 c3x + 12.0f, cardY + 6.0f, cardW - 24.0f, 16.0f, mutedCol, D2D_ALIGN_LEFT, FALSE);

    const wchar_t *fgName = g_statsCopy.foregroundAppName[0]
        ? g_statsCopy.foregroundAppName
        : (g_zhLang ? L"等待代理流量..." : L"Idle");
    uint32_t showRtt = g_statsCopy.foregroundMatched ? g_statsCopy.foregroundStats.rttMs : g_statsCopy.globalRttMs;
    if (!g_proxyEnabled) lstrcpynW(rttBuf, g_zhLang ? L"已暂停" : L"PAUSED", ARRAYSIZE(rttBuf));
    else if (showRtt > 0) _snwprintf_s(rttBuf, ARRAYSIZE(rttBuf), _TRUNCATE, L"%u ms", showRtt);
    else lstrcpynW(rttBuf, L"-- ms", ARRAYSIZE(rttBuf));

    uint32_t rttCol = (showRtt > 0 || !g_proxyEnabled) ? ModernStatusColorArgb(g_proxyEnabled, showRtt) : mutedCol;
    D2d_DrawText(c, D2D_FONT_UI_13, fgName,
                 c3x + 12.0f, cardY + 26.0f, cardW - 96.0f, 24.0f, textCol, D2D_ALIGN_LEFT, FALSE);
    D2d_DrawText(c, D2D_FONT_MONO_16_BOLD, rttBuf,
                 c3x + cardW - 86.0f, cardY + 24.0f, 74.0f, 26.0f, rttCol, D2D_ALIGN_RIGHT, TRUE);

    // ==== 3. Modern 3-Tab Navigation Strip (36px) ====
    float tabY = cardY + cardH + 10.0f;
    float tabH = 34.0f;
    const wchar_t *tabLabels[3] = {
        g_zhLang ? L"活跃应用监控" : L"Active Apps",
        g_zhLang ? L"代理规则管理" : L"Proxy Rules",
        g_zhLang ? L"连接日志与设置" : L"Logs & Settings"
    };
    const HeaderHitId tabIds[3] = { HIT_TAB_APPS, HIT_TAB_RULES, HIT_TAB_LOGS };
    float tx = padX;
    for (int i = 0; i < 3; i++) {
        BOOL sel = ((int)g_curTab == i);
        BOOL hot = (g_hotHit == tabIds[i]);
        float tw = 128.0f;
        uint32_t tBg = sel ? (dark ? 0x24FFFFFFu : 0x18000000u)
                           : (hot ? (dark ? 0x14FFFFFFu : 0x0E000000u) : 0x00000000u);
        if (tBg) D2d_FillRoundedRect(c, tx, tabY, tw, tabH - 2.0f, 8.0f, 8.0f, tBg);
        D2d_DrawText(c, sel ? D2D_FONT_UI_13 : D2D_FONT_UI_12, tabLabels[i],
                     tx, tabY, tw, tabH - 2.0f, sel ? textCol : mutedCol, D2D_ALIGN_CENTER, FALSE);
        if (sel) {
            D2d_FillRoundedRect(c, tx + 24.0f, tabY + tabH - 4.0f, tw - 48.0f, 2.5f, 1.2f, 1.2f, MODERN_COLOR_DL_ARGB);
        }
        AddHit(tabIds[i], tx, tabY, tw, tabH);
        tx += tw + 8.0f;
    }

    D2d_DrawLine(c, padX, tabY + tabH, fw - padX, tabY + tabH, 1.0f, borderCol);
    D2dHwnd_EndDraw(&g_hdrSurf);
}

static LRESULT CALLBACK ModernHeaderWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        RenderHeader(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
    {
        int x = (int)(short)LOWORD(lp);
        int y = (int)(short)HIWORD(lp);
        HeaderHitId hit = HitTestHeader(x, y);
        if (hit != g_hotHit) {
            g_hotHit = hit;
            InvalidateRect(hwnd, NULL, FALSE);
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            tme.dwHoverTime = 0;
            TrackMouseEvent(&tme);
        }
        SetCursor(LoadCursorW(NULL, hit != HIT_NONE ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_hotHit != HIT_NONE) {
            g_hotHit = HIT_NONE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
    {
        int x = (int)(short)LOWORD(lp);
        int y = (int)(short)HIWORD(lp);
        HeaderHitId hit = HitTestHeader(x, y);
        if (!g_actionCb) return 0;
        switch (hit) {
        case HIT_TOGGLE_PROXY:  g_actionCb(MODERN_ACT_TOGGLE_PROXY,   0, g_actionUser); break;
        case HIT_BTN_CAPSULE:   g_actionCb(MODERN_ACT_TOGGLE_CAPSULE, 0, g_actionUser); break;
        case HIT_BTN_OSD:       g_actionCb(MODERN_ACT_TOGGLE_OSD,     0, g_actionUser); break;
        case HIT_BTN_TELEMETRY: g_actionCb(MODERN_ACT_OPEN_TELEMETRY, 0, g_actionUser); break;
        case HIT_BTN_THEME:     g_actionCb(MODERN_ACT_CYCLE_THEME,    0, g_actionUser); break;
        case HIT_TAB_APPS:      ModernMain_SetTab(MODERN_MAIN_TAB_APPS);  g_actionCb(MODERN_ACT_TAB_CHANGED, MODERN_MAIN_TAB_APPS,  g_actionUser); break;
        case HIT_TAB_RULES:     ModernMain_SetTab(MODERN_MAIN_TAB_RULES); g_actionCb(MODERN_ACT_TAB_CHANGED, MODERN_MAIN_TAB_RULES, g_actionUser); break;
        case HIT_TAB_LOGS:      ModernMain_SetTab(MODERN_MAIN_TAB_LOGS);  g_actionCb(MODERN_ACT_TAB_CHANGED, MODERN_MAIN_TAB_LOGS,  g_actionUser); break;
        default: break;
        }
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void OnStatsUpdated(const ModernStatsView *view, void *user)
{
    (void)user;
    if (!view) return;
    g_statsCopy = *view;
    if (g_hHeader && IsWindow(g_hHeader)) {
        InvalidateRect(g_hHeader, NULL, FALSE);
    }
    if (g_hAppsLv && IsWindow(g_hAppsLv)) {
        ListView_SetItemCountEx(g_hAppsLv, (int)g_statsCopy.appCount, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        if (g_curTab == MODERN_MAIN_TAB_APPS) {
            InvalidateRect(g_hAppsLv, NULL, FALSE);
        }
    }
}

static void StyleListViewColors(HWND lv, BOOL isDark)
{
    if (!lv || !IsWindow(lv)) return;
    COLORREF bg  = isDark ? MODERN_RGB_CARD_DARK  : MODERN_RGB_CARD_LIGHT;
    COLORREF txt = isDark ? MODERN_RGB_TEXT_DARK  : MODERN_RGB_TEXT_LIGHT;
    ListView_SetBkColor(lv, bg);
    ListView_SetTextBkColor(lv, bg);
    ListView_SetTextColor(lv, txt);
    SetWindowTheme(lv, isDark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    HWND hdr = ListView_GetHeader(lv);
    if (hdr) SetWindowTheme(hdr, isDark ? L"DarkMode_ItemsView" : L"ItemsView", NULL);
    InvalidateRect(lv, NULL, TRUE);
}

static void UpdateTabControlVisibility(void)
{
    BOOL showApps  = (g_curTab == MODERN_MAIN_TAB_APPS);
    BOOL showRules = (g_curTab == MODERN_MAIN_TAB_RULES);
    BOOL showLogs  = (g_curTab == MODERN_MAIN_TAB_LOGS);

    if (g_hAppsLv)  ShowWindow(g_hAppsLv,  showApps  ? SW_SHOW : SW_HIDE);
    if (g_hRulesLv) ShowWindow(g_hRulesLv, showRules ? SW_SHOW : SW_HIDE);
    for (int i = 0; i < 7; i++) {
        if (g_hRuleBtns[i]) ShowWindow(g_hRuleBtns[i], showRules ? SW_SHOW : SW_HIDE);
    }
    if (g_hLogsStatsBtn) ShowWindow(g_hLogsStatsBtn, showLogs ? SW_SHOW : SW_HIDE);
}

static void UpdateLocalizedLabels(void)
{
    if (g_hAppsLv) {
        const wchar_t *cols[6] = {
            g_zhLang ? L"应用名 & PID" : L"Application & PID",
            g_zhLang ? L"命中规则" : L"Matched Rule",
            g_zhLang ? L"实时下载" : L"Download",
            g_zhLang ? L"实时上传" : L"Upload",
            g_zhLang ? L"累计流量" : L"Total",
            g_zhLang ? L"代理延迟" : L"Latency"
        };
        for (int i = 0; i < 6; i++) {
            LVCOLUMNW c; ZeroMemory(&c, sizeof(c));
            c.mask = LVCF_TEXT; c.pszText = (LPWSTR)cols[i];
            ListView_SetColumn(g_hAppsLv, i, &c);
        }
    }
    if (g_hRulesLv) {
        const wchar_t *cols[8] = {
            L"#",
            g_zhLang ? L"状态" : L"Status",
            g_zhLang ? L"规则名称" : L"Rule Name",
            g_zhLang ? L"应用进程" : L"Process",
            g_zhLang ? L"目标主机" : L"Targets",
            g_zhLang ? L"端口" : L"Ports",
            g_zhLang ? L"协议" : L"Proto",
            g_zhLang ? L"动作" : L"Action"
        };
        for (int i = 0; i < 8; i++) {
            LVCOLUMNW c; ZeroMemory(&c, sizeof(c));
            c.mask = LVCF_TEXT; c.pszText = (LPWSTR)cols[i];
            ListView_SetColumn(g_hRulesLv, i, &c);
        }
    }
    const wchar_t *btnTxt[7] = {
        g_zhLang ? L"+ 添加规则" : L"+ Add Rule",
        g_zhLang ? L"编辑" : L"Edit",
        g_zhLang ? L"启用 / 禁用" : L"Toggle",
        g_zhLang ? L"上移" : L"Up",
        g_zhLang ? L"下移" : L"Down",
        g_zhLang ? L"删除" : L"Delete",
        g_zhLang ? L"代理服务器..." : L"Proxy Servers..."
    };
    for (int i = 0; i < 7; i++) {
        if (g_hRuleBtns[i]) SetWindowTextW(g_hRuleBtns[i], btnTxt[i]);
    }
    if (g_hLogsStatsBtn) {
        SetWindowTextW(g_hLogsStatsBtn, g_zhLang ? L"统计 / OSD / 悬浮胶囊设置..." : L"Telemetry / OSD / Capsule Settings...");
    }
}

BOOL ModernMain_Init(HWND mainHwnd, HINSTANCE hInst, int dpi,
                     const ModernUiConfig *cfg,
                     ModernMainActionCallback onAction, void *user)
{
    if (g_hHeader != NULL) return TRUE;
    g_mainHwnd   = mainHwnd;
    g_hInst      = hInst;
    g_dpi        = dpi >= 96 ? dpi : 96;
    g_actionCb   = onAction;
    g_actionUser = user;

    if (cfg) g_cfg = *cfg;
    else     ModernUiConfig_Defaults(&g_cfg);
    g_isDark = D2d_ResolveDarkMode(g_cfg.themeMode);

    if (!D2d_IsReady()) D2d_Init();

    if (!g_hdrClassReg) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = ModernHeaderWndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = MODERN_HEADER_CLASS;
        if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            g_hdrClassReg = TRUE;
        }
    }

    g_hUiFont = CreateFontW(-MSc(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hMonoFont = CreateFontW(-MSc(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    g_hRowImgList = ImageList_Create(1, MSc(32), ILC_COLOR32, 1, 1);

    // Direct2D top header + 3 cards + modern tab strip
    g_hHeader = CreateWindowExW(0, MODERN_HEADER_CLASS, L"",
                                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                0, 0, 100, 170, mainHwnd, NULL, hInst, NULL);

    // Tab 0: Active Applications Monitor (virtual ListView, 32px row height)
    g_hAppsLv = CreateWindowExW(0, WC_LISTVIEWW, NULL,
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                0, 0, 0, 0, mainHwnd, (HMENU)(UINT_PTR)IDC_MOD_APPS_LV, hInst, NULL);
    if (g_hAppsLv) {
        ListView_SetExtendedListViewStyle(g_hAppsLv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        if (g_hRowImgList) ListView_SetImageList(g_hAppsLv, g_hRowImgList, LVSIL_SMALL);
        SendMessageW(g_hAppsLv, WM_SETFONT, (WPARAM)g_hUiFont, TRUE);
        static const int w[6] = { 260, 180, 130, 130, 130, 110 };
        LVCOLUMNW col; ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        for (int i = 0; i < 6; i++) {
            col.pszText = L"";
            col.cx      = MSc(w[i]);
            col.fmt     = (i >= 2) ? LVCFMT_RIGHT : LVCFMT_LEFT;
            ListView_InsertColumn(g_hAppsLv, i, &col);
        }
    }

    // Tab 1: Proxy Rules Management (32px row height ListView + inline action bar)
    g_hRulesLv = CreateWindowExW(0, WC_LISTVIEWW, NULL,
                                 WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                 0, 0, 0, 0, mainHwnd, (HMENU)(UINT_PTR)IDC_MOD_RULES_LV, hInst, NULL);
    if (g_hRulesLv) {
        ListView_SetExtendedListViewStyle(g_hRulesLv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        if (g_hRowImgList) ListView_SetImageList(g_hRulesLv, g_hRowImgList, LVSIL_SMALL);
        SendMessageW(g_hRulesLv, WM_SETFONT, (WPARAM)g_hUiFont, TRUE);
        static const int rw[8] = { 44, 78, 160, 200, 180, 86, 66, 92 };
        LVCOLUMNW col; ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        for (int i = 0; i < 8; i++) {
            col.pszText = L"";
            col.cx      = MSc(rw[i]);
            ListView_InsertColumn(g_hRulesLv, i, &col);
        }
    }

    static const int btnIds[7] = {
        IDC_MOD_RULE_ADD, IDC_MOD_RULE_EDIT, IDC_MOD_RULE_TOGGLE,
        IDC_MOD_RULE_UP, IDC_MOD_RULE_DOWN, IDC_MOD_RULE_DEL, IDC_MOD_RULE_SERVERS
    };
    static const ModernBtnVariant btnVars[7] = {
        MODERN_BTN_PRIMARY, MODERN_BTN_DEFAULT, MODERN_BTN_DEFAULT,
        MODERN_BTN_DEFAULT, MODERN_BTN_DEFAULT, MODERN_BTN_DANGER, MODERN_BTN_DEFAULT
    };
    for (int i = 0; i < 7; i++) {
        g_hRuleBtns[i] = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                                         0, 0, 0, 0, mainHwnd, (HMENU)(UINT_PTR)btnIds[i], hInst, NULL);
        if (g_hRuleBtns[i]) {
            SendMessageW(g_hRuleBtns[i], WM_SETFONT, (WPARAM)g_hUiFont, TRUE);
            ModernMain_StyleButton(g_hRuleBtns[i], btnVars[i]);
        }
    }

    // Tab 2 quick button to open Telemetry / OSD / Capsule settings
    g_hLogsStatsBtn = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                                      0, 0, 0, 0, mainHwnd, (HMENU)(UINT_PTR)IDC_MOD_LOGS_STATS, hInst, NULL);
    if (g_hLogsStatsBtn) {
        SendMessageW(g_hLogsStatsBtn, WM_SETFONT, (WPARAM)g_hUiFont, TRUE);
        ModernMain_StyleButton(g_hLogsStatsBtn, MODERN_BTN_ACCENT_SOFT);
    }

    UpdateLocalizedLabels();
    ModernMain_ApplyTheme(mainHwnd, g_isDark);
    UpdateTabControlVisibility();
    StatsManager_AddListener(OnStatsUpdated, NULL);
    return TRUE;
}

void ModernMain_Shutdown(void)
{
    StatsManager_RemoveListener(OnStatsUpdated, NULL);
    D2dHwnd_Destroy(&g_hdrSurf);
    if (g_hHeader) { DestroyWindow(g_hHeader); g_hHeader = NULL; }
    if (g_hAppsLv) { DestroyWindow(g_hAppsLv); g_hAppsLv = NULL; }
    if (g_hRulesLv) { DestroyWindow(g_hRulesLv); g_hRulesLv = NULL; }
    for (int i = 0; i < 7; i++) {
        if (g_hRuleBtns[i]) { DestroyWindow(g_hRuleBtns[i]); g_hRuleBtns[i] = NULL; }
    }
    if (g_hLogsStatsBtn) { DestroyWindow(g_hLogsStatsBtn); g_hLogsStatsBtn = NULL; }
    if (g_hRowImgList) { ImageList_Destroy(g_hRowImgList); g_hRowImgList = NULL; }
    if (g_hUiFont)   { DeleteObject(g_hUiFont);   g_hUiFont = NULL; }
    if (g_hMonoFont) { DeleteObject(g_hMonoFont); g_hMonoFont = NULL; }
    if (g_hdrClassReg && g_hInst) {
        UnregisterClassW(MODERN_HEADER_CLASS, g_hInst);
        g_hdrClassReg = FALSE;
    }
}

void ModernMain_Layout(HWND mainHwnd, int clientW, int clientH, int *outTab2ContentTop)
{
    (void)mainHwnd;
    int hdrH = 168; // 48px status bar + 10px + 64px cards + 10px + 34px tab strip + 2px
    int pad  = 14;
    if (g_hHeader) MoveWindow(g_hHeader, 0, 0, clientW, hdrH, TRUE);

    int bodyY = hdrH + 8;
    int bodyH = clientH - bodyY - 12;
    if (bodyH < 0) bodyH = 0;
    int bodyW = clientW - pad * 2;
    if (bodyW < 0) bodyW = 0;

    // Tab 0: Active Apps Monitor (inset cleanly inside a rounded SPA card)
    int cardInset = 6;
    SetRect(&g_rcAppsCard, pad, bodyY, pad + bodyW, bodyY + bodyH);
    if (g_hAppsLv) {
        int lw = bodyW - cardInset * 2; if (lw < 0) lw = 0;
        int lh = bodyH - cardInset * 2; if (lh < 0) lh = 0;
        MoveWindow(g_hAppsLv, pad + cardInset, bodyY + cardInset, lw, lh, TRUE);
    }

    // Tab 1: Proxy Rules Toolbar + ListView (inset cleanly inside a rounded SPA card)
    int tbH = MSc(32);
    int bx = pad;
    static const int bw[7] = { 100, 68, 96, 62, 62, 68, 130 };
    for (int i = 0; i < 7; i++) {
        int wBtn = MSc(bw[i]);
        if (i == 6) {
            int rightX = pad + bodyW - wBtn;
            if (rightX > bx + 8) bx = rightX;
        }
        if (g_hRuleBtns[i]) MoveWindow(g_hRuleBtns[i], bx, bodyY, wBtn, tbH, TRUE);
        bx += wBtn + 8;
    }
    int rulesTop = bodyY + tbH + 10;
    int rulesH   = clientH - rulesTop - 12;
    if (rulesH < 0) rulesH = 0;
    SetRect(&g_rcRulesCard, pad, rulesTop, pad + bodyW, rulesTop + rulesH);
    if (g_hRulesLv) {
        int lw = bodyW - cardInset * 2; if (lw < 0) lw = 0;
        int lh = rulesH - cardInset * 2; if (lh < 0) lh = 0;
        MoveWindow(g_hRulesLv, pad + cardInset, rulesTop + cardInset, lw, lh, TRUE);
    }

    // Tab 2: Quick settings pill button at top-right of Logs strip
    int statsBtnW = MSc(228);
    if (g_hLogsStatsBtn) {
        MoveWindow(g_hLogsStatsBtn, pad + bodyW - statsBtnW, bodyY, statsBtnW, tbH, TRUE);
    }

    if (outTab2ContentTop) *outTab2ContentTop = bodyY;
}

void ModernMain_SetTab(ModernMainTab tab)
{
    g_curTab = tab;
    UpdateTabControlVisibility();
    if (g_hHeader) InvalidateRect(g_hHeader, NULL, FALSE);
    if (g_mainHwnd) InvalidateRect(g_mainHwnd, NULL, TRUE);
}

ModernMainTab ModernMain_GetTab(void)
{
    return g_curTab;
}

void ModernMain_SetState(const ModernUiConfig *cfg, BOOL proxyEnabled,
                         const wchar_t *profileName, const wchar_t *versionStr, BOOL zhLang)
{
    if (cfg) g_cfg = *cfg;
    g_proxyEnabled = proxyEnabled;
    if (profileName) lstrcpynW(g_profileName, profileName, ARRAYSIZE(g_profileName));
    if (versionStr)  lstrcpynW(g_versionStr,  versionStr,  ARRAYSIZE(g_versionStr));
    BOOL langChanged = (g_zhLang != zhLang);
    g_zhLang = zhLang;
    if (langChanged) UpdateLocalizedLabels();
    if (g_hHeader) InvalidateRect(g_hHeader, NULL, FALSE);
}

void ModernMain_ApplyTheme(HWND mainHwnd, BOOL isDark)
{
    g_isDark = isDark;
    D2d_ApplyWindowDwm(mainHwnd, isDark, TRUE, TRUE);
    StyleListViewColors(g_hAppsLv, isDark);
    StyleListViewColors(g_hRulesLv, isDark);
    for (int i = 0; i < 7; i++) {
        if (g_hRuleBtns[i]) InvalidateRect(g_hRuleBtns[i], NULL, FALSE);
    }
    if (g_hLogsStatsBtn) InvalidateRect(g_hLogsStatsBtn, NULL, FALSE);
    if (g_hHeader) InvalidateRect(g_hHeader, NULL, FALSE);
}

void ModernMain_RefreshRules(const PBProfile *profile)
{
    g_lastProfile = profile;
    if (!g_hRulesLv || !profile) return;

    int prevSel = ModernMain_GetSelectedRuleIndex();
    SendMessageW(g_hRulesLv, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_hRulesLv);

    g_ruleCountCached = profile->ruleCount;
    if (g_ruleCountCached > PB_MAX_RULE) g_ruleCountCached = PB_MAX_RULE;

    for (int i = 0; i < g_ruleCountCached; i++) {
        const PBRule *r = &profile->rule[i];
        g_ruleEnabled[i] = r->enabled ? 1 : 0;
        g_ruleActions[i] = (_wcsicmp(r->action, L"BLOCK") == 0) ? 2 :
                           (_wcsicmp(r->action, L"DIRECT") == 0) ? 1 : 0;

        wchar_t idxBuf[16];
        _snwprintf_s(idxBuf, ARRAYSIZE(idxBuf), _TRUNCATE, L"%d", i + 1);

        LVITEMW it; ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = i; it.pszText = idxBuf;
        ListView_InsertItem(g_hRulesLv, &it);

        ListView_SetItemText(g_hRulesLv, i, 1, (LPWSTR)(r->enabled ? (g_zhLang ? L"\x25CF 启用" : L"\x25CF On")
                                                                   : (g_zhLang ? L"\x25CB 禁用" : L"\x25CB Off")));
        ListView_SetItemText(g_hRulesLv, i, 2, (LPWSTR)(r->name[0] ? r->name : L"-"));
        ListView_SetItemText(g_hRulesLv, i, 3, (LPWSTR)r->proc);
        ListView_SetItemText(g_hRulesLv, i, 4, (LPWSTR)r->hosts);
        ListView_SetItemText(g_hRulesLv, i, 5, (LPWSTR)r->ports);
        ListView_SetItemText(g_hRulesLv, i, 6, (LPWSTR)r->proto);
        ListView_SetItemText(g_hRulesLv, i, 7, (LPWSTR)r->action);
    }

    if (prevSel >= 0 && prevSel < g_ruleCountCached) {
        ListView_SetItemState(g_hRulesLv, prevSel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    SendMessageW(g_hRulesLv, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hRulesLv, NULL, TRUE);
}

int ModernMain_GetSelectedRuleIndex(void)
{
    if (!g_hRulesLv) return -1;
    return ListView_GetNextItem(g_hRulesLv, -1, LVNI_SELECTED);
}

static void FormatAppCell(const ModernAppTraffic *a, int col, wchar_t *out, int cch)
{
    if (!a || !out || cch <= 0) return;
    out[0] = L'\0';
    switch (col) {
    case 0:
        if (a->treePidCount > 1) {
            _snwprintf_s(out, cch, _TRUNCATE, L"%s (PID %lu, %u proc%s)",
                         a->appName, (unsigned long)a->hostPid, a->treePidCount,
                         a->isForeground ? L", FG" : L"");
        } else {
            _snwprintf_s(out, cch, _TRUNCATE, L"%s (PID %lu%s)",
                         a->appName, (unsigned long)a->hostPid,
                         a->isForeground ? L", FG" : L"");
        }
        break;
    case 1:
        lstrcpynW(out, a->ruleName[0] ? a->ruleName : L"Proxy", cch);
        break;
    case 2:
        ModernFormatSpeedW(a->rxBytesPerSec, FALSE, out, (size_t)cch);
        break;
    case 3:
        ModernFormatSpeedW(a->txBytesPerSec, FALSE, out, (size_t)cch);
        break;
    case 4:
        ModernFormatBytesW(a->rxBytesTotal + a->txBytesTotal, out, (size_t)cch);
        break;
    case 5:
        if (a->rttMs > 0) _snwprintf_s(out, cch, _TRUNCATE, L"%u ms", a->rttMs);
        else              lstrcpynW(out, L"-- ms", cch);
        break;
    default:
        break;
    }
}

BOOL ModernMain_HandleNotify(LPARAM lp, LRESULT *outResult)
{
    LPNMHDR nh = (LPNMHDR)lp;
    if (!nh) return FALSE;

    if (nh->hwndFrom == g_hAppsLv) {
        switch (nh->code) {
        case LVN_GETDISPINFOW:
        {
            NMLVDISPINFOW *di = (NMLVDISPINFOW *)lp;
            if ((di->item.mask & LVIF_TEXT) && di->item.pszText && di->item.cchTextMax > 0) {
                di->item.pszText[0] = L'\0';
                if (di->item.iItem >= 0 && (uint32_t)di->item.iItem < g_statsCopy.appCount) {
                    FormatAppCell(&g_statsCopy.apps[di->item.iItem], di->item.iSubItem,
                                  di->item.pszText, di->item.cchTextMax);
                }
            }
            if (outResult) *outResult = 0;
            return TRUE;
        }
        case NM_CUSTOMDRAW:
        {
            NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW *)lp;
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                if (outResult) *outResult = CDRF_NOTIFYITEMDRAW;
                return TRUE;
            }
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                if (outResult) *outResult = CDRF_NOTIFYSUBITEMDRAW;
                return TRUE;
            }
            if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                int row = (int)cd->nmcd.dwItemSpec;
                int sub = cd->iSubItem;
                cd->clrTextBk = g_isDark ? MODERN_RGB_CARD_DARK : MODERN_RGB_CARD_LIGHT;
                cd->clrText   = g_isDark ? MODERN_RGB_TEXT_DARK : MODERN_RGB_TEXT_LIGHT;
                if (row >= 0 && (uint32_t)row < g_statsCopy.appCount) {
                    const ModernAppTraffic *a = &g_statsCopy.apps[row];
                    if (sub == 2) cd->clrText = MODERN_RGB_DL;
                    else if (sub == 3) cd->clrText = MODERN_RGB_UL;
                    else if (sub == 5 && a->rttMs > 0) {
                        cd->clrText = (a->rttMs > 150u) ? MODERN_RGB_ERR
                                    : (a->rttMs >= 60u) ? MODERN_RGB_WARN
                                                        : MODERN_RGB_OK;
                    }
                    if (sub >= 2 && g_hMonoFont) SelectObject(cd->nmcd.hdc, g_hMonoFont);
                }
                if (outResult) *outResult = CDRF_NEWFONT;
                return TRUE;
            }
            break;
        }
        }
        return FALSE;
    }

    if (nh->hwndFrom == g_hRulesLv) {
        if (nh->code == NM_DBLCLK) {
            int sel = ModernMain_GetSelectedRuleIndex();
            if (sel >= 0 && g_actionCb) g_actionCb(MODERN_ACT_RULE_EDIT, sel, g_actionUser);
            if (outResult) *outResult = 0;
            return TRUE;
        }
        if (nh->code == NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW *)lp;
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) {
                if (outResult) *outResult = CDRF_NOTIFYITEMDRAW;
                return TRUE;
            }
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                if (outResult) *outResult = CDRF_NOTIFYSUBITEMDRAW;
                return TRUE;
            }
            if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
                int row = (int)cd->nmcd.dwItemSpec;
                int sub = cd->iSubItem;
                cd->clrTextBk = g_isDark ? MODERN_RGB_CARD_DARK : MODERN_RGB_CARD_LIGHT;
                cd->clrText   = g_isDark ? MODERN_RGB_TEXT_DARK : MODERN_RGB_TEXT_LIGHT;
                if (row >= 0 && row < g_ruleCountCached) {
                    if (!g_ruleEnabled[row]) {
                        cd->clrText = g_isDark ? MODERN_RGB_MUTED_DARK : MODERN_RGB_MUTED_LIGHT;
                    } else if (sub == 1) {
                        cd->clrText = MODERN_RGB_OK;
                    } else if (sub == 7) {
                        cd->clrText = (g_ruleActions[row] == 2) ? MODERN_RGB_ERR
                                    : (g_ruleActions[row] == 1) ? MODERN_RGB_MUTED_DARK
                                                                : MODERN_RGB_DL;
                    }
                }
                if (outResult) *outResult = CDRF_NEWFONT;
                return TRUE;
            }
        }
    }
    return FALSE;
}

BOOL ModernMain_HandleCommand(WPARAM wp, LPARAM lp, LRESULT *outResult)
{
    (void)lp;
    if (!g_actionCb) return FALSE;
    WORD id = LOWORD(wp);
    int sel = ModernMain_GetSelectedRuleIndex();
    switch (id) {
    case IDC_MOD_RULE_ADD:     g_actionCb(MODERN_ACT_RULE_ADD,     sel, g_actionUser); break;
    case IDC_MOD_RULE_EDIT:    if (sel >= 0) g_actionCb(MODERN_ACT_RULE_EDIT,   sel, g_actionUser); break;
    case IDC_MOD_RULE_TOGGLE:  if (sel >= 0) g_actionCb(MODERN_ACT_RULE_TOGGLE, sel, g_actionUser); break;
    case IDC_MOD_RULE_UP:      if (sel >  0) g_actionCb(MODERN_ACT_RULE_UP,     sel, g_actionUser); break;
    case IDC_MOD_RULE_DOWN:    if (sel >= 0) g_actionCb(MODERN_ACT_RULE_DOWN,   sel, g_actionUser); break;
    case IDC_MOD_RULE_DEL:     if (sel >= 0) g_actionCb(MODERN_ACT_RULE_DELETE, sel, g_actionUser); break;
    case IDC_MOD_RULE_SERVERS: g_actionCb(MODERN_ACT_OPEN_SERVERS, 0,   g_actionUser); break;
    case IDC_MOD_LOGS_STATS:   g_actionCb(MODERN_ACT_OPEN_TELEMETRY, 0, g_actionUser); break;
    default: return FALSE;
    }
    if (outResult) *outResult = 0;
    return TRUE;
}

// ============================================================================
// Unified Web-SPA Styled Native Controls (Rounded Buttons, Segmented Tabs, Cards)
// ============================================================================

#define SUBCLASS_SPA_BTN   ((UINT_PTR)0x50A1)
#define SUBCLASS_SPA_TAB   ((UINT_PTR)0x50A2)
#define PROP_SPA_BTN_STATE L"PbSpaBtnState"
#define PROP_SPA_TAB_HOT   L"PbSpaTabHot"

static LRESULT CALLBACK SpaButtonSubProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    (void)id;
    ModernBtnVariant variant = (ModernBtnVariant)ref;
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        if (IsWindowEnabled(h)) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
    {
        ULONG_PTR st = (ULONG_PTR)GetPropW(h, PROP_SPA_BTN_STATE);
        if ((st & 1u) == 0) {
            SetPropW(h, PROP_SPA_BTN_STATE, (HANDLE)(st | 1u));
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(h, NULL, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE:
    {
        ULONG_PTR st = (ULONG_PTR)GetPropW(h, PROP_SPA_BTN_STATE);
        SetPropW(h, PROP_SPA_BTN_STATE, (HANDLE)(st & ~3u));
        InvalidateRect(h, NULL, FALSE);
        break;
    }
    case WM_LBUTTONDOWN:
    {
        ULONG_PTR st = (ULONG_PTR)GetPropW(h, PROP_SPA_BTN_STATE);
        SetPropW(h, PROP_SPA_BTN_STATE, (HANDLE)(st | 2u));
        InvalidateRect(h, NULL, FALSE);
        break;
    }
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
    {
        ULONG_PTR st = (ULONG_PTR)GetPropW(h, PROP_SPA_BTN_STATE);
        if (st & 2u) {
            SetPropW(h, PROP_SPA_BTN_STATE, (HANDLE)(st & ~2u));
            InvalidateRect(h, NULL, FALSE);
        }
        break;
    }
    case WM_SETTEXT:
    case WM_ENABLE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    {
        LRESULT r = DefSubclassProc(h, m, w, l);
        InvalidateRect(h, NULL, FALSE);
        return r;
    }
    case WM_NCDESTROY:
        RemovePropW(h, PROP_SPA_BTN_STATE);
        RemovePropW(h, L"PbSpaBtnOnCard");
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        float W = (float)(rc.right - rc.left);
        float H = (float)(rc.bottom - rc.top);

        ULONG_PTR st = (ULONG_PTR)GetPropW(h, PROP_SPA_BTN_STATE);
        BOOL hot     = (st & 1u) != 0;
        BOOL down    = (st & 2u) != 0;
        BOOL enabled = IsWindowEnabled(h);

        BOOL onCard = (GetPropW(h, L"PbSpaBtnOnCard") != NULL);
        uint32_t canvasBg = onCard
            ? (g_isDark ? 0xFF232429u : 0xFFFFFFFFu)
            : (g_isDark ? 0xFF1C1C1Fu : 0xFFF3F4F6u);
        uint32_t fillClr, borderClr, textClr;

        if (variant == MODERN_BTN_PRIMARY) {
            fillClr   = down ? 0xFF005A9Eu : (hot ? 0xFF106EBEu : 0xFF0078D4u);
            borderClr = fillClr;
            textClr   = 0xFFFFFFFFu;
        } else if (variant == MODERN_BTN_DANGER) {
            if (g_isDark) {
                fillClr   = down ? 0xFF42272Eu : (hot ? 0xFF352429u : 0xFF281F22u);
                borderClr = hot ? 0xFF7F343Fu : 0xFF562C33u;
                textClr   = 0xFFF87171u;
            } else {
                fillClr   = down ? 0xFFFEE2E2u : (hot ? 0xFFFEF2F2u : 0xFFFFFFFFu);
                borderClr = hot ? 0xFFFCA5A5u : 0xFFF0D4D4u;
                textClr   = 0xFFDC2626u;
            }
        } else if (variant == MODERN_BTN_ACCENT_SOFT) {
            if (g_isDark) {
                fillClr   = down ? 0xFF284A44u : (hot ? 0xFF223E39u : 0xFF1A312Du);
                borderClr = hot ? 0xFF397066u : 0xFF2B564Eu;
                textClr   = 0xFF34D399u;
            } else {
                fillClr   = down ? 0xFFC4E7DDu : (hot ? 0xFFD6EFE8u : 0xFFE8F5F1u);
                borderClr = hot ? 0xFF8CD0BDu : 0xFFBAE2D6u;
                textClr   = 0xFF0F766Eu;
            }
        } else {
            if (g_isDark) {
                fillClr   = down ? 0xFF393C45u : (hot ? 0xFF303239u : 0xFF26272Cu);
                borderClr = hot ? 0xFF4C505Eu : 0xFF3A3D47u;
                textClr   = 0xFFF1F3F6u;
            } else {
                fillClr   = down ? 0xFFE6EBF2u : (hot ? 0xFFF4F6FAu : 0xFFFFFFFFu);
                borderClr = hot ? 0xFFC5CEDBu : 0xFFDDE2EAu;
                textClr   = 0xFF111827u;
            }
        }
        if (!enabled) {
            textClr = g_isDark ? 0xFF666B77u : 0xFF9CA3AFu;
        }

        wchar_t txt[128] = {0};
        GetWindowTextW(h, txt, ARRAYSIZE(txt));

        D2dCanvas c;
        if (D2dDc_Begin(hdc, &rc, &c)) {
            D2d_FillRect(&c, 0.0f, 0.0f, W, H, canvasBg);
            float rad = (H <= 36.0f) ? 8.0f : 10.0f;
            D2d_FillRoundedRect(&c, 0.5f, 0.5f, W - 1.0f, H - 1.0f, rad, rad, fillClr);
            D2d_DrawRoundedRect(&c, 0.5f, 0.5f, W - 1.0f, H - 1.0f, rad, rad, 1.0f, borderClr);
            D2d_DrawText(&c, D2D_FONT_UI_12, txt, 6.0f, 0.0f, W - 12.0f, H, textClr, D2D_ALIGN_CENTER, FALSE);
            D2dDc_End();
        } else {
            HBRUSH br = CreateSolidBrush(g_isDark ? MODERN_RGB_CARD_DARK : MODERN_RGB_CARD_LIGHT);
            FillRect(hdc, &rc, br);
            DeleteObject(br);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, g_isDark ? MODERN_RGB_TEXT_DARK : MODERN_RGB_TEXT_LIGHT);
            DrawTextW(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefSubclassProc(h, m, w, l);
}

void ModernMain_StyleButton(HWND btn, ModernBtnVariant variant)
{
    if (!btn || !IsWindow(btn)) return;
    SetWindowSubclass(btn, SpaButtonSubProc, SUBCLASS_SPA_BTN, (DWORD_PTR)variant);
    InvalidateRect(btn, NULL, FALSE);
}

static LRESULT CALLBACK SpaTabSubProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    (void)id; (void)ref;
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_HAND));
        return TRUE;
    case WM_MOUSEMOVE:
    {
        TCHITTESTINFO ht;
        ht.pt.x  = (SHORT)LOWORD(l);
        ht.pt.y  = (SHORT)HIWORD(l);
        ht.flags = 0;
        int hit  = TabCtrl_HitTest(h, &ht);
        int prev = (int)(INT_PTR)GetPropW(h, PROP_SPA_TAB_HOT) - 1;
        if (hit != prev) {
            SetPropW(h, PROP_SPA_TAB_HOT, (HANDLE)(INT_PTR)(hit + 1));
            InvalidateRect(h, NULL, FALSE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        break;
    }
    case WM_MOUSELEAVE:
        SetPropW(h, PROP_SPA_TAB_HOT, (HANDLE)(INT_PTR)0);
        InvalidateRect(h, NULL, FALSE);
        break;
    case WM_NCDESTROY:
        RemovePropW(h, PROP_SPA_TAB_HOT);
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        float W = (float)(rc.right - rc.left);
        float H = (float)(rc.bottom - rc.top);

        int count = TabCtrl_GetItemCount(h);
        int sel   = TabCtrl_GetCurSel(h);
        int hot   = (int)(INT_PTR)GetPropW(h, PROP_SPA_TAB_HOT) - 1;

        D2dCanvas c;
        if (D2dDc_Begin(hdc, &rc, &c)) {
            uint32_t canvasBg  = g_isDark ? 0xFF1C1C1Fu : 0xFFF3F4F6u;
            uint32_t trackBg   = g_isDark ? 0xFF25262Cu : 0xFFE5E9F0u;
            uint32_t pillSelBg = g_isDark ? 0xFF353741u : 0xFFFFFFFFu;
            uint32_t pillSelBd = g_isDark ? 0xFF474A57u : 0xFFDCE1EAu;
            uint32_t pillHotBg = g_isDark ? 0xFF2D2F38u : 0xFFEDF0F6u;
            uint32_t txtSel    = g_isDark ? 0xFFF4F6FAu : 0xFF111827u;
            uint32_t txtDim    = g_isDark ? 0xFF9BA1AEu : 0xFF6B7280u;

            D2d_FillRect(&c, 0.0f, 0.0f, W, H, canvasBg);

            // Measure tight track width enclosing all segments
            float trackRight = 0.0f;
            for (int i = 0; i < count; i++) {
                RECT ir;
                if (TabCtrl_GetItemRect(h, i, &ir) && (float)ir.right > trackRight)
                    trackRight = (float)ir.right;
            }
            if (trackRight < 40.0f) trackRight = W;
            trackRight += 4.0f;
            if (trackRight > W) trackRight = W;

            D2d_FillRoundedRect(&c, 0.0f, 0.0f, trackRight, H, 9.0f, 9.0f, trackBg);

            for (int i = 0; i < count; i++) {
                RECT ir;
                if (!TabCtrl_GetItemRect(h, i, &ir)) continue;
                float x = (float)ir.left + 2.0f;
                float y = 3.0f;
                float wPill = (float)(ir.right - ir.left) - 2.0f;
                float hPill = H - 6.0f;
                if (wPill < 8.0f || hPill < 8.0f) continue;

                BOOL isSel = (i == sel);
                BOOL isHot = (i == hot);
                if (isSel) {
                    D2d_FillRoundedRect(&c, x, y, wPill, hPill, 7.0f, 7.0f, pillSelBg);
                    D2d_DrawRoundedRect(&c, x, y, wPill, hPill, 7.0f, 7.0f, 1.0f, pillSelBd);
                } else if (isHot) {
                    D2d_FillRoundedRect(&c, x, y, wPill, hPill, 7.0f, 7.0f, pillHotBg);
                }

                wchar_t txt[64] = {0};
                TCITEMW ti;
                ti.mask = TCIF_TEXT;
                ti.pszText = txt;
                ti.cchTextMax = ARRAYSIZE(txt);
                TabCtrl_GetItem(h, i, &ti);

                D2d_DrawText(&c, D2D_FONT_UI_12, txt, x + 2.0f, y, wPill - 4.0f, hPill,
                             isSel ? txtSel : (isHot ? txtSel : txtDim), D2D_ALIGN_CENTER, FALSE);
            }
            D2dDc_End();
        }
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefSubclassProc(h, m, w, l);
}

void ModernMain_StyleTabStrip(HWND tab)
{
    if (!tab || !IsWindow(tab)) return;
    TabCtrl_SetPadding(tab, MSc(16), MSc(6));
    SetWindowSubclass(tab, SpaTabSubProc, SUBCLASS_SPA_TAB, 0);
    InvalidateRect(tab, NULL, FALSE);
}

void ModernMain_PaintCardDC(HDC hdc, const RECT *rc, float radius, BOOL focused)
{
    if (!hdc || !rc) return;
    float W = (float)(rc->right - rc->left);
    float H = (float)(rc->bottom - rc->top);
    if (W <= 4.0f || H <= 4.0f) return;

    D2dCanvas c;
    if (!D2dDc_Begin(hdc, rc, &c)) return;

    uint32_t canvasBg  = g_isDark ? 0xFF1C1C1Fu : 0xFFF3F4F6u;
    uint32_t cardBg    = g_isDark ? 0xFF232429u : 0xFFFFFFFFu;
    uint32_t borderClr = focused
        ? 0xFF0078D4u
        : (g_isDark ? 0xFF333640u : 0xFFE2E6ECu);
    float stroke = focused ? 1.5f : 1.0f;

    D2d_FillRect(&c, 0.0f, 0.0f, W, H, canvasBg);
    D2d_FillRoundedRect(&c, 0.5f, 0.5f, W - 1.0f, H - 1.0f, radius, radius, cardBg);
    D2d_DrawRoundedRect(&c, 0.5f, 0.5f, W - 1.0f, H - 1.0f, radius, radius, stroke, borderClr);
    D2dDc_End();
}

void ModernMain_PaintMenuPillDC(HDC hdc, const RECT *rc, BOOL hot, BOOL dimmed)
{
    (void)dimmed;
    if (!hdc || !rc) return;
    float W = (float)(rc->right - rc->left);
    float H = (float)(rc->bottom - rc->top);
    if (W <= 2.0f || H <= 2.0f) return;

    D2dCanvas c;
    if (!D2dDc_Begin(hdc, rc, &c)) return;

    uint32_t barBg  = g_isDark ? 0xFF202024u : 0xFFF3F4F6u;
    uint32_t pillBg = g_isDark ? 0xFF32343Cu : 0xFFE3E7EEu;

    D2d_FillRect(&c, 0.0f, 0.0f, W, H, barBg);
    if (hot && W > 8.0f && H > 8.0f) {
        D2d_FillRoundedRect(&c, 2.0f, 3.0f, W - 4.0f, H - 6.0f, 6.0f, 6.0f, pillBg);
    }
    D2dDc_End();
}

void ModernMain_PaintActiveBodyCards(HDC hdc, const RECT *tab2SearchCard, BOOL searchVisible,
                                     BOOL searchFocused, const RECT *tab2LogCard)
{
    if (!hdc) return;
    if (g_curTab == MODERN_MAIN_TAB_APPS) {
        ModernMain_PaintCardDC(hdc, &g_rcAppsCard, 10.0f, FALSE);
    } else if (g_curTab == MODERN_MAIN_TAB_RULES) {
        ModernMain_PaintCardDC(hdc, &g_rcRulesCard, 10.0f, FALSE);
    } else if (g_curTab == MODERN_MAIN_TAB_LOGS) {
        if (searchVisible && tab2SearchCard) {
            ModernMain_PaintCardDC(hdc, tab2SearchCard, 8.0f, searchFocused);
        }
        if (tab2LogCard) {
            ModernMain_PaintCardDC(hdc, tab2LogCard, 10.0f, FALSE);
        }
    }
}

static BOOL WildcardMatchW(const wchar_t *pat, const wchar_t *str)
{
    const wchar_t *star = NULL, *ss = str;
    while (*str) {
        if (*pat == L'*') { star = pat++; ss = str; }
        else if (*pat == L'?' || towlower(*pat) == towlower(*str)) { pat++; str++; }
        else if (star) { pat = star + 1; str = ++ss; }
        else return FALSE;
    }
    while (*pat == L'*') pat++;
    return *pat == L'\0';
}

void ModernMain_ResolveRuleForApp(const PBProfile *profile, const wchar_t *appName,
                                  wchar_t *ruleOut, size_t ruleCch)
{
    if (!ruleOut || ruleCch == 0) return;
    lstrcpynW(ruleOut, L"Proxy", (int)ruleCch);
    if (!profile || !appName || !appName[0]) return;

    for (int i = 0; i < profile->ruleCount; i++) {
        const PBRule *r = &profile->rule[i];
        if (!r->enabled) continue;
        if (_wcsicmp(r->action, L"PROXY") != 0) continue;

        // Check semicolon-separated process patterns
        const wchar_t *p = r->proc;
        while (*p) {
            while (*p == L';' || *p == L',' || *p == L' ') p++;
            if (!*p) break;
            const wchar_t *s = p;
            while (*p && *p != L';' && *p != L',') p++;
            const wchar_t *e = p;
            while (e > s && e[-1] == L' ') e--;
            size_t len = (size_t)(e - s);
            if (len > 0 && len < 128) {
                wchar_t tok[128];
                memcpy(tok, s, len * sizeof(wchar_t));
                tok[len] = L'\0';
                // Strip directory prefix if pattern has a backslash
                const wchar_t *baseTok = wcsrchr(tok, L'\\');
                baseTok = baseTok ? (baseTok + 1) : tok;
                if (WildcardMatchW(baseTok, appName)) {
                    lstrcpynW(ruleOut, r->name[0] ? r->name : tok, (int)ruleCch);
                    return;
                }
            }
        }
    }
}
