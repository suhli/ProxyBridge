#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "rtss_provider.h"
#include <stdio.h>
#include <string.h>

#define RTSS_REOPEN_BACKOFF_MS 2000ULL

static HANDLE     g_rtssMap = NULL;
static BYTE      *g_rtssView = NULL;
static SIZE_T     g_rtssViewSize = 0;
static ULONGLONG  g_lastOpenAttemptMs = 0;
static BOOL       g_rtssSlotActive = FALSE;

static void UnmapRtss(void)
{
    if (g_rtssView) {
        UnmapViewOfFile(g_rtssView);
        g_rtssView = NULL;
    }
    if (g_rtssMap) {
        CloseHandle(g_rtssMap);
        g_rtssMap = NULL;
    }
    g_rtssViewSize = 0;
    g_rtssSlotActive = FALSE;
}

static RTSS_SHARED_MEMORY_HEADER *EnsureRtssMapped(void)
{
    if (g_rtssView != NULL) {
        RTSS_SHARED_MEMORY_HEADER *hdr = (RTSS_SHARED_MEMORY_HEADER *)g_rtssView;
        if (hdr->dwSignature == RTSS_SIGNATURE_ACTIVE && hdr->dwVersion >= 0x00020000u) {
            return hdr;
        }
        // RTSS exited or restarted (dwSignature == 0xDEAD)
        UnmapRtss();
    }

    ULONGLONG now = GetTickCount64();
    if (g_lastOpenAttemptMs != 0 && (now - g_lastOpenAttemptMs) < RTSS_REOPEN_BACKOFF_MS) {
        return NULL;
    }
    g_lastOpenAttemptMs = now;

    HANDLE hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, RTSS_SHARED_MEMORY_NAME);
    if (!hMap) return NULL;

    BYTE *view = (BYTE *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!view) {
        CloseHandle(hMap);
        return NULL;
    }

    MEMORY_BASIC_INFORMATION mbi;
    ZeroMemory(&mbi, sizeof(mbi));
    if (VirtualQuery(view, &mbi, sizeof(mbi)) == 0 || mbi.RegionSize < sizeof(RTSS_SHARED_MEMORY_HEADER)) {
        UnmapViewOfFile(view);
        CloseHandle(hMap);
        return NULL;
    }

    RTSS_SHARED_MEMORY_HEADER *hdr = (RTSS_SHARED_MEMORY_HEADER *)view;
    if (hdr->dwSignature != RTSS_SIGNATURE_ACTIVE || hdr->dwVersion < 0x00020000u) {
        UnmapViewOfFile(view);
        CloseHandle(hMap);
        return NULL;
    }

    g_rtssMap      = hMap;
    g_rtssView     = view;
    g_rtssViewSize = mbi.RegionSize;
    return hdr;
}

static void FormatSpeedAnsi(uint64_t bps, BOOL compact, char *out, size_t cap)
{
    if (bps < 1024ULL) {
        _snprintf_s(out, cap, _TRUNCATE, compact ? "%lluB/s" : "%llu B/s", (unsigned long long)bps);
    } else if (bps < 1024ULL * 1024ULL) {
        double kb = (double)bps / 1024.0;
        if (kb >= 100.0) _snprintf_s(out, cap, _TRUNCATE, compact ? "%.0fK/s" : "%.0f KB/s", kb);
        else             _snprintf_s(out, cap, _TRUNCATE, compact ? "%.1fK/s" : "%.1f KB/s", kb);
    } else if (bps < 1024ULL * 1024ULL * 1024ULL) {
        double mb = (double)bps / (1024.0 * 1024.0);
        if (mb >= 100.0) _snprintf_s(out, cap, _TRUNCATE, compact ? "%.0fM/s" : "%.0f MB/s", mb);
        else             _snprintf_s(out, cap, _TRUNCATE, compact ? "%.1fM/s" : "%.1f MB/s", mb);
    } else {
        double gb = (double)bps / (1024.0 * 1024.0 * 1024.0);
        _snprintf_s(out, cap, _TRUNCATE, compact ? "%.2fG/s" : "%.2f GB/s", gb);
    }
}

static void FormatBytesAnsi(uint64_t bytes, char *out, size_t cap)
{
    if (bytes < 1024ULL)
        _snprintf_s(out, cap, _TRUNCATE, "%llu B", (unsigned long long)bytes);
    else if (bytes < 1024ULL * 1024ULL)
        _snprintf_s(out, cap, _TRUNCATE, "%.1f KB", (double)bytes / 1024.0);
    else if (bytes < 1024ULL * 1024ULL * 1024ULL)
        _snprintf_s(out, cap, _TRUNCATE, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    else
        _snprintf_s(out, cap, _TRUNCATE, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
}

static RTSS_SHARED_MEMORY_OSD_ENTRY_BASE *GetOsdEntry(const RTSS_SHARED_MEMORY_HEADER *hdr, DWORD index)
{
    if (!hdr || !g_rtssView) return NULL;
    if (index >= hdr->dwOSDArrSize || hdr->dwOSDEntrySize < sizeof(RTSS_SHARED_MEMORY_OSD_ENTRY_BASE))
        return NULL;
    SIZE_T offset = (SIZE_T)hdr->dwOSDArrOffset + (SIZE_T)index * (SIZE_T)hdr->dwOSDEntrySize;
    if (offset + hdr->dwOSDEntrySize > g_rtssViewSize) return NULL;
    return (RTSS_SHARED_MEMORY_OSD_ENTRY_BASE *)(g_rtssView + offset);
}

static RTSS_SHARED_MEMORY_OSD_ENTRY_BASE *FindOrClaimSlot(const RTSS_SHARED_MEMORY_HEADER *hdr, BOOL allowClaim)
{
    if (!hdr || hdr->dwOSDArrSize == 0) return NULL;

    // Pass 1: find slot already owned by ProxyBridgeOSD
    for (DWORD i = 0; i < hdr->dwOSDArrSize; i++) {
        RTSS_SHARED_MEMORY_OSD_ENTRY_BASE *e = GetOsdEntry(hdr, i);
        if (!e) break;
        if (strncmp(e->szOSDOwner, RTSS_OSD_OWNER_TAG, sizeof(e->szOSDOwner)) == 0) {
            return e;
        }
    }
    if (!allowClaim) return NULL;

    // Pass 2: claim first empty slot, preferring index >= 1 (slot 0 is commonly used by Afterburner)
    for (DWORD pass = 0; pass < 2; pass++) {
        DWORD start = (pass == 0 && hdr->dwOSDArrSize > 1) ? 1u : 0u;
        DWORD end   = (pass == 0 && hdr->dwOSDArrSize > 1) ? hdr->dwOSDArrSize : 1u;
        for (DWORD i = start; i < end; i++) {
            RTSS_SHARED_MEMORY_OSD_ENTRY_BASE *e = GetOsdEntry(hdr, i);
            if (!e) break;
            if (e->szOSDOwner[0] == '\0') {
                strncpy_s(e->szOSDOwner, sizeof(e->szOSDOwner), RTSS_OSD_OWNER_TAG, _TRUNCATE);
                return e;
            }
        }
    }
    return NULL;
}

void RtssProvider_Init(void)
{
    g_lastOpenAttemptMs = 0;
    g_rtssSlotActive = FALSE;
    EnsureRtssMapped();
}

void RtssProvider_Shutdown(void)
{
    RtssProvider_Clear();
    UnmapRtss();
}

BOOL RtssProvider_IsPidHooked(DWORD pid)
{
    if (pid == 0) return FALSE;
    RTSS_SHARED_MEMORY_HEADER *hdr = EnsureRtssMapped();
    if (!hdr) return FALSE;

    if (hdr->dwAppEntrySize < sizeof(RTSS_SHARED_MEMORY_APP_ENTRY_PREFIX) || hdr->dwAppArrSize == 0)
        return FALSE;

    for (DWORD i = 0; i < hdr->dwAppArrSize; i++) {
        SIZE_T offset = (SIZE_T)hdr->dwAppArrOffset + (SIZE_T)i * (SIZE_T)hdr->dwAppEntrySize;
        if (offset + sizeof(RTSS_SHARED_MEMORY_APP_ENTRY_PREFIX) > g_rtssViewSize) break;
        const RTSS_SHARED_MEMORY_APP_ENTRY_PREFIX *app =
            (const RTSS_SHARED_MEMORY_APP_ENTRY_PREFIX *)(g_rtssView + offset);
        if (app->dwProcessID == pid) {
            if (app->dwFlags != 0 || app->dwFrames > 0 || app->dwTime1 != 0) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

BOOL RtssProvider_Update(const ModernAppTraffic *app, BOOL proxyEnabled, BOOL compactMode)
{
    if (!app) return FALSE;
    RTSS_SHARED_MEMORY_HEADER *hdr = EnsureRtssMapped();
    if (!hdr) return FALSE;

    RTSS_SHARED_MEMORY_OSD_ENTRY_BASE *slot = FindOrClaimSlot(hdr, TRUE);
    if (!slot) return FALSE;

    uint32_t dotArgb = ModernStatusColorArgb(proxyEnabled, app->rttMs);
    unsigned int dotRgb = (unsigned int)(dotArgb & 0x00FFFFFFu);

    char dn[32], up[32], tot[32], rttBuf[24], text[256];
    FormatSpeedAnsi(app->rxBytesPerSec, compactMode, dn, sizeof(dn));
    FormatSpeedAnsi(app->txBytesPerSec, compactMode, up, sizeof(up));
    FormatBytesAnsi(app->rxBytesTotal + app->txBytesTotal, tot, sizeof(tot));

    if (!proxyEnabled) {
        strncpy_s(rttBuf, sizeof(rttBuf), "PAUSED", _TRUNCATE);
    } else if (app->rttMs > 0) {
        _snprintf_s(rttBuf, sizeof(rttBuf), _TRUNCATE, "%ums", app->rttMs);
    } else {
        strncpy_s(rttBuf, sizeof(rttBuf), "--ms", _TRUNCATE);
    }

    if (compactMode) {
        _snprintf_s(text, sizeof(text), _TRUNCATE,
            "<C0=%06X><C0>*<C> %s  <C1=38BDF8><C1>DN %s<C>  <C2=A78BFA><C2>UP %s<C>",
            dotRgb, rttBuf, dn, up);
    } else {
        _snprintf_s(text, sizeof(text), _TRUNCATE,
            "<C0=%06X><C0>*<C> %s  |  <C1=38BDF8><C1>DN %s<C>  <C2=A78BFA><C2>UP %s<C>  |  Total %s",
            dotRgb, rttBuf, dn, up, tot);
    }

    strncpy_s(slot->szOSD, sizeof(slot->szOSD), text, _TRUNCATE);
    if (hdr->dwOSDEntrySize >= sizeof(RTSS_SHARED_MEMORY_OSD_ENTRY_EX)) {
        RTSS_SHARED_MEMORY_OSD_ENTRY_EX *ex = (RTSS_SHARED_MEMORY_OSD_ENTRY_EX *)slot;
        ex->szOSDEx[0] = '\0';
    }
    InterlockedIncrement((volatile LONG *)&hdr->dwOSDFrame);
    g_rtssSlotActive = TRUE;
    return TRUE;
}

void RtssProvider_Clear(void)
{
    if (!g_rtssView) {
        g_rtssSlotActive = FALSE;
        return;
    }
    RTSS_SHARED_MEMORY_HEADER *hdr = (RTSS_SHARED_MEMORY_HEADER *)g_rtssView;
    if (hdr->dwSignature != RTSS_SIGNATURE_ACTIVE || hdr->dwVersion < 0x00020000u) {
        UnmapRtss();
        return;
    }

    RTSS_SHARED_MEMORY_OSD_ENTRY_BASE *slot = FindOrClaimSlot(hdr, FALSE);
    if (slot) {
        BOOL changed = (slot->szOSD[0] != '\0' || slot->szOSDOwner[0] != '\0');
        slot->szOSD[0] = '\0';
        if (hdr->dwOSDEntrySize >= sizeof(RTSS_SHARED_MEMORY_OSD_ENTRY_EX)) {
            RTSS_SHARED_MEMORY_OSD_ENTRY_EX *ex = (RTSS_SHARED_MEMORY_OSD_ENTRY_EX *)slot;
            if (ex->szOSDEx[0] != '\0') changed = TRUE;
            ex->szOSDEx[0] = '\0';
        }
        slot->szOSDOwner[0] = '\0';
        if (changed) {
            InterlockedIncrement((volatile LONG *)&hdr->dwOSDFrame);
        }
    }
    g_rtssSlotActive = FALSE;
}

BOOL RtssProvider_IsSlotActive(void)
{
    return g_rtssSlotActive;
}
