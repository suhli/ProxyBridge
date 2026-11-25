#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "overlay_dispatcher.h"
#include "stats_manager.h"
#include "rtss_provider.h"
#include "window_osd.h"
#include <string.h>

static BOOL           g_dispInit = FALSE;
static ModernUiConfig g_dispCfg;
static OverlayChannel g_activeChannel = OVERLAY_CHANNEL_NONE;

static void DisableAllChannels(void)
{
    RtssProvider_Clear();
    WindowOsd_Hide();
    g_activeChannel = OVERLAY_CHANNEL_NONE;
}

// Mutual-exclusion arbiter:
//   - If foreground PID is hooked by RTSS -> Channel A (RTSS) ONLY; native OSD MUST be hidden.
//   - Otherwise                           -> RTSS slot MUST be emptied; Channel B (native OSD) ONLY.
static void ArbitrateForTarget(HWND targetHwnd, DWORD fgPid, const ModernAppTraffic *app, BOOL proxyEnabled)
{
    if ((!g_dispCfg.osdEnabled && !g_dispCfg.rtssEnabled) || !app || fgPid == 0 || fgPid == GetCurrentProcessId()) {
        DisableAllChannels();
        return;
    }

    // Do not display OSD / RTSS for applications not matched by any enabled PROXY rule.
    if (!StatsManager_IsAppInProxyRules(app->appName, NULL, 0)) {
        DisableAllChannels();
        return;
    }

    // Channel A: RTSS SharedMemoryV2 (DirectX / Vulkan / OpenGL fullscreen & 3D apps)
    if (g_dispCfg.rtssEnabled && RtssProvider_IsPidHooked(fgPid)) {
        WindowOsd_Hide(); // Guarantee zero dual-layer ghosting
        if (RtssProvider_Update(app, proxyEnabled, g_dispCfg.osdCompactMode)) {
            g_activeChannel = OVERLAY_CHANNEL_RTSS;
            return;
        }
        // Fallback to Channel B only if RTSS had no free OSD slot
        RtssProvider_Clear();
    } else {
        // Not hooked by RTSS: guarantee RTSS slot is immediately emptied
        RtssProvider_Clear();
    }

    // Channel B: Win32 Direct2D click-through companion micro-capsule (only when OSD toggle is ON)
    if (!g_dispCfg.osdEnabled) {
        WindowOsd_Hide();
        g_activeChannel = OVERLAY_CHANNEL_NONE;
        return;
    }
    WindowOsd_Update(targetHwnd, app, proxyEnabled, &g_dispCfg);
    g_activeChannel = WindowOsd_IsVisible() ? OVERLAY_CHANNEL_NATIVE : OVERLAY_CHANNEL_NONE;
}

static void OnStatsUpdated(const ModernStatsView *view, void *user)
{
    (void)user;
    if (!g_dispInit || !view) return;
    if (!view->foregroundMatched) {
        DisableAllChannels();
        return;
    }
    ArbitrateForTarget(view->foregroundHwnd, view->foregroundPid, &view->foregroundStats, view->proxyEnabled);
}

static void OnForegroundChanged(HWND newFgHwnd, void *user)
{
    (void)user;
    if (!g_dispInit) return;

    HWND fgHwnd = NULL;
    DWORD fgPid = StatsManager_GetForegroundAppPid(&fgHwnd);
    if (!fgHwnd) fgHwnd = newFgHwnd;

    if ((!g_dispCfg.osdEnabled && !g_dispCfg.rtssEnabled) || fgPid == 0 || fgPid == GetCurrentProcessId()) {
        DisableAllChannels();
        return;
    }

    const ModernStatsView *latest = StatsManager_GetLatest();
    if (!latest) {
        DisableAllChannels();
        return;
    }

    // Check if the newly focused app is already tracked in the latest aggregated view
    const ModernAppTraffic *matched = NULL;
    for (uint32_t i = 0; i < latest->appCount; i++) {
        if (latest->apps[i].hostPid == fgPid) {
            matched = &latest->apps[i];
            break;
        }
    }
    if (!matched && latest->foregroundMatched && latest->foregroundStats.hostPid == fgPid) {
        matched = &latest->foregroundStats;
    }

    if (matched) {
        ArbitrateForTarget(fgHwnd, fgPid, matched, latest->proxyEnabled);
    } else {
        DisableAllChannels();
    }
}

BOOL OverlayDispatcher_Init(HINSTANCE hInst, const ModernUiConfig *initialCfg)
{
    if (g_dispInit) return TRUE;

    if (initialCfg) {
        g_dispCfg = *initialCfg;
    } else {
        ModernUiConfig_Defaults(&g_dispCfg);
    }

    RtssProvider_Init();
    if (!WindowOsd_Init(hInst, OnForegroundChanged, NULL)) {
        RtssProvider_Shutdown();
        return FALSE;
    }

    g_activeChannel = OVERLAY_CHANNEL_NONE;
    g_dispInit = TRUE;
    StatsManager_AddListener(OnStatsUpdated, NULL);
    return TRUE;
}

void OverlayDispatcher_Shutdown(void)
{
    if (!g_dispInit) return;
    g_dispInit = FALSE;
    StatsManager_RemoveListener(OnStatsUpdated, NULL);
    RtssProvider_Shutdown();
    WindowOsd_Shutdown();
    g_activeChannel = OVERLAY_CHANNEL_NONE;
}

void OverlayDispatcher_SetConfig(const ModernUiConfig *cfg)
{
    if (!cfg) return;
    g_dispCfg = *cfg;
    if (!g_dispInit) return;

    if (!g_dispCfg.osdEnabled && !g_dispCfg.rtssEnabled) {
        DisableAllChannels();
    } else {
        OnStatsUpdated(StatsManager_GetLatest(), NULL);
    }
}

void OverlayDispatcher_GetConfig(ModernUiConfig *outCfg)
{
    if (!outCfg) return;
    *outCfg = g_dispCfg;
}

OverlayChannel OverlayDispatcher_GetActiveChannel(void)
{
    return g_activeChannel;
}
