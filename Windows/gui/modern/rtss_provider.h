#ifndef RTSS_PROVIDER_H
#define RTSS_PROVIDER_H

#include "modern_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Standard RTSSSharedMemoryV2 contract definitions (RivaTuner Statistics Server v2.x)
#define RTSS_SHARED_MEMORY_NAME  L"RTSSSharedMemoryV2"
#define RTSS_SIGNATURE_ACTIVE    0x52545353u // 'RTSS'
#define RTSS_SIGNATURE_DEAD      0x0000DEADu
#define RTSS_OSD_OWNER_TAG       "ProxyBridgeOSD"

#pragma pack(push, 4)
typedef struct {
    DWORD dwSignature;      // 'RTSS' (0x52545353) or 0xDEAD when shutting down
    DWORD dwVersion;        // 0x00020000 or higher
    DWORD dwAppEntrySize;   // Size of each RTSS_SHARED_MEMORY_APP_ENTRY in bytes
    DWORD dwAppArrOffset;   // Byte offset to arrApp[]
    DWORD dwAppArrSize;     // Number of entries in arrApp[]
    DWORD dwOSDEntrySize;   // Size of each RTSS_SHARED_MEMORY_OSD_ENTRY in bytes
    DWORD dwOSDArrOffset;   // Byte offset to arrOSD[]
    DWORD dwOSDArrSize;     // Number of entries in arrOSD[]
    DWORD dwOSDFrame;       // Global OSD frame counter (increment after updates)
} RTSS_SHARED_MEMORY_HEADER;

typedef struct {
    DWORD dwProcessID;
    char  szName[MAX_PATH];
    DWORD dwFlags;          // Non-zero when hooked by DirectX/Vulkan/OpenGL
    DWORD dwTime0;
    DWORD dwTime1;
    DWORD dwFrames;
    DWORD dwFrameTime;
} RTSS_SHARED_MEMORY_APP_ENTRY_PREFIX;

typedef struct {
    char  szOSD[256];       // ANSI OSD text
    char  szOSDOwner[256];  // Owner tag ("ProxyBridgeOSD")
} RTSS_SHARED_MEMORY_OSD_ENTRY_BASE;

typedef struct {
    char  szOSD[256];
    char  szOSDOwner[256];
    char  szOSDEx[4096];    // Extended ANSI OSD text (RTSS v2.7+)
} RTSS_SHARED_MEMORY_OSD_ENTRY_EX;
#pragma pack(pop)

void RtssProvider_Init(void);
void RtssProvider_Shutdown(void);

// Returns TRUE iff RTSSSharedMemoryV2 is active and `pid` is currently hooked in arrApp[].
BOOL RtssProvider_IsPidHooked(DWORD pid);

// Writes formatted proxy telemetry to our owned RTSS OSD slot.
BOOL RtssProvider_Update(const ModernAppTraffic *app, BOOL proxyEnabled, BOOL compactMode);

// Immediately empties our RTSS OSD slot (szOSD[0] = '\0') and bumps dwOSDFrame.
void RtssProvider_Clear(void);

// Returns TRUE if our RTSS OSD slot currently holds non-empty text.
BOOL RtssProvider_IsSlotActive(void);

#ifdef __cplusplus
}
#endif

#endif // RTSS_PROVIDER_H
