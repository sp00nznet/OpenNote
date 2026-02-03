#include "supernote.h"
#include "res/resource.h"

// Create tab control
HWND TabControl_Create(HWND hParent) {
    HWND hTab = CreateWindowExW(
        0,
        WC_TABCONTROLW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TOOLTIPS,
        0, 0, 100, 30,
        hParent,
        NULL,
        g_app->hInstance,
        NULL
    );

    if (hTab) {
        // Create a smaller font for tabs
        HFONT hFont = CreateFontW(
            -11,                        // Height (negative = character height)
            0, 0, 0,                    // Width, escapement, orientation
            FW_NORMAL,                  // Weight
            FALSE, FALSE, FALSE,        // Italic, underline, strikeout
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );
        if (hFont) {
            SendMessageW(hTab, WM_SETFONT, (WPARAM)hFont, TRUE);
        } else {
            SendMessageW(hTab, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        }
    }

    return hTab;
}

// Add a tab
int TabControl_AddTab(const WCHAR* title) {
    if (!g_app->hTabControl) return -1;

    int count = (int)SendMessageW(g_app->hTabControl, TCM_GETITEMCOUNT, 0, 0);

    TCITEMW tci = {
        .mask = TCIF_TEXT,
        .pszText = (LPWSTR)title
    };

    int index = (int)SendMessageW(g_app->hTabControl, TCM_INSERTITEMW, count, (LPARAM)&tci);
    return index;
}

// Remove a tab
void TabControl_RemoveTab(int index) {
    if (!g_app->hTabControl) return;
    SendMessageW(g_app->hTabControl, TCM_DELETEITEM, index, 0);
}

// Set active tab
void TabControl_SetActiveTab(int index) {
    if (!g_app->hTabControl) return;
    SendMessageW(g_app->hTabControl, TCM_SETCURSEL, index, 0);
}

// Get active tab
int TabControl_GetActiveTab(void) {
    if (!g_app->hTabControl) return -1;
    return (int)SendMessageW(g_app->hTabControl, TCM_GETCURSEL, 0, 0);
}

// Get tab count
int TabControl_GetTabCount(void) {
    if (!g_app->hTabControl) return 0;
    return (int)SendMessageW(g_app->hTabControl, TCM_GETITEMCOUNT, 0, 0);
}

// Set tab title
void TabControl_SetTabTitle(int index, const WCHAR* title) {
    if (!g_app->hTabControl) return;

    TCITEMW tci = {
        .mask = TCIF_TEXT,
        .pszText = (LPWSTR)title
    };

    SendMessageW(g_app->hTabControl, TCM_SETITEMW, index, (LPARAM)&tci);
}

// Get tab title
void TabControl_GetTabTitle(int index, WCHAR* buffer, int bufferSize) {
    if (!g_app->hTabControl || !buffer) return;

    TCITEMW tci = {
        .mask = TCIF_TEXT,
        .pszText = buffer,
        .cchTextMax = bufferSize
    };

    SendMessageW(g_app->hTabControl, TCM_GETITEMW, index, (LPARAM)&tci);
}

// Update tab title based on document state
void TabControl_UpdateTabTitle(int index) {
    if (index < 0 || index >= MAX_TABS || !g_app->tabs[index]) return;

    Tab* tab = g_app->tabs[index];
    if (!tab->document) return;

    WCHAR title[MAX_TITLE_LEN + 2];
    const WCHAR* docTitle = Document_GetTitle(tab->document);

    if (tab->document->modified) {
        swprintf_s(title, sizeof(title)/sizeof(WCHAR), L"*%s", docTitle);
    } else {
        wcscpy_s(title, sizeof(title)/sizeof(WCHAR), docTitle);
    }

    TabControl_SetTabTitle(index, title);
}

// Handle tab selection change
void TabControl_OnSelChange(void) {
    int index = TabControl_GetActiveTab();
    if (index >= 0) {
        App_SetActiveTab(index);
    }
}

// Show context menu for empty tab area
static void TabControl_ShowEmptyAreaMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, IDM_FILE_NEW, L"&New");
    AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN, L"&Open...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN_NOTE, L"&Browse Notes...");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             x, y, 0, g_app->hMainWindow, NULL);

    if (cmd) {
        SendMessageW(g_app->hMainWindow, WM_COMMAND, cmd, 0);
    }

    DestroyMenu(hMenu);
}

// Handle right-click on tab
void TabControl_OnRightClick(int x, int y) {
    if (!g_app->hTabControl) return;

    // Find which tab was clicked
    POINT pt = { x, y };
    ScreenToClient(g_app->hTabControl, &pt);

    TCHITTESTINFO hti = {
        .pt = pt
    };

    int index = (int)SendMessageW(g_app->hTabControl, TCM_HITTEST, 0, (LPARAM)&hti);

    if (index >= 0) {
        TabControl_ShowContextMenu(index, x, y);
    } else {
        // Clicked on empty area - show New/Open menu
        TabControl_ShowEmptyAreaMenu(x, y);
    }
}

// Show context menu
void TabControl_ShowContextMenu(int tabIndex, int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, IDM_FILE_SAVE, L"&Save");
    AppendMenuW(hMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save &As...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Compare submenu - list other open tabs
    if (g_app->tabCount > 1) {
        HMENU hCompareMenu = CreatePopupMenu();
        int compareItems = 0;
        for (int i = 0; i < MAX_TABS; i++) {
            if (i != tabIndex && g_app->tabs[i] && g_app->tabs[i]->document) {
                WCHAR title[MAX_TITLE_LEN + 8];
                swprintf_s(title, sizeof(title)/sizeof(WCHAR), L"&%d. %s", compareItems + 1,
                          Document_GetTitle(g_app->tabs[i]->document));
                AppendMenuW(hCompareMenu, MF_STRING, IDM_COMPARE_BASE + i, title);
                compareItems++;
            }
        }
        if (compareItems > 0) {
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hCompareMenu, L"C&ompare With");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        } else {
            DestroyMenu(hCompareMenu);
        }
    }

    AppendMenuW(hMenu, MF_STRING, IDM_FILE_CLOSE_TAB, L"&Close Tab");
    AppendMenuW(hMenu, MF_STRING, 100, L"Close &Other Tabs");
    AppendMenuW(hMenu, MF_STRING, 101, L"Close All &Tabs");

    // Track which tab was right-clicked
    int prevActive = g_app->activeTab;
    App_SetActiveTab(tabIndex);

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             x, y, 0, g_app->hMainWindow, NULL);

    switch (cmd) {
        case IDM_FILE_SAVE:
        case IDM_FILE_SAVEAS:
        case IDM_FILE_CLOSE_TAB:
            SendMessageW(g_app->hMainWindow, WM_COMMAND, cmd, 0);
            break;

        case 100:  // Close Other Tabs
            for (int i = MAX_TABS - 1; i >= 0; i--) {
                if (i != tabIndex && g_app->tabs[i]) {
                    App_CloseTab(i);
                    if (tabIndex > i) tabIndex--;
                }
            }
            break;

        case 101:  // Close All Tabs
            while (g_app->tabCount > 0) {
                App_CloseTab(0);
            }
            break;

        default:
            // Check for compare commands
            if (cmd >= IDM_COMPARE_BASE && cmd < IDM_COMPARE_BASE + MAX_TABS) {
                int otherTab = cmd - IDM_COMPARE_BASE;
                if (otherTab >= 0 && otherTab < MAX_TABS && g_app->tabs[otherTab]) {
                    Dialogs_Compare(g_app->hMainWindow, tabIndex, otherTab);
                }
            } else {
                // Restore previous active tab if nothing was selected
                App_SetActiveTab(prevActive);
            }
            break;
    }

    DestroyMenu(hMenu);
}
