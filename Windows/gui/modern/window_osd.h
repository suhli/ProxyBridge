#ifndef WINDOW_OSD_H
#define WINDOW_OSD_H

#include "modern_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*WindowOsdForegroundCallback)(HWND newFgHwnd, void *user);

BOOL WindowOsd_Init(HINSTANCE hInst, WindowOsdForegroundCallback fgCb, void *fgUser);
void WindowOsd_Shutdown(void);

// Updates content, anchors to targetHwnd client top-left + (offsetX, offsetY), and shows non-activating.
void WindowOsd_Update(HWND targetHwnd, const ModernAppTraffic *app, BOOL proxyEnabled, const ModernUiConfig *cfg);

// Immediately hides the companion OSD capsule.
void WindowOsd_Hide(void);

// Inspection helpers (used by overlay_dispatcher & verification).
BOOL  WindowOsd_IsVisible(void);
HWND  WindowOsd_GetHwnd(void);
DWORD WindowOsd_GetExStyle(void);

#ifdef __cplusplus
}
#endif

#endif // WINDOW_OSD_H
