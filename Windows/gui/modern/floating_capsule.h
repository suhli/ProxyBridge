#ifndef FLOATING_CAPSULE_H
#define FLOATING_CAPSULE_H

#include "modern_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAPSULE_ACTION_TOGGLE_PROXY = 1,
    CAPSULE_ACTION_TOGGLE_TOPMOST,
    CAPSULE_ACTION_TOGGLE_OSD,
    CAPSULE_ACTION_OPEN_OSD_SETTINGS,
    CAPSULE_ACTION_SHOW_MAIN,
    CAPSULE_ACTION_HIDE_CAPSULE,
    CAPSULE_ACTION_EXIT_APP
} FloatingCapsuleAction;

typedef void (*FloatingCapsuleActionCallback)(FloatingCapsuleAction action, void *user);
typedef void (*FloatingCapsulePosCallback)(int x, int y, void *user);

BOOL FloatingCapsule_Init(HINSTANCE hInst, const ModernUiConfig *initialCfg,
                          FloatingCapsuleActionCallback onAction,
                          FloatingCapsulePosCallback onPosChanged,
                          void *user);
void FloatingCapsule_Shutdown(void);

// Applies updated visibility, topmost, theme, and position settings immediately.
void FloatingCapsule_ApplyConfig(const ModernUiConfig *cfg);

// Immediately updates proxy enabled visual state (emerald vs coral dot).
void FloatingCapsule_SetProxyEnabled(BOOL enabled);

// Pure edge-snap & work-area clamp helper: snaps to left/right edge when distance < 16px.
void FloatingCapsule_ComputeSnapPosition(int x, int y, int width, int height,
                                         const RECT *workArea, int *outX, int *outY);

// Inspection helpers
BOOL FloatingCapsule_IsVisible(void);
HWND FloatingCapsule_GetHwnd(void);
BOOL FloatingCapsule_GetPosition(int *outX, int *outY);

#ifdef __cplusplus
}
#endif

#endif // FLOATING_CAPSULE_H
