#ifndef STATS_MANAGER_H
#define STATS_MANAGER_H

#include "modern_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STATS_MAX_LISTENERS 8
#define STATS_POLL_INTERVAL_MS 1000

typedef BOOL (*StatsFetchSnapshotFn)(PidStatsSnapshot *out);
typedef void (*StatsUpdateCallback)(const ModernStatsView *view, void *user);
typedef void (*StatsRuleResolverFn)(const wchar_t *appName, wchar_t *ruleOut, size_t ruleCch, void *user);

void StatsManager_Init(void);
void StatsManager_Shutdown(void);
void StatsManager_Reset(void);

BOOL StatsManager_AddListener(StatsUpdateCallback cb, void *user);
void StatsManager_RemoveListener(StatsUpdateCallback cb, void *user);
void StatsManager_SetRuleResolver(StatsRuleResolverFn fn, void *user);

// Polls PidStatsSnapshot once (called on 1000ms timer), aggregates process trees,
// updates rates/totals/history, and notifies all registered listeners.
void StatsManager_Tick(StatsFetchSnapshotFn fetchFn, BOOL proxyEnabled);

// Returns pointer to the most recent UI-thread ModernStatsView (valid until next Tick/Shutdown).
const ModernStatsView *StatsManager_GetLatest(void);

// Helper for instant foreground lookup (e.g., WinEventHook in Stage 2):
// resolves the effective foreground PID (including UWP ApplicationFrameHost unwrapping).
DWORD StatsManager_GetForegroundAppPid(HWND *outHwnd);

#ifdef __cplusplus
}
#endif

#endif // STATS_MANAGER_H
