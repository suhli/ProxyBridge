#ifndef MODERN_TYPES_H
#define MODERN_TYPES_H

#include <windows.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include "../../src/pid_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Global Design Tokens (UI / UE Spec)
// ============================================================================

// Surfaces (85% opacity -> alpha 0xD9)
#define MODERN_COLOR_BG_DARK_ARGB       0xD918181Bu
#define MODERN_COLOR_BG_LIGHT_ARGB      0xD9F4F4F5u
#define MODERN_COLOR_CARD_DARK_ARGB     0xCC222227u
#define MODERN_COLOR_CARD_LIGHT_ARGB    0xE6FFFFFFu
#define MODERN_COLOR_BORDER_DARK_ARGB   0x1FFFFFFFu  // rgba(255, 255, 255, 0.12)
#define MODERN_COLOR_BORDER_LIGHT_ARGB  0x1A000000u  // rgba(0, 0, 0, 0.10)

// Text
#define MODERN_COLOR_TEXT_DARK_ARGB     0xFFF4F4F5u
#define MODERN_COLOR_MUTED_DARK_ARGB    0xFFA1A1AAu
#define MODERN_COLOR_TEXT_LIGHT_ARGB    0xFF18181Bu
#define MODERN_COLOR_MUTED_LIGHT_ARGB   0xFF52525Bu

// Semantic status & telemetry colors
#define MODERN_COLOR_OK_ARGB            0xFF10B981u  // Emerald green: running / RTT < 60ms
#define MODERN_COLOR_WARN_ARGB          0xFFF59E0Bu  // Amber: RTT 60ms ~ 150ms
#define MODERN_COLOR_ERR_ARGB           0xFFEF4444u  // Coral red: paused / RTT > 150ms
#define MODERN_COLOR_DL_ARGB            0xFF38BDF8u  // Sky blue: download rate
#define MODERN_COLOR_UL_ARGB            0xFFA78BFAu  // Lavender: upload rate

// Win32 COLORREF (0x00BBGGRR) equivalents for GDI / ListView interoperability
#define MODERN_RGB_BG_DARK              RGB(0x18, 0x18, 0x1B)
#define MODERN_RGB_BG_LIGHT             RGB(0xF4, 0xF4, 0xF5)
#define MODERN_RGB_CARD_DARK            RGB(0x22, 0x22, 0x27)
#define MODERN_RGB_CARD_LIGHT           RGB(0xFF, 0xFF, 0xFF)
#define MODERN_RGB_TEXT_DARK            RGB(0xF4, 0xF4, 0xF5)
#define MODERN_RGB_MUTED_DARK           RGB(0xA1, 0xA1, 0xAA)
#define MODERN_RGB_TEXT_LIGHT           RGB(0x18, 0x18, 0x1B)
#define MODERN_RGB_MUTED_LIGHT          RGB(0x52, 0x52, 0x5B)
#define MODERN_RGB_OK                   RGB(0x10, 0xB9, 0x81)
#define MODERN_RGB_WARN                 RGB(0xF5, 0x9E, 0x0B)
#define MODERN_RGB_ERR                  RGB(0xEF, 0x44, 0x44)
#define MODERN_RGB_DL                   RGB(0x38, 0xBD, 0xF8)
#define MODERN_RGB_UL                   RGB(0xA7, 0x8B, 0xFA)

// Typography
#define MODERN_FONT_UI                  L"Segoe UI Variable Text"
#define MODERN_FONT_UI_FALLBACK         L"Segoe UI"
#define MODERN_FONT_MONO                L"Cascadia Mono"
#define MODERN_FONT_MONO_FALLBACK       L"Consolas"

// Geometry constants
#define MODERN_CAPSULE_WIDTH            184
#define MODERN_CAPSULE_HEIGHT           32
#define MODERN_CAPSULE_RADIUS           16
#define MODERN_CAPSULE_SNAP_PX          16
#define MODERN_OSD_HEIGHT               22
#define MODERN_OSD_DEFAULT_OFFSET_X     12
#define MODERN_OSD_DEFAULT_OFFSET_Y     10
#define MODERN_POS_UNSET                INT_MIN

// Data limits
#define MODERN_HISTORY_LEN              30
#define MODERN_MAX_APPS                 MAX_TRACKED_PIDS
#define MODERN_MAX_PARENT_DEPTH         5

typedef enum {
    MODERN_THEME_SYSTEM = 0,
    MODERN_THEME_DARK   = 1,
    MODERN_THEME_LIGHT  = 2
} ModernThemeMode;

typedef struct {
    int  themeMode;         // ModernThemeMode (0 = follow system, 1 = dark, 2 = light)
    BOOL capsuleEnabled;    // Desktop floating capsule visible
    BOOL capsuleTopmost;    // Desktop floating capsule always-on-top
    int  capsuleX;          // Saved screen X (MODERN_POS_UNSET = auto)
    int  capsuleY;          // Saved screen Y (MODERN_POS_UNSET = auto)
    BOOL osdEnabled;        // Master switch for top-left dual-mode OSD
    BOOL osdCompactMode;    // FALSE = standard, TRUE = compact template
    BOOL rtssEnabled;       // Enable Channel A (RTSS SharedMemoryV2) when hooked
    int  osdOffsetX;        // Channel B client-area X offset (default +12px)
    int  osdOffsetY;        // Channel B client-area Y offset (default +10px)
} ModernUiConfig;

typedef struct {
    DWORD    hostPid;           // Aggregated top-level host process PID
    wchar_t  appName[128];      // Process executable name (UTF-16)
    wchar_t  ruleName[128];     // Matched rule name (UTF-16, optional)
    uint64_t rxBytesTotal;      // Cumulative session download bytes (tree-aggregated)
    uint64_t txBytesTotal;      // Cumulative session upload bytes (tree-aggregated)
    uint64_t rxBytesPerSec;     // Real-time download speed (bytes/sec)
    uint64_t txBytesPerSec;     // Real-time upload speed (bytes/sec)
    uint32_t rttMs;             // Latest TCP connect RTT (ms)
    uint32_t activeConns;       // Current active proxied TCP connections
    uint32_t treePidCount;      // Number of raw PIDs aggregated into this host
    BOOL     isForeground;      // TRUE if this hostPid owns the foreground window
} ModernAppTraffic;

typedef struct {
    ULONGLONG        timestampMs;
    BOOL             proxyEnabled;
    uint64_t         globalRxBps;
    uint64_t         globalTxBps;
    uint64_t         globalRxTotal;
    uint64_t         globalTxTotal;
    uint32_t         totalActiveConns;
    uint32_t         globalRttMs;
    DWORD            foregroundPid;
    HWND             foregroundHwnd;
    wchar_t          foregroundAppName[128];
    BOOL             foregroundMatched;
    ModernAppTraffic foregroundStats;
    uint32_t         appCount;
    ModernAppTraffic apps[MODERN_MAX_APPS];
    uint64_t         rxHistory[MODERN_HISTORY_LEN]; // oldest [0] .. newest [29]
    uint64_t         txHistory[MODERN_HISTORY_LEN];
} ModernStatsView;

static __inline void ModernUiConfig_Defaults(ModernUiConfig *cfg)
{
    if (!cfg) return;
    cfg->themeMode      = MODERN_THEME_SYSTEM;
    cfg->capsuleEnabled = TRUE;
    cfg->capsuleTopmost = TRUE;
    cfg->capsuleX       = MODERN_POS_UNSET;
    cfg->capsuleY       = MODERN_POS_UNSET;
    cfg->osdEnabled     = TRUE;
    cfg->osdCompactMode = FALSE;
    cfg->rtssEnabled    = TRUE;
    cfg->osdOffsetX     = MODERN_OSD_DEFAULT_OFFSET_X;
    cfg->osdOffsetY     = MODERN_OSD_DEFAULT_OFFSET_Y;
}

static __inline uint32_t ModernStatusColorArgb(BOOL proxyEnabled, uint32_t rttMs)
{
    if (!proxyEnabled) return MODERN_COLOR_ERR_ARGB;
    if (rttMs > 150u)  return MODERN_COLOR_ERR_ARGB;
    if (rttMs >= 60u)  return MODERN_COLOR_WARN_ARGB;
    return MODERN_COLOR_OK_ARGB;
}

static __inline void ModernFormatSpeedW(uint64_t bps, BOOL compact, wchar_t *out, size_t cch)
{
    if (!out || cch == 0) return;
    if (bps < 1024ULL) {
        _snwprintf_s(out, cch, _TRUNCATE, compact ? L"%lluB/s" : L"%llu B/s", (unsigned long long)bps);
    } else if (bps < 1024ULL * 1024ULL) {
        double kb = (double)bps / 1024.0;
        if (kb >= 100.0)
            _snwprintf_s(out, cch, _TRUNCATE, compact ? L"%.0fK/s" : L"%.0f KB/s", kb);
        else
            _snwprintf_s(out, cch, _TRUNCATE, compact ? L"%.1fK/s" : L"%.1f KB/s", kb);
    } else if (bps < 1024ULL * 1024ULL * 1024ULL) {
        double mb = (double)bps / (1024.0 * 1024.0);
        if (mb >= 100.0)
            _snwprintf_s(out, cch, _TRUNCATE, compact ? L"%.0fM/s" : L"%.0f MB/s", mb);
        else
            _snwprintf_s(out, cch, _TRUNCATE, compact ? L"%.1fM/s" : L"%.1f MB/s", mb);
    } else {
        double gb = (double)bps / (1024.0 * 1024.0 * 1024.0);
        _snwprintf_s(out, cch, _TRUNCATE, compact ? L"%.2fG/s" : L"%.2f GB/s", gb);
    }
}

static __inline void ModernFormatBytesW(uint64_t bytes, wchar_t *out, size_t cch)
{
    if (!out || cch == 0) return;
    if (bytes < 1024ULL) {
        _snwprintf_s(out, cch, _TRUNCATE, L"%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024ULL * 1024ULL) {
        _snwprintf_s(out, cch, _TRUNCATE, L"%.1f KB", (double)bytes / 1024.0);
    } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        _snwprintf_s(out, cch, _TRUNCATE, L"%.1f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        _snwprintf_s(out, cch, _TRUNCATE, L"%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

#ifdef __cplusplus
}
#endif

#endif // MODERN_TYPES_H
