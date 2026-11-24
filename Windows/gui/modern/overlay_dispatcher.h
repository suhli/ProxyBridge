#ifndef OVERLAY_DISPATCHER_H
#define OVERLAY_DISPATCHER_H

#include "modern_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OVERLAY_CHANNEL_NONE   = 0,
    OVERLAY_CHANNEL_RTSS   = 1, // Channel A: RTSSSharedMemoryV2 (3D/DX/Vulkan/OpenGL)
    OVERLAY_CHANNEL_NATIVE = 2  // Channel B: Win32 Direct2D click-through micro-capsule
} OverlayChannel;

BOOL OverlayDispatcher_Init(HINSTANCE hInst, const ModernUiConfig *initialCfg);
void OverlayDispatcher_Shutdown(void);

void OverlayDispatcher_SetConfig(const ModernUiConfig *cfg);
void OverlayDispatcher_GetConfig(ModernUiConfig *outCfg);
OverlayChannel OverlayDispatcher_GetActiveChannel(void);

#ifdef __cplusplus
}
#endif

#endif // OVERLAY_DISPATCHER_H
