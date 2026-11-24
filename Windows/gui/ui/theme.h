// ui/theme.h - dark-theme helpers for the GUI.
//
// Unity-build include: pulled into main.c after the colour macros (C_*), brush globals
// (g_br*), the UAH menu structs and the uxtheme ordinal typedefs are declared, so this
// file relies on those already being visible. Not a standalone header.
#ifndef PB_UI_THEME_H
#define PB_UI_THEME_H

static void RebuildThemeBrushes(BOOL isDark)
{
    if (g_brBg)      DeleteObject(g_brBg);
    if (g_brPanel)   DeleteObject(g_brPanel);
    if (g_brTab)     DeleteObject(g_brTab);
    if (g_brTabSel)  DeleteObject(g_brTabSel);
    if (g_brTabHot)  DeleteObject(g_brTabHot);
    if (g_brAccent)  DeleteObject(g_brAccent);
    if (g_brMenu)    DeleteObject(g_brMenu);
    if (g_brMenuHot) DeleteObject(g_brMenuHot);

    if (isDark)
    {
        g_brBg      = CreateSolidBrush(MODERN_RGB_BG_DARK);
        g_brPanel   = CreateSolidBrush(C_PANEL);
        g_brTab     = CreateSolidBrush(C_TAB);
        g_brTabSel  = CreateSolidBrush(MODERN_RGB_BG_DARK);
        g_brTabHot  = CreateSolidBrush(C_TABHOT);
        g_brAccent  = CreateSolidBrush(C_ACCENT);
        g_brMenu    = CreateSolidBrush(RGB(0x20, 0x20, 0x24));
        g_brMenuHot = CreateSolidBrush(RGB(0x32, 0x33, 0x3A));
    }
    else
    {
        g_brBg      = CreateSolidBrush(MODERN_RGB_BG_LIGHT);
        g_brPanel   = CreateSolidBrush(RGB(0xEC, 0xEF, 0xF4));
        g_brTab     = CreateSolidBrush(RGB(0xE8, 0xEC, 0xF2));
        g_brTabSel  = CreateSolidBrush(MODERN_RGB_CARD_LIGHT);
        g_brTabHot  = CreateSolidBrush(RGB(0xDD, 0xE3, 0xEC));
        g_brAccent  = CreateSolidBrush(RGB(0x00, 0x78, 0xD4));
        g_brMenu    = CreateSolidBrush(MODERN_RGB_BG_LIGHT);
        g_brMenuHot = CreateSolidBrush(RGB(0xE2, 0xE6, 0xEC));
    }
}

static void CreateDarkBrushes(void)
{
    RebuildThemeBrushes(g_uiIsDark);
}

// Group boxes (BS_GROUPBOX) draw their own caption; the themed button keeps it black on
// dark. Owner-paint the frame + caption so they match the active light/dark theme.
static LRESULT CALLBACK GroupSubProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    (void)id; (void)ref;
    if (m == WM_PAINT)
    {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        wchar_t txt[64] = {0}; GetWindowTextW(h, txt, 64);
        FillRect(dc, &rc, g_brBg);
        HFONT f = (HFONT)SendMessageW(h, WM_GETFONT, 0, 0);
        HFONT of = f ? (HFONT)SelectObject(dc, f) : NULL;
        SIZE ts; GetTextExtentPoint32W(dc, txt, lstrlenW(txt), &ts);
        HPEN pen = CreatePen(PS_SOLID, 1, UiClrLine()), op = (HPEN)SelectObject(dc, pen);
        HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, rc.left, rc.top + ts.cy / 2, rc.right - 1, rc.bottom - 1, 6, 6);
        SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
        RECT tr = { rc.left + 8, rc.top, rc.left + 14 + ts.cx, rc.top + ts.cy };
        FillRect(dc, &tr, g_brBg);                       // break the frame line behind the caption
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, UiClrText());
        TextOutW(dc, rc.left + 11, rc.top, txt, lstrlenW(txt));
        if (of) SelectObject(dc, of);
        EndPaint(h, &ps);
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

// Apply the matching light/dark theme to a child control by class.
static BOOL CALLBACK DarkThemeChild(HWND h, LPARAM lp)
{
    (void)lp;
    const wchar_t* cfd = g_uiIsDark ? L"DarkMode_CFD"      : L"Explorer";
    const wchar_t* exp = g_uiIsDark ? L"DarkMode_Explorer" : L"Explorer";
    wchar_t cls[48]; GetClassNameW(h, cls, 48);
    if (!_wcsicmp(cls, L"Edit") || !_wcsicmp(cls, L"ComboBox"))
        SetWindowTheme(h, cfd, NULL);
    else if (!_wcsicmp(cls, L"Button"))
    {
        if ((GetWindowLongW(h, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX)
            SetWindowSubclass(h, GroupSubProc, 7, 0);
        else
            SetWindowTheme(h, exp, NULL);
    }
    else if (!_wcsicmp(cls, L"ListBox"))
        SetWindowTheme(h, exp, NULL);
    else if (!_wcsicmp(cls, L"SysListView32") || !_wcsicmp(cls, WC_TABCONTROLW))
        SetWindowTheme(h, exp, NULL);
    return TRUE;
}

static void InitDarkMode(HWND hwnd)
{
    // Sync OS immersive theme so popup menus, scrollbars and combo dropdowns match g_uiIsDark.
    HMODULE ux = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (ux)
    {
        fnSetPreferredAppMode setMode = (fnSetPreferredAppMode)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(135));
        fnAllowDarkModeForWindow allow = (fnAllowDarkModeForWindow)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(133));
        fnFlushMenuThemes flush = (fnFlushMenuThemes)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(136));
        if (setMode) setMode(g_uiIsDark ? APPMODE_FORCEDARK : APPMODE_FORCELIGHT);
        if (allow)   allow(hwnd, g_uiIsDark ? TRUE : FALSE);
        if (flush)   flush();
        FreeLibrary(ux);   // ordinals were called; release our loader ref (called per dialog)
    }
    BOOL dark = g_uiIsDark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    EnumChildWindows(hwnd, DarkThemeChild, 0);
}

// Paint over the 1px line the OS draws under the menu bar so it matches the active theme.
static void PaintMenuBottomLine(HWND hwnd)
{
    MENUBARINFO mbi; mbi.cbSize = sizeof(mbi);
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi)) return;
    RECT wr; GetWindowRect(hwnd, &wr);
    RECT r = mbi.rcBar; OffsetRect(&r, -wr.left, -wr.top);
    r.top = r.bottom; r.bottom += 1;
    HDC dc = GetWindowDC(hwnd);
    HBRUSH lineBr = CreateSolidBrush(UiClrLine());
    FillRect(dc, &r, lineBr ? lineBr : g_brMenu);
    if (lineBr) DeleteObject(lineBr);
    ReleaseDC(hwnd, dc);
}

// Shared light/dark colouring for dialog controls (dialog procs return the brush directly).
static INT_PTR DlgCtlColor(WPARAM wp)
{
    HDC dc = (HDC)wp;
    SetTextColor(dc, UiClrText());
    SetBkColor(dc, UiClrBg());
    return (INT_PTR)g_brBg;
}

// Drop-in theme handling for every WM_CTLCOLOR* a dialog gets. Use inside a dialog proc's
// switch for the common case; dialogs that need per-control colours handle them explicitly.
#define PB_DARK_CTLCOLORS                                                    \
    case WM_CTLCOLORDLG: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:      \
    case WM_CTLCOLORBTN: case WM_CTLCOLORSTATIC: return DlgCtlColor(wp)

#endif // PB_UI_THEME_H
