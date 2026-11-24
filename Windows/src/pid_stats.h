#ifndef PID_STATS_H
#define PID_STATS_H

#include <windows.h>
#include <stdint.h>

#define MAX_TRACKED_PIDS 128

typedef struct {
    DWORD pid;              // Process ID
    uint64_t tx_bytes;      // Cumulative sent bytes (InterlockedAdd64)
    uint64_t rx_bytes;      // Cumulative received bytes (InterlockedAdd64)
    uint32_t rtt_ms;        // Latest TCP connect handshake latency (ms)
    uint32_t active_conns;  // Current active connections
} PidTrafficEntry;

typedef struct {
    uint32_t count;
    PidTrafficEntry entries[MAX_TRACKED_PIDS];
} PidStatsSnapshot;

#ifdef PROXYBRIDGE_EXPORTS

#define PID_STATS_IDLE_EVICT_MS 60000ULL

typedef struct {
    volatile LONG   pid;           // 0 = empty, -1 = recycling, >0 = active PID
    volatile LONG64 tx_bytes;
    volatile LONG64 rx_bytes;
    volatile LONG   rtt_ms;
    volatile LONG   active_conns;
    volatile LONG64 last_active;
} PidStatsSlot;

#ifdef PID_STATS_IMPL
PidStatsSlot  g_pid_stats_table[MAX_TRACKED_PIDS];
LARGE_INTEGER g_pid_stats_qpf;
#else
extern PidStatsSlot  g_pid_stats_table[MAX_TRACKED_PIDS];
extern LARGE_INTEGER g_pid_stats_qpf;
#endif

static __inline void pid_stats_add64(volatile LONG64 *dst, LONG64 val)
{
#if defined(_WIN64) && defined(_MSC_VER)
    InterlockedAdd64(dst, val);
#else
    InterlockedExchangeAdd64(dst, val);
#endif
}

static __inline void pid_stats_reset(void)
{
    ZeroMemory(g_pid_stats_table, sizeof(g_pid_stats_table));
    QueryPerformanceFrequency(&g_pid_stats_qpf);
}

static __inline LARGE_INTEGER pid_stats_now(void)
{
    LARGE_INTEGER t;
    t.QuadPart = 0;
    QueryPerformanceCounter(&t);
    return t;
}

static __inline PidStatsSlot *pid_stats_find_existing(DWORD pid)
{
    if (pid == 0) return NULL;
    uint32_t start = ((uint32_t)pid * 2654435761u) & (MAX_TRACKED_PIDS - 1);
    for (uint32_t i = 0; i < MAX_TRACKED_PIDS; i++) {
        uint32_t idx = (start + i) & (MAX_TRACKED_PIDS - 1);
        LONG cur = g_pid_stats_table[idx].pid;
        if (cur == (LONG)pid) return &g_pid_stats_table[idx];
    }
    return NULL;
}

static __inline PidStatsSlot *pid_stats_get_slot(DWORD pid)
{
    if (pid == 0) return NULL;
    uint32_t start = ((uint32_t)pid * 2654435761u) & (MAX_TRACKED_PIDS - 1);
    ULONGLONG now = GetTickCount64();

    // Pass 1: find existing entry or claim an empty slot
    for (uint32_t i = 0; i < MAX_TRACKED_PIDS; i++) {
        uint32_t idx = (start + i) & (MAX_TRACKED_PIDS - 1);
        PidStatsSlot *s = &g_pid_stats_table[idx];
        LONG cur = s->pid;
        if (cur == (LONG)pid) return s;
        if (cur == 0) {
            LONG prev = InterlockedCompareExchange(&s->pid, (LONG)pid, 0);
            if (prev == 0) {
                InterlockedExchange64(&s->last_active, (LONGLONG)now);
                return s;
            }
            if (prev == (LONG)pid) return s;
        }
    }

    // Pass 2: table full - atomically recycle a slot that has been idle > 60s with 0 active conns
    for (uint32_t i = 0; i < MAX_TRACKED_PIDS; i++) {
        uint32_t idx = (start + i) & (MAX_TRACKED_PIDS - 1);
        PidStatsSlot *s = &g_pid_stats_table[idx];
        LONG cur = s->pid;
        if (cur <= 0) continue;
        if (s->active_conns > 0) continue;
        ULONGLONG last = (ULONGLONG)InterlockedCompareExchange64(&s->last_active, 0, 0);
        if (now > last && (now - last) >= PID_STATS_IDLE_EVICT_MS) {
            if (InterlockedCompareExchange(&s->pid, -1, cur) == cur) {
                InterlockedExchange64(&s->tx_bytes, 0);
                InterlockedExchange64(&s->rx_bytes, 0);
                InterlockedExchange(&s->rtt_ms, 0);
                InterlockedExchange(&s->active_conns, 0);
                InterlockedExchange64(&s->last_active, (LONGLONG)now);
                InterlockedExchange(&s->pid, (LONG)pid);
                return s;
            }
        }
    }
    return NULL;
}

static __inline void pid_stats_add_bytes(DWORD pid, uint64_t tx, uint64_t rx)
{
    if (pid == 0 || (tx == 0 && rx == 0)) return;
    PidStatsSlot *s = pid_stats_get_slot(pid);
    if (s == NULL) return;
    if (tx > 0) pid_stats_add64(&s->tx_bytes, (LONG64)tx);
    if (rx > 0) pid_stats_add64(&s->rx_bytes, (LONG64)rx);
    InterlockedExchange64(&s->last_active, (LONGLONG)GetTickCount64());
}

static __inline void pid_stats_record_rtt(DWORD pid, LARGE_INTEGER t0)
{
    if (pid == 0 || t0.QuadPart == 0) return;
    LARGE_INTEGER freq = g_pid_stats_qpf;
    if (freq.QuadPart <= 0) {
        QueryPerformanceFrequency(&freq);
        if (freq.QuadPart <= 0) return;
        g_pid_stats_qpf = freq;
    }
    LARGE_INTEGER t1;
    QueryPerformanceCounter(&t1);
    LONGLONG delta = t1.QuadPart - t0.QuadPart;
    if (delta < 0) delta = 0;
    uint32_t ms = (uint32_t)((delta * 1000LL + (freq.QuadPart / 2)) / freq.QuadPart);
    if (ms == 0 && delta > 0) ms = 1;
    PidStatsSlot *s = pid_stats_get_slot(pid);
    if (s != NULL) {
        InterlockedExchange(&s->rtt_ms, (LONG)ms);
        InterlockedExchange64(&s->last_active, (LONGLONG)GetTickCount64());
    }
}

static __inline void pid_stats_conn_open(DWORD pid)
{
    if (pid == 0) return;
    PidStatsSlot *s = pid_stats_get_slot(pid);
    if (s == NULL) return;
    InterlockedIncrement(&s->active_conns);
    InterlockedExchange64(&s->last_active, (LONGLONG)GetTickCount64());
}

static __inline void pid_stats_conn_close(DWORD pid)
{
    PidStatsSlot *s = pid_stats_find_existing(pid);
    if (s == NULL) return;
    for (;;) {
        LONG cur = s->active_conns;
        if (cur <= 0) break;
        if (InterlockedCompareExchange(&s->active_conns, cur - 1, cur) == cur) break;
    }
    InterlockedExchange64(&s->last_active, (LONGLONG)GetTickCount64());
}

static __inline BOOL pid_stats_snapshot(PidStatsSnapshot *out)
{
    if (out == NULL) return FALSE;
    ZeroMemory(out, sizeof(*out));
    uint32_t n = 0;
    for (int i = 0; i < MAX_TRACKED_PIDS && n < MAX_TRACKED_PIDS; i++) {
        LONG p = g_pid_stats_table[i].pid;
        if (p <= 0) continue;
        LONG64 tx    = InterlockedCompareExchange64(&g_pid_stats_table[i].tx_bytes, 0, 0);
        LONG64 rx    = InterlockedCompareExchange64(&g_pid_stats_table[i].rx_bytes, 0, 0);
        LONG   rtt   = g_pid_stats_table[i].rtt_ms;
        LONG   conns = g_pid_stats_table[i].active_conns;
        if (g_pid_stats_table[i].pid != p) continue;
        if (tx <= 0 && rx <= 0 && conns <= 0 && rtt <= 0) continue;
        PidTrafficEntry *e = &out->entries[n++];
        e->pid          = (DWORD)p;
        e->tx_bytes     = tx > 0 ? (uint64_t)tx : 0;
        e->rx_bytes     = rx > 0 ? (uint64_t)rx : 0;
        e->rtt_ms       = rtt > 0 ? (uint32_t)rtt : 0;
        e->active_conns = conns > 0 ? (uint32_t)conns : 0;
    }
    out->count = n;
    return TRUE;
}

#endif // PROXYBRIDGE_EXPORTS

#endif // PID_STATS_H
