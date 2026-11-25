#ifndef MAIN_WINDOW_MODERN_H
#define MAIN_WINDOW_MODERN_H

#include "modern_types.h"
#include "../profile/profile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODERN_MAIN_TAB_APPS  = 0, // Active applications (process-tree aggregated)
    MODERN_MAIN_TAB_RULES = 1, // Modern proxy rules ListView + CRUD toolbar
    MODERN_MAIN_TAB_LOGS  = 2  // Connection logs, activity logs, live flows & quick settings
} ModernMainTab;

typedef enum {
    MODERN_ACT_TOGGLE_PROXY = 1,
    MODERN_ACT_TOGGLE_CAPSULE,
    MODERN_ACT_TOGGLE_OSD,
    MODERN_ACT_CYCLE_THEME,
    MODERN_ACT_OPEN_TELEMETRY,
    MODERN_ACT_OPEN_SERVERS,
    MODERN_ACT_TAB_CHANGED,
    MODERN_ACT_RULE_ADD,
    MODERN_ACT_RULE_EDIT,
    MODERN_ACT_RULE_TOGGLE,
    MODERN_ACT_RULE_UP,
    MODERN_ACT_RULE_DOWN,
    MODERN_ACT_RULE_DELETE
} ModernMainAction;

typedef void (*ModernMainActionCallback)(ModernMainAction action, int param, void *user);

BOOL ModernMain_Init(HWND mainHwnd, HINSTANCE hInst, int dpi,
                     const ModernUiConfig *cfg,
                     ModernMainActionCallback onAction, void *user);
void ModernMain_Shutdown(void);

// Layout: positions the Direct2D header + cards + tab bar and the Tab 0 / Tab 1 child controls,
// returning the content top Y coordinate for Tab 2 (logs/flows) controls owned by main.c.
void ModernMain_Layout(HWND mainHwnd, int clientW, int clientH, int *outTab2ContentTop);

// Tab selection & state sync
void ModernMain_SetTab(ModernMainTab tab);
ModernMainTab ModernMain_GetTab(void);
void ModernMain_SetState(const ModernUiConfig *cfg, BOOL proxyEnabled,
                         const wchar_t *profileName, const wchar_t *versionStr, BOOL zhLang);
void ModernMain_RefreshRules(const PBProfile *profile);
int  ModernMain_GetSelectedRuleIndex(void);

// Theme synchronization (dark/light)
void ModernMain_ApplyTheme(HWND mainHwnd, BOOL isDark);

// Forward WM_NOTIFY and WM_COMMAND from main WndProc for ModernMain's child controls.
// Returns TRUE if handled (and writes *outResult).
BOOL ModernMain_HandleNotify(LPARAM lp, LRESULT *outResult);
BOOL ModernMain_HandleCommand(WPARAM wp, LPARAM lp, LRESULT *outResult);

// Unified SPA visual styling helpers for native Win32 child controls
typedef enum {
    MODERN_BTN_DEFAULT = 0,
    MODERN_BTN_PRIMARY,
    MODERN_BTN_DANGER,
    MODERN_BTN_ACCENT_SOFT
} ModernBtnVariant;

void ModernMain_StyleButton(HWND btn, ModernBtnVariant variant);
void ModernMain_StyleTabStrip(HWND tab);
void ModernMain_PaintCardDC(HDC hdc, const RECT *rc, float radius, BOOL focused);
void ModernMain_PaintMenuPillDC(HDC hdc, const RECT *rc, BOOL hot, BOOL dimmed);
void ModernMain_PaintActiveBodyCards(HDC hdc, const RECT *tab2SearchCard, BOOL searchVisible,
                                     BOOL searchFocused, const RECT *tab2LogCard);

// Rule-name resolver helper for StatsManager_SetRuleResolver.
// Returns TRUE iff appName is matched by an enabled PROXY rule (respecting top-down precedence).
BOOL ModernMain_ResolveRuleForApp(const PBProfile *profile, const wchar_t *appName,
                                  wchar_t *ruleOut, size_t ruleCch);

#ifdef __cplusplus
}
#endif

#endif // MAIN_WINDOW_MODERN_H
