#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "stats_manager.h"
#include <tlhelp32.h>
#include <stdlib.h>
#include <string.h>

#define PROC_SNAP_MAX       2048
#define VISIBLE_WIN_MAX     512
#define RAW_PREV_MAX        256
#define HOST_TOTAL_MAX      256
#define PID_CACHE_EXPIRE_MS 60000ULL

typedef struct {
    DWORD   pid;
    DWORD   parentPid;
    wchar_t exeName[64];
} ProcNode;

typedef struct {
    DWORD     pid;
    uint64_t  lastTx;
    uint64_t  lastRx;
    DWORD     lastHostPid;
    wchar_t   lastExeName[64];
    ULONGLONG lastSeenMs;
} RawPidState;

typedef struct {
    DWORD     hostPid;
    wchar_t   appName[128];
    uint64_t  rxTotal;
    uint64_t  txTotal;
    uint32_t  lastRttMs;
    ULONGLONG lastActiveMs;
} HostAccumState;

typedef struct {
    StatsUpdateCallback cb;
    void               *user;
} StatsListener;

static BOOL            g_initialized = FALSE;
static ULONGLONG       g_prevTickMs = 0;
static uint64_t        g_globalRxTotal = 0;
static uint64_t        g_globalTxTotal = 0;
static uint32_t        g_lastGlobalRtt = 0;
static ModernStatsView g_latestView;

static RawPidState     g_rawPrev[RAW_PREV_MAX];
static HostAccumState  g_hostAccum[HOST_TOTAL_MAX];
static StatsListener   g_listeners[STATS_MAX_LISTENERS];
static StatsRuleResolverFn g_ruleResolver = NULL;
static void           *g_ruleResolverUser = NULL;

// Scratch buffers (allocated once on Init, freed on Shutdown)
static ProcNode       *g_procNodes = NULL;
static DWORD          *g_visibleWinPids = NULL;
static uint32_t        g_visibleWinCount = 0;

// ---- Shell / system stop-boundary process names ----
static BOOL IsStopBoundaryProcess(const wchar_t *exeName)
{
    static const wchar_t *const kBoundaries[] = {
        L"explorer.exe",
        L"svchost.exe",
        L"services.exe",
        L"wininit.exe",
        L"winlogon.exe",
        L"csrss.exe",
        L"smss.exe",
        L"lsass.exe",
        L"sihost.exe",
        L"RuntimeBroker.exe",
        L"ApplicationFrameHost.exe",
        L"StartMenuExperienceHost.exe",
        L"SearchHost.exe",
        L"System",
        L"[System Process]"
    };
    if (!exeName || !exeName[0]) return TRUE;
    for (size_t i = 0; i < ARRAYSIZE(kBoundaries); i++) {
        if (_wcsicmp(exeName, kBoundaries[i]) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL IsConsoleShellHelper(const wchar_t *exeName)
{
    static const wchar_t *const kShells[] = {
        L"conhost.exe",
        L"OpenConsole.exe",
        L"cmd.exe",
        L"pwsh.exe",
        L"powershell.exe",
        L"bash.exe",
        L"wsl.exe",
        L"sh.exe"
    };
    if (!exeName || !exeName[0]) return FALSE;
    for (size_t i = 0; i < ARRAYSIZE(kShells); i++) {
        if (_wcsicmp(exeName, kShells[i]) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL IsKnownTerminalHost(const wchar_t *exeName)
{
    static const wchar_t *const kHosts[] = {
        L"WindowsTerminal.exe",
        L"wezterm-gui.exe",
        L"alacritty.exe",
        L"mintty.exe",
        L"Code.exe",
        L"Cursor.exe",
        L"devenv.exe",
        L"idea64.exe",
        L"clion64.exe",
        L"pycharm64.exe"
    };
    if (!exeName || !exeName[0]) return FALSE;
    for (size_t i = 0; i < ARRAYSIZE(kHosts); i++) {
        if (_wcsicmp(exeName, kHosts[i]) == 0) return TRUE;
    }
    return FALSE;
}

// ---- Visible top-level window PID collection ----
static BOOL CALLBACK EnumVisibleWindowsProc(HWND hwnd, LPARAM lp)
{
    (void)lp;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;

    RECT rc;
    if (!GetWindowRect(hwnd, &rc) || (rc.right - rc.left) <= 0 || (rc.bottom - rc.top) <= 0)
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return TRUE;

    for (uint32_t i = 0; i < g_visibleWinCount; i++) {
        if (g_visibleWinPids[i] == pid) return TRUE;
    }
    if (g_visibleWinCount < VISIBLE_WIN_MAX) {
        g_visibleWinPids[g_visibleWinCount++] = pid;
    }
    return TRUE;
}

static BOOL HasVisibleWindow(DWORD pid)
{
    if (pid == 0 || !g_visibleWinPids) return FALSE;
    for (uint32_t i = 0; i < g_visibleWinCount; i++) {
        if (g_visibleWinPids[i] == pid) return TRUE;
    }
    return FALSE;
}

// ---- UWP ApplicationFrameHost unwrapping for foreground window ----
typedef struct {
    DWORD hostPid;
    DWORD realPid;
} UwpEnumCtx;

static BOOL CALLBACK EnumUwpChildProc(HWND child, LPARAM lp)
{
    UwpEnumCtx *ctx = (UwpEnumCtx *)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(child, &pid);
    if (pid != 0 && pid != ctx->hostPid) {
        ctx->realPid = pid;
        return FALSE;
    }
    return TRUE;
}

static BOOL QueryProcessBaseName(DWORD pid, wchar_t *out, size_t cch)
{
    if (!out || cch == 0) return FALSE;
    out[0] = L'\0';
    if (pid == 0) return FALSE;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    wchar_t full[MAX_PATH];
    DWORD len = ARRAYSIZE(full);
    BOOL ok = QueryFullProcessImageNameW(h, 0, full, &len);
    CloseHandle(h);
    if (!ok || len == 0) return FALSE;
    const wchar_t *slash = wcsrchr(full, L'\\');
    lstrcpynW(out, slash ? (slash + 1) : full, (int)cch);
    return TRUE;
}

DWORD StatsManager_GetForegroundAppPid(HWND *outHwnd)
{
    HWND fg = GetForegroundWindow();
    if (outHwnd) *outHwnd = fg;
    if (!fg) return 0;

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == 0) return 0;

    wchar_t exe[64];
    if (QueryProcessBaseName(pid, exe, ARRAYSIZE(exe)) &&
        _wcsicmp(exe, L"ApplicationFrameHost.exe") == 0) {
        UwpEnumCtx ctx;
        ctx.hostPid = pid;
        ctx.realPid = 0;
        EnumChildWindows(fg, EnumUwpChildProc, (LPARAM)&ctx);
        if (ctx.realPid != 0) pid = ctx.realPid;
    }
    return pid;
}

// ---- Toolhelp32 process snapshot ----
static uint32_t CaptureProcessTable(ProcNode *nodes, uint32_t cap)
{
    if (!nodes || cap == 0) return 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe;
    ZeroMemory(&pe, sizeof(pe));
    pe.dwSize = sizeof(pe);

    uint32_t count = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (count >= cap) break;
            nodes[count].pid       = pe.th32ProcessID;
            nodes[count].parentPid = pe.th32ParentProcessID;
            lstrcpynW(nodes[count].exeName, pe.szExeFile, ARRAYSIZE(nodes[count].exeName));
            count++;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return count;
}

static const ProcNode *FindProcNode(const ProcNode *nodes, uint32_t count, DWORD pid)
{
    for (uint32_t i = 0; i < count; i++) {
        if (nodes[i].pid == pid) return &nodes[i];
    }
    return NULL;
}

// Walks up th32ParentProcessID (up to MODERN_MAX_PARENT_DEPTH = 5 levels, cycle-safe)
// to map CLI child processes (e.g. codex/node/git inside WindowsTerminal.exe) and
// multi-process browser/Electron sub-processes onto their top-level host window PID.
static DWORD ResolveHostPid(DWORD pid, DWORD fgPid, const ProcNode *nodes, uint32_t nodeCount,
                            wchar_t *outHostName, size_t outHostNameCch)
{
    if (outHostName && outHostNameCch > 0) outHostName[0] = L'\0';
    if (pid == 0) return 0;

    const ProcNode *startNode = FindProcNode(nodes, nodeCount, pid);
    if (!startNode) {
        if (outHostName && outHostNameCch > 0) {
            if (!QueryProcessBaseName(pid, outHostName, outHostNameCch)) {
                _snwprintf_s(outHostName, outHostNameCch, _TRUNCATE, L"PID %lu", (unsigned long)pid);
            }
        }
        return pid;
    }

    DWORD visited[MODERN_MAX_PARENT_DEPTH + 2];
    uint32_t visitedCount = 0;
    visited[visitedCount++] = pid;

    DWORD curPid = pid;
    const ProcNode *curNode = startNode;
    DWORD bestHost = pid;
    const ProcNode *bestNode = startNode;

    // If the starting process itself already owns a visible top-level window
    // and is not a console helper (e.g. a game launched by Steam), keep itself as host.
    BOOL startIsStandaloneWindow = (HasVisibleWindow(pid) && !IsConsoleShellHelper(startNode->exeName));

    if (!startIsStandaloneWindow) {
        for (int depth = 0; depth < MODERN_MAX_PARENT_DEPTH; depth++) {
            if (curPid == fgPid && fgPid != 0) {
                bestHost = fgPid;
                bestNode = curNode;
                break;
            }

            DWORD ppid = curNode->parentPid;
            if (ppid == 0 || ppid == curPid) break;

            // Cycle guard (Windows can recycle PIDs into apparent loops)
            BOOL seen = FALSE;
            for (uint32_t v = 0; v < visitedCount; v++) {
                if (visited[v] == ppid) { seen = TRUE; break; }
            }
            if (seen) break;
            if (visitedCount < ARRAYSIZE(visited)) visited[visitedCount++] = ppid;

            const ProcNode *parentNode = FindProcNode(nodes, nodeCount, ppid);
            if (!parentNode) break;
            if (IsStopBoundaryProcess(parentNode->exeName)) break;

            if (ppid == fgPid && fgPid != 0) {
                bestHost = ppid;
                bestNode = parentNode;
                break;
            }

            BOOL sameImage     = (_wcsicmp(parentNode->exeName, curNode->exeName) == 0);
            BOOL parentHasWin  = HasVisibleWindow(ppid);
            BOOL parentIsTerm  = IsKnownTerminalHost(parentNode->exeName);

            if (parentHasWin && !IsConsoleShellHelper(parentNode->exeName)) {
                bestHost = ppid;
                bestNode = parentNode;
                // Reached a real windowed host (e.g. WindowsTerminal.exe, chrome.exe, ChatGPT.exe)
                break;
            }
            if (parentIsTerm || sameImage) {
                bestHost = ppid;
                bestNode = parentNode;
            }

            curPid  = ppid;
            curNode = parentNode;
        }
    }

    if (outHostName && outHostNameCch > 0 && bestNode) {
        lstrcpynW(outHostName, bestNode->exeName, (int)outHostNameCch);
    }
    return bestHost;
}

// ---- Per-raw-PID previous byte tracker ----
static RawPidState *FindOrAllocRawState(DWORD pid, ULONGLONG now)
{
    int freeIdx = -1;
    int oldestIdx = 0;
    ULONGLONG oldestTick = ULLONG_MAX;

    for (int i = 0; i < RAW_PREV_MAX; i++) {
        if (g_rawPrev[i].pid == pid) return &g_rawPrev[i];
        if (g_rawPrev[i].pid == 0 && freeIdx < 0) freeIdx = i;
        if (g_rawPrev[i].pid != 0 && g_rawPrev[i].lastSeenMs < oldestTick) {
            oldestTick = g_rawPrev[i].lastSeenMs;
            oldestIdx = i;
        }
    }

    int slot = (freeIdx >= 0) ? freeIdx : oldestIdx;
    ZeroMemory(&g_rawPrev[slot], sizeof(g_rawPrev[slot]));
    g_rawPrev[slot].pid = pid;
    g_rawPrev[slot].lastSeenMs = now;
    return &g_rawPrev[slot];
}

// ---- Persistent host cumulative tracker ----
static HostAccumState *FindOrAllocHostAccum(DWORD hostPid, const wchar_t *appName, ULONGLONG now)
{
    int freeIdx = -1;
    int oldestIdx = 0;
    ULONGLONG oldestTick = ULLONG_MAX;

    for (int i = 0; i < HOST_TOTAL_MAX; i++) {
        if (g_hostAccum[i].hostPid == hostPid) {
            if (appName && appName[0] && wcsncmp(appName, L"PID ", 4) != 0) {
                lstrcpynW(g_hostAccum[i].appName, appName, ARRAYSIZE(g_hostAccum[i].appName));
            }
            g_hostAccum[i].lastActiveMs = now;
            return &g_hostAccum[i];
        }
        if (g_hostAccum[i].hostPid == 0 && freeIdx < 0) freeIdx = i;
        if (g_hostAccum[i].hostPid != 0 && g_hostAccum[i].lastActiveMs < oldestTick) {
            oldestTick = g_hostAccum[i].lastActiveMs;
            oldestIdx = i;
        }
    }

    int slot = (freeIdx >= 0) ? freeIdx : oldestIdx;
    ZeroMemory(&g_hostAccum[slot], sizeof(g_hostAccum[slot]));
    g_hostAccum[slot].hostPid = hostPid;
    if (appName && appName[0]) {
        lstrcpynW(g_hostAccum[slot].appName, appName, ARRAYSIZE(g_hostAccum[slot].appName));
    }
    g_hostAccum[slot].lastActiveMs = now;
    return &g_hostAccum[slot];
}

// ---- Sorting comparator: descending by real-time speed, then total bytes ----
static int AppTrafficCmp(const void *a, const void *b)
{
    const ModernAppTraffic *x = (const ModernAppTraffic *)a;
    const ModernAppTraffic *y = (const ModernAppTraffic *)b;
    uint64_t xBps = x->rxBytesPerSec + x->txBytesPerSec;
    uint64_t yBps = y->rxBytesPerSec + y->txBytesPerSec;
    if (xBps != yBps) return (yBps > xBps) ? 1 : -1;
    uint64_t xTot = x->rxBytesTotal + x->txBytesTotal;
    uint64_t yTot = y->rxBytesTotal + y->txBytesTotal;
    if (xTot != yTot) return (yTot > xTot) ? 1 : -1;
    if (x->activeConns != y->activeConns) return (y->activeConns > x->activeConns) ? 1 : -1;
    if (x->hostPid != y->hostPid) return (x->hostPid < y->hostPid) ? -1 : 1;
    return 0;
}

// ---- Public lifecycle & subscription API ----
void StatsManager_Init(void)
{
    if (g_initialized) return;
    ZeroMemory(&g_latestView, sizeof(g_latestView));
    ZeroMemory(g_rawPrev, sizeof(g_rawPrev));
    ZeroMemory(g_hostAccum, sizeof(g_hostAccum));
    ZeroMemory(g_listeners, sizeof(g_listeners));
    g_procNodes      = (ProcNode *)calloc(PROC_SNAP_MAX, sizeof(ProcNode));
    g_visibleWinPids = (DWORD *)calloc(VISIBLE_WIN_MAX, sizeof(DWORD));
    g_prevTickMs     = 0;
    g_globalRxTotal  = 0;
    g_globalTxTotal  = 0;
    g_lastGlobalRtt  = 0;
    g_initialized    = TRUE;
}

void StatsManager_Shutdown(void)
{
    if (!g_initialized) return;
    free(g_procNodes);
    free(g_visibleWinPids);
    g_procNodes      = NULL;
    g_visibleWinPids = NULL;
    ZeroMemory(g_listeners, sizeof(g_listeners));
    g_ruleResolver     = NULL;
    g_ruleResolverUser = NULL;
    g_initialized      = FALSE;
}

void StatsManager_Reset(void)
{
    ZeroMemory(&g_latestView, sizeof(g_latestView));
    ZeroMemory(g_rawPrev, sizeof(g_rawPrev));
    ZeroMemory(g_hostAccum, sizeof(g_hostAccum));
    g_prevTickMs    = 0;
    g_globalRxTotal = 0;
    g_globalTxTotal = 0;
    g_lastGlobalRtt = 0;
}

BOOL StatsManager_AddListener(StatsUpdateCallback cb, void *user)
{
    if (!cb) return FALSE;
    for (int i = 0; i < STATS_MAX_LISTENERS; i++) {
        if (g_listeners[i].cb == cb && g_listeners[i].user == user) return TRUE;
    }
    for (int i = 0; i < STATS_MAX_LISTENERS; i++) {
        if (g_listeners[i].cb == NULL) {
            g_listeners[i].cb = cb;
            g_listeners[i].user = user;
            return TRUE;
        }
    }
    return FALSE;
}

void StatsManager_RemoveListener(StatsUpdateCallback cb, void *user)
{
    for (int i = 0; i < STATS_MAX_LISTENERS; i++) {
        if (g_listeners[i].cb == cb && g_listeners[i].user == user) {
            g_listeners[i].cb = NULL;
            g_listeners[i].user = NULL;
        }
    }
}

void StatsManager_SetRuleResolver(StatsRuleResolverFn fn, void *user)
{
    g_ruleResolver = fn;
    g_ruleResolverUser = user;
}

BOOL StatsManager_IsAppInProxyRules(const wchar_t *appName, wchar_t *ruleOut, size_t ruleCch)
{
    if (g_ruleResolver) {
        return g_ruleResolver(appName, ruleOut, ruleCch, g_ruleResolverUser);
    }
    if (ruleOut && ruleCch > 0) lstrcpynW(ruleOut, L"Proxy", (int)ruleCch);
    return TRUE;
}

const ModernStatsView *StatsManager_GetLatest(void)
{
    return &g_latestView;
}

void StatsManager_Tick(StatsFetchSnapshotFn fetchFn, BOOL proxyEnabled)
{
    if (!g_initialized) StatsManager_Init();

    ULONGLONG now = GetTickCount64();
    ULONGLONG dtMs = (g_prevTickMs > 0 && now > g_prevTickMs) ? (now - g_prevTickMs) : STATS_POLL_INTERVAL_MS;
    if (dtMs < 250ULL)  dtMs = 250ULL;
    if (dtMs > 5000ULL) dtMs = 5000ULL;
    g_prevTickMs = now;

    // Preserve sliding history across ticks
    uint64_t prevRxHist[MODERN_HISTORY_LEN];
    uint64_t prevTxHist[MODERN_HISTORY_LEN];
    memcpy(prevRxHist, g_latestView.rxHistory, sizeof(prevRxHist));
    memcpy(prevTxHist, g_latestView.txHistory, sizeof(prevTxHist));

    PidStatsSnapshot snap;
    ZeroMemory(&snap, sizeof(snap));
    BOOL fetched = (fetchFn != NULL) ? fetchFn(&snap) : FALSE;
    if (snap.count > MAX_TRACKED_PIDS) snap.count = MAX_TRACKED_PIDS;

    // Refresh foreground window + Toolhelp32 process tree + visible top-level windows
    HWND fgHwnd = NULL;
    DWORD fgPid = StatsManager_GetForegroundAppPid(&fgHwnd);

    g_visibleWinCount = 0;
    if (g_visibleWinPids) {
        EnumWindows(EnumVisibleWindowsProc, 0);
    }

    uint32_t procCount = g_procNodes ? CaptureProcessTable(g_procNodes, PROC_SNAP_MAX) : 0;

    ModernStatsView next;
    ZeroMemory(&next, sizeof(next));
    next.timestampMs    = now;
    next.proxyEnabled   = proxyEnabled;
    next.foregroundPid  = fgPid;
    next.foregroundHwnd = fgHwnd;

    if (fgPid != 0) {
        const ProcNode *fgNode = FindProcNode(g_procNodes, procCount, fgPid);
        if (fgNode) {
            lstrcpynW(next.foregroundAppName, fgNode->exeName, ARRAYSIZE(next.foregroundAppName));
        } else {
            QueryProcessBaseName(fgPid, next.foregroundAppName, ARRAYSIZE(next.foregroundAppName));
        }
    }

    uint64_t globalRxBps = 0;
    uint64_t globalTxBps = 0;
    uint32_t totalConns  = 0;
    uint32_t latestRtt   = 0;
    uint64_t latestRttTraffic = 0;

    if (fetched) {
        for (uint32_t i = 0; i < snap.count; i++) {
            const PidTrafficEntry *e = &snap.entries[i];
            if (e->pid == 0) continue;

            RawPidState *prev = FindOrAllocRawState(e->pid, now);
            uint64_t deltaRx = (e->rx_bytes >= prev->lastRx) ? (e->rx_bytes - prev->lastRx) : e->rx_bytes;
            uint64_t deltaTx = (e->tx_bytes >= prev->lastTx) ? (e->tx_bytes - prev->lastTx) : e->tx_bytes;
            prev->lastRx = e->rx_bytes;
            prev->lastTx = e->tx_bytes;
            prev->lastSeenMs = now;

            uint64_t rxBps = (deltaRx * 1000ULL) / dtMs;
            uint64_t txBps = (deltaTx * 1000ULL) / dtMs;

            globalRxBps     += rxBps;
            globalTxBps     += txBps;
            g_globalRxTotal += deltaRx;
            g_globalTxTotal += deltaTx;
            totalConns      += e->active_conns;

            if (e->rtt_ms > 0) {
                uint64_t weight = deltaRx + deltaTx + 1ULL;
                if (weight >= latestRttTraffic) {
                    latestRtt = e->rtt_ms;
                    latestRttTraffic = weight;
                }
            }

            // Resolve parent process tree -> host PID (with exited-child cache fallback)
            wchar_t hostName[128];
            hostName[0] = L'\0';
            DWORD hostPid = 0;
            if (FindProcNode(g_procNodes, procCount, e->pid) != NULL) {
                hostPid = ResolveHostPid(e->pid, fgPid, g_procNodes, procCount, hostName, ARRAYSIZE(hostName));
                prev->lastHostPid = hostPid;
                lstrcpynW(prev->lastExeName, hostName, ARRAYSIZE(prev->lastExeName));
            } else if (prev->lastHostPid != 0 && (now - prev->lastSeenMs) < PID_CACHE_EXPIRE_MS) {
                hostPid = prev->lastHostPid;
                lstrcpynW(hostName, prev->lastExeName, ARRAYSIZE(hostName));
            } else {
                hostPid = e->pid;
                _snwprintf_s(hostName, ARRAYSIZE(hostName), _TRUNCATE, L"PID %lu", (unsigned long)e->pid);
            }

            HostAccumState *accum = FindOrAllocHostAccum(hostPid, hostName, now);
            accum->rxTotal += deltaRx;
            accum->txTotal += deltaTx;
            if (e->rtt_ms > 0) accum->lastRttMs = e->rtt_ms;

            // Merge into next.apps[] by hostPid
            int appIdx = -1;
            for (uint32_t j = 0; j < next.appCount; j++) {
                if (next.apps[j].hostPid == hostPid) { appIdx = (int)j; break; }
            }
            if (appIdx < 0 && next.appCount < MODERN_MAX_APPS) {
                appIdx = (int)next.appCount++;
                ModernAppTraffic *a = &next.apps[appIdx];
                ZeroMemory(a, sizeof(*a));
                a->hostPid = hostPid;
                lstrcpynW(a->appName, accum->appName[0] ? accum->appName : hostName, ARRAYSIZE(a->appName));
                a->rxBytesTotal = accum->rxTotal;
                a->txBytesTotal = accum->txTotal;
                a->rttMs        = accum->lastRttMs;
                a->isForeground = (fgPid != 0 && hostPid == fgPid);
                a->inProxyRules = StatsManager_IsAppInProxyRules(a->appName, a->ruleName, ARRAYSIZE(a->ruleName));
            }
            if (appIdx >= 0) {
                ModernAppTraffic *a = &next.apps[appIdx];
                a->rxBytesPerSec += rxBps;
                a->txBytesPerSec += txBps;
                a->rxBytesTotal   = accum->rxTotal;
                a->txBytesTotal   = accum->txTotal;
                a->activeConns   += e->active_conns;
                a->treePidCount++;
                if (e->rtt_ms > 0 && (deltaRx + deltaTx > 0 || a->rttMs == 0)) {
                    a->rttMs = e->rtt_ms;
                }
            }
        }
    }

    // Also surface foreground host from persistent host cache if it ran short-lived CLI children
    // that have already exited and been evicted, so OSD/cards still know it was proxied recently.
    if (fgPid != 0) {
        BOOL foundInApps = FALSE;
        for (uint32_t i = 0; i < next.appCount; i++) {
            if (next.apps[i].hostPid == fgPid) {
                next.apps[i].isForeground = TRUE;
                next.foregroundMatched    = TRUE;
                next.foregroundStats      = next.apps[i];
                foundInApps = TRUE;
                break;
            }
        }
        if (!foundInApps) {
            for (int i = 0; i < HOST_TOTAL_MAX; i++) {
                if (g_hostAccum[i].hostPid == fgPid &&
                    (g_hostAccum[i].rxTotal > 0 || g_hostAccum[i].txTotal > 0) &&
                    (now - g_hostAccum[i].lastActiveMs) < PID_CACHE_EXPIRE_MS) {
                    next.foregroundMatched = TRUE;
                    next.foregroundStats.hostPid      = fgPid;
                    lstrcpynW(next.foregroundStats.appName, g_hostAccum[i].appName, ARRAYSIZE(next.foregroundStats.appName));
                    next.foregroundStats.rxBytesTotal = g_hostAccum[i].rxTotal;
                    next.foregroundStats.txBytesTotal = g_hostAccum[i].txTotal;
                    next.foregroundStats.rttMs        = g_hostAccum[i].lastRttMs;
                    next.foregroundStats.isForeground = TRUE;
                    next.foregroundStats.inProxyRules = StatsManager_IsAppInProxyRules(
                        next.foregroundStats.appName, next.foregroundStats.ruleName,
                        ARRAYSIZE(next.foregroundStats.ruleName));
                    break;
                }
            }
        }
    }

    if (latestRtt > 0) g_lastGlobalRtt = latestRtt;
    else if (totalConns == 0 && globalRxBps == 0 && globalTxBps == 0) g_lastGlobalRtt = 0;

    next.globalRxBps      = globalRxBps;
    next.globalTxBps      = globalTxBps;
    next.globalRxTotal    = g_globalRxTotal;
    next.globalTxTotal    = g_globalTxTotal;
    next.totalActiveConns = totalConns;
    next.globalRttMs      = g_lastGlobalRtt;

    // Sort active applications descending by real-time throughput
    if (next.appCount > 1) {
        qsort(next.apps, next.appCount, sizeof(ModernAppTraffic), AppTrafficCmp);
    }

    // Shift 30-second sliding speed history
    memmove(&next.rxHistory[0], &prevRxHist[1], (MODERN_HISTORY_LEN - 1) * sizeof(uint64_t));
    memmove(&next.txHistory[0], &prevTxHist[1], (MODERN_HISTORY_LEN - 1) * sizeof(uint64_t));
    next.rxHistory[MODERN_HISTORY_LEN - 1] = globalRxBps;
    next.txHistory[MODERN_HISTORY_LEN - 1] = globalTxBps;

    g_latestView = next;

    // Notify subscribers (Stage 2 OSD/RTSS, Stage 3 Capsule, Stage 4 Main Window)
    for (int i = 0; i < STATS_MAX_LISTENERS; i++) {
        if (g_listeners[i].cb) {
            g_listeners[i].cb(&g_latestView, g_listeners[i].user);
        }
    }
}
