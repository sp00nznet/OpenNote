#include "supernote.h"
#include "res/resource.h"
#include <windowsx.h>
#include <Scintilla.h>
#include <shellapi.h>

// Window procedure forward declaration
static void OnCreate(HWND hwnd);
static void OnDestroy(HWND hwnd);
static void OnNotify(HWND hwnd, int idCtrl, LPNMHDR pnmh);

// System tray
static NOTIFYICONDATAW g_trayIcon = {0};
static BOOL g_trayIconVisible = FALSE;

static void TrayIcon_Add(HWND hwnd) {
    if (g_trayIconVisible) return;

    g_trayIcon.cbSize = sizeof(g_trayIcon);
    g_trayIcon.hWnd = hwnd;
    g_trayIcon.uID = 1;
    g_trayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_trayIcon.uCallbackMessage = WM_APP_TRAY_CALLBACK;
    g_trayIcon.hIcon = LoadIconW(g_app->hInstance, MAKEINTRESOURCEW(IDI_SUPERNOTE));
    wcscpy_s(g_trayIcon.szTip, 128, APP_NAME);

    Shell_NotifyIconW(NIM_ADD, &g_trayIcon);
    g_trayIconVisible = TRUE;
}

static void TrayIcon_Remove(void) {
    if (!g_trayIconVisible) return;

    Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
    g_trayIconVisible = FALSE;
}

// Register window class
BOOL MainWindow_RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {
        .cbSize = sizeof(wc),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = MainWindow_WndProc,
        .hInstance = hInstance,
        .hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SUPERNOTE)),
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszMenuName = NULL,
        .lpszClassName = APP_CLASS_NAME,
        .hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SUPERNOTE))
    };

    return RegisterClassExW(&wc) != 0;
}

// Create main window
HWND MainWindow_Create(HINSTANCE hInstance) {
    // Create menu
    HMENU hMenu = MenuBar_Create();

    HWND hwnd = CreateWindowExW(
        0,
        APP_CLASS_NAME,
        APP_NAME,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL,
        hMenu,
        hInstance,
        NULL
    );

    return hwnd;
}

// Window procedure
LRESULT CALLBACK MainWindow_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate(hwnd);
            return 0;

        case WM_DESTROY:
            OnDestroy(hwnd);
            return 0;

        case WM_SIZE:
            MainWindow_OnSize(hwnd, (UINT)wParam, LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_CLOSE:
            MainWindow_OnClose(hwnd);
            return 0;

        case WM_QUERYENDSESSION:
            return MainWindow_OnQueryEndSession(hwnd);

        case WM_COMMAND:
            MainWindow_OnCommand(hwnd, LOWORD(wParam), (HWND)lParam, HIWORD(wParam));
            return 0;

        case WM_NOTIFY:
            OnNotify(hwnd, (int)wParam, (LPNMHDR)lParam);
            return 0;

        case WM_PARENTNOTIFY:
            // Handle double-click on tab control empty area
            if (LOWORD(wParam) == WM_LBUTTONDOWN) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                HWND hChild = ChildWindowFromPoint(hwnd, pt);
                if (hChild == g_app->hTabControl) {
                    // Click is on tab control - check if on empty area
                    POINT ptTab = pt;
                    MapWindowPoints(hwnd, g_app->hTabControl, &ptTab, 1);
                    TCHITTESTINFO hti = { .pt = ptTab };
                    int hitTab = (int)SendMessageW(g_app->hTabControl, TCM_HITTEST, 0, (LPARAM)&hti);
                    if (hitTab == -1) {
                        // Check for double-click timing
                        static DWORD lastClick = 0;
                        static POINT lastPt = {0, 0};
                        DWORD now = GetTickCount();
                        if ((now - lastClick) <= GetDoubleClickTime() &&
                            abs(pt.x - lastPt.x) <= GetSystemMetrics(SM_CXDOUBLECLK) &&
                            abs(pt.y - lastPt.y) <= GetSystemMetrics(SM_CYDOUBLECLK)) {
                            App_CreateTab(L"Untitled");
                            lastClick = 0;
                        } else {
                            lastClick = now;
                            lastPt = pt;
                        }
                    }
                }
            }
            break;

        case WM_SETFOCUS:
            // Forward focus to active editor
            {
                Tab* tab = App_GetActiveTab();
                if (tab && tab->hEditor) {
                    SetFocus(tab->hEditor);
                }
            }
            return 0;

        case WM_INITMENUPOPUP:
            {
                HMENU hMenu = (HMENU)wParam;
                int menuIndex = LOWORD(lParam);
                switch (menuIndex) {
                    case 0: MenuBar_UpdateFileMenu(hMenu); break;
                    case 1: MenuBar_UpdateEditMenu(hMenu); break;
                    case 2: MenuBar_UpdateFormatMenu(hMenu); break;
                    case 3: MenuBar_UpdateViewMenu(hMenu); break;
                    case 4: MenuBar_UpdateSettingsMenu(hMenu); break;
                }
            }
            return 0;

        case WM_CONTEXTMENU:
            {
                HWND hTarget = (HWND)wParam;
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                POINT pt = { x, y };

                // Check if right-click is on tab control
                if (hTarget == g_app->hTabControl) {
                    TabControl_OnRightClick(x, y);
                    return 0;
                }

                // Check if click is in tab control area (even if on empty space)
                RECT rcTab;
                GetWindowRect(g_app->hTabControl, &rcTab);
                if (PtInRect(&rcTab, pt)) {
                    TabControl_OnRightClick(x, y);
                    return 0;
                }

                // Check if right-click is on editor (by target or by position)
                Tab* tab = App_GetActiveTab();
                if (tab && tab->hEditor) {
                    RECT rcEditor;
                    GetWindowRect(tab->hEditor, &rcEditor);
                    if (hTarget == tab->hEditor || PtInRect(&rcEditor, pt)) {
                        MainWindow_ShowEditorContextMenu(hwnd, x, y);
                        return 0;
                    }
                }
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);

        case WM_APP_UPDATE_STATUS:
            {
                Tab* tab = App_GetActiveTab();
                if (tab && tab->hEditor) {
                    int line, col;
                    Editor_GetCursorPos(tab->hEditor, &line, &col);
                    StatusBar_UpdatePosition(line, col);
                }
            }
            return 0;

        case WM_APP_TRAY_CALLBACK:
            if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
                TrayIcon_Remove();
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            }
            return 0;

        case WM_APP_DOC_MODIFIED:
            {
                Tab* tab = App_GetActiveTab();
                if (tab && tab->document) {
                    tab->document->modified = TRUE;
                    TabControl_UpdateTabTitle(tab->index);
                    MainWindow_UpdateTitle();
                    StatusBar_UpdateModified(TRUE);
                }
            }
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// New tab button handle
static HWND g_hNewTabBtn = NULL;

// Update position of the new tab button (call after adding/removing tabs)
void MainWindow_UpdateNewTabButton(void) {
    if (!g_hNewTabBtn || !g_app->hTabControl) return;

    int btnWidth = 24;
    int tabHeight = 24;
    int btnX = 4;

    int tabCount = (int)SendMessageW(g_app->hTabControl, TCM_GETITEMCOUNT, 0, 0);
    if (tabCount > 0) {
        RECT rcTab;
        SendMessageW(g_app->hTabControl, TCM_GETITEMRECT, tabCount - 1, (LPARAM)&rcTab);
        btnX = rcTab.right + 2;
    }
    SetWindowPos(g_hNewTabBtn, HWND_TOP, btnX, 2, btnWidth, tabHeight - 4, 0);
}

// Create child controls
static void OnCreate(HWND hwnd) {
    // Create tab control
    g_app->hTabControl = TabControl_Create(hwnd);

    // Create "+" button for new tab
    g_hNewTabBtn = CreateWindowExW(
        0, L"BUTTON", L"+",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 24, 22,
        hwnd, (HMENU)(INT_PTR)IDC_NEW_TAB_BTN,
        g_app->hInstance, NULL
    );
    if (g_hNewTabBtn) {
        // Use same font as tab control
        SendMessageW(g_hNewTabBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }

    // Create status bar
    g_app->hStatusBar = StatusBar_Create(hwnd);
    StatusBar_Show(g_app->showStatusBar);

    // Set up status update timer
    SetTimer(hwnd, TIMER_STATUS_UPDATE, 100, NULL);
}

// Cleanup on destroy
static void OnDestroy(HWND hwnd) {
    (void)hwnd;
    TrayIcon_Remove();
    KillTimer(hwnd, TIMER_STATUS_UPDATE);
    PostQuitMessage(0);
}

// Handle link click - navigate to target tab
static void HandleLinkClick(int position) {
    Tab* tab = App_GetActiveTab();
    if (!tab || !tab->document) return;

    // Get link at this position
    Link* link = NULL;
    if (tab->document->type == DOC_TYPE_FILE) {
        link = Links_GetAtPosition(DOC_TYPE_FILE, tab->document->filePath, 0, position);
    } else if (tab->document->type == DOC_TYPE_NOTE) {
        link = Links_GetAtPosition(DOC_TYPE_NOTE, NULL, tab->document->noteId, position);
    }

    if (!link) return;

    // Handle URL links - open in browser
    if (link->targetType == LINK_TARGET_URL && link->targetURL[0]) {
        ShellExecuteW(NULL, L"open", link->targetURL, NULL, NULL, SW_SHOWNORMAL);
        Links_Free(link);
        return;
    }

    // Find the target tab for file/note links
    for (int i = 0; i < MAX_TABS; i++) {
        if (!g_app->tabs[i] || !g_app->tabs[i]->document) continue;

        BOOL match = FALSE;
        if (link->targetType == LINK_TARGET_FILE && g_app->tabs[i]->document->type == DOC_TYPE_FILE) {
            match = (_wcsicmp(g_app->tabs[i]->document->filePath, link->targetPath) == 0);
        } else if (link->targetType == LINK_TARGET_NOTE && g_app->tabs[i]->document->type == DOC_TYPE_NOTE) {
            match = (g_app->tabs[i]->document->noteId == link->targetNoteId);
        }

        if (match) {
            App_SetActiveTab(i);
            break;
        }
    }

    Links_Free(link);
}

// Handle WM_NOTIFY
static void OnNotify(HWND hwnd, int idCtrl, LPNMHDR pnmh) {
    (void)hwnd;
    (void)idCtrl;

    if (pnmh->hwndFrom == g_app->hTabControl) {
        switch (pnmh->code) {
            case TCN_SELCHANGE:
                TabControl_OnSelChange();
                break;
            case NM_RCLICK:
                {
                    POINT pt;
                    GetCursorPos(&pt);
                    TabControl_OnRightClick(pt.x, pt.y);
                }
                break;
            case NM_DBLCLK:
                {
                    // Double-click on empty area creates new tab
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(g_app->hTabControl, &pt);
                    TCHITTESTINFO hti = { .pt = pt };
                    int hitTab = (int)SendMessageW(g_app->hTabControl, TCM_HITTEST, 0, (LPARAM)&hti);
                    if (hitTab == -1) {
                        // Clicked on empty space, create new tab
                        App_CreateTab(L"Untitled");
                    }
                }
                break;
        }
    }

    // Scintilla notifications
    Tab* tab = App_GetActiveTab();
    if (tab && pnmh->hwndFrom == tab->hEditor) {
        SCNotification* scn = (SCNotification*)pnmh;

        switch (pnmh->code) {
            case SCN_MODIFIED:
                PostMessageW(hwnd, WM_APP_DOC_MODIFIED, 0, 0);
                PostMessageW(hwnd, WM_APP_UPDATE_STATUS, 0, 0);
                break;

            case SCN_UPDATEUI:
                PostMessageW(hwnd, WM_APP_UPDATE_STATUS, 0, 0);
                break;

            case SCN_INDICATORCLICK:
                // Check if click was on link indicator (indicator 8)
                {
                    int indicators = (int)SendMessage(tab->hEditor, SCI_INDICATORALLONFOR, scn->position, 0);
                    if (indicators & (1 << 8)) {  // INDICATOR_LINK = 8
                        HandleLinkClick(scn->position);
                    }
                }
                break;
        }
    }

    // Legacy RichEdit notifications (kept for compatibility)
    if (pnmh->code == EN_CHANGE || pnmh->code == EN_SELCHANGE) {
        if (tab && pnmh->hwndFrom == tab->hEditor) {
            if (pnmh->code == EN_CHANGE) {
                PostMessageW(hwnd, WM_APP_DOC_MODIFIED, 0, 0);
            }
            PostMessageW(hwnd, WM_APP_UPDATE_STATUS, 0, 0);
        }
    }
}

// Handle WM_SIZE
void MainWindow_OnSize(HWND hwnd, UINT state, int cx, int cy) {
    (void)hwnd;
    if (state == SIZE_MINIMIZED) {
        if (g_app->minimizeToTray) {
            TrayIcon_Add(hwnd);
            ShowWindow(hwnd, SW_HIDE);
        }
        return;
    }

    // Position status bar
    if (g_app->hStatusBar && g_app->showStatusBar) {
        StatusBar_Resize(hwnd);
    }

    // Get status bar height
    int statusHeight = 0;
    if (g_app->hStatusBar && g_app->showStatusBar) {
        RECT rcStatus;
        GetWindowRect(g_app->hStatusBar, &rcStatus);
        statusHeight = rcStatus.bottom - rcStatus.top;
    }

    // Fixed tab control height and new tab button
    int tabHeight = 24;
    int btnWidth = 24;
    if (g_app->hTabControl) {
        SetWindowPos(g_app->hTabControl, NULL, 0, 0, cx, tabHeight, SWP_NOZORDER);
    }
    if (g_hNewTabBtn) {
        // Position button right after the last tab
        int btnX = 4;  // Default position if no tabs
        int tabCount = (int)SendMessageW(g_app->hTabControl, TCM_GETITEMCOUNT, 0, 0);
        if (tabCount > 0) {
            RECT rcTab;
            SendMessageW(g_app->hTabControl, TCM_GETITEMRECT, tabCount - 1, (LPARAM)&rcTab);
            btnX = rcTab.right + 2;
        }
        SetWindowPos(g_hNewTabBtn, HWND_TOP, btnX, 2, btnWidth, tabHeight - 4, 0);
    }

    // Position editor
    Tab* tab = App_GetActiveTab();
    if (tab && tab->hEditor) {
        SetWindowPos(tab->hEditor, NULL, 0, tabHeight, cx, cy - tabHeight - statusHeight, SWP_NOZORDER);
    }
}

// Handle WM_CLOSE
void MainWindow_OnClose(HWND hwnd) {
    // If auto-save on exit is enabled, skip prompts - session will be saved
    if (!g_app->autoSaveSession) {
        // Check for unsaved changes in all tabs
        for (int i = 0; i < MAX_TABS; i++) {
            if (g_app->tabs[i] && g_app->tabs[i]->document && g_app->tabs[i]->document->modified) {
                App_SetActiveTab(i);
                int result = Dialogs_SaveChanges(hwnd, Document_GetTitle(g_app->tabs[i]->document));
                if (result == IDCANCEL) return;
                if (result == IDYES) {
                    if (!Document_Save(g_app->tabs[i]->document, g_app->tabs[i]->hEditor)) {
                        return;  // Save failed
                    }
                }
            }
        }
    }

    DestroyWindow(hwnd);
}

// Handle WM_QUERYENDSESSION
BOOL MainWindow_OnQueryEndSession(HWND hwnd) {
    // Check for unsaved changes
    for (int i = 0; i < MAX_TABS; i++) {
        if (g_app->tabs[i] && g_app->tabs[i]->document && g_app->tabs[i]->document->modified) {
            return FALSE;  // Block shutdown
        }
    }
    (void)hwnd;
    return TRUE;
}

// Handle WM_COMMAND
void MainWindow_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
    (void)hwndCtl;
    (void)codeNotify;

    Tab* tab = App_GetActiveTab();
    Document* doc = tab ? tab->document : NULL;
    HWND hEditor = tab ? tab->hEditor : NULL;

    switch (id) {
        // File menu
        case IDM_FILE_NEW:
        case IDC_NEW_TAB_BTN:
            App_CreateTab(L"Untitled");
            break;

        case IDM_FILE_OPEN:
            {
                WCHAR path[MAX_PATH] = {0};
                if (Dialogs_OpenFile(hwnd, path, MAX_PATH)) {
                    Document* newDoc = Document_CreateFromFile(path);
                    if (newDoc) {
                        // If current tab is empty and unmodified, reuse it
                        if (tab && doc && doc->isNew && !doc->modified) {
                            Document_Destroy(doc);
                            tab->document = newDoc;
                            Document_Load(newDoc, hEditor);
                        } else {
                            int idx = App_CreateTab(NULL);
                            if (idx >= 0) {
                                Document_Destroy(g_app->tabs[idx]->document);
                                g_app->tabs[idx]->document = newDoc;
                                Document_Load(newDoc, g_app->tabs[idx]->hEditor);
                            }
                        }
                        TabControl_UpdateTabTitle(g_app->activeTab);
                        MainWindow_UpdateTitle();
                        App_AddRecentFile(path);
                    }
                }
            }
            break;

        case IDM_FILE_OPEN_NOTE:
            {
                int noteId = 0;
                if (Dialogs_NotesBrowser(hwnd, &noteId) && noteId > 0) {
                    Document* newDoc = Document_CreateFromNote(noteId);
                    if (newDoc) {
                        if (tab && doc && doc->isNew && !doc->modified) {
                            Document_Destroy(doc);
                            tab->document = newDoc;
                            Document_Load(newDoc, hEditor);
                        } else {
                            int idx = App_CreateTab(NULL);
                            if (idx >= 0) {
                                Document_Destroy(g_app->tabs[idx]->document);
                                g_app->tabs[idx]->document = newDoc;
                                Document_Load(newDoc, g_app->tabs[idx]->hEditor);
                            }
                        }
                        TabControl_UpdateTabTitle(g_app->activeTab);
                        MainWindow_UpdateTitle();
                    }
                }
            }
            break;

        case IDM_FILE_SAVE:
            if (doc && hEditor) {
                Document_Save(doc, hEditor);
                TabControl_UpdateTabTitle(tab->index);
                MainWindow_UpdateTitle();
                StatusBar_UpdateModified(doc->modified);
            }
            break;

        case IDM_FILE_SAVEAS:
            if (doc && hEditor) {
                WCHAR path[MAX_PATH] = {0};
                if (Dialogs_SaveFile(hwnd, path, MAX_PATH, Document_GetTitle(doc))) {
                    Document_SaveAs(doc, hEditor, path);
                    TabControl_UpdateTabTitle(tab->index);
                    MainWindow_UpdateTitle();
                    StatusBar_UpdateModified(doc->modified);
                    App_AddRecentFile(path);
                }
            }
            break;

        case IDM_FILE_EXPORT:
            // Export a database note to a file
            if (doc && hEditor && doc->type == DOC_TYPE_NOTE) {
                WCHAR path[MAX_PATH] = {0};
                if (Dialogs_SaveFile(hwnd, path, MAX_PATH, doc->noteTitle)) {
                    WCHAR* content = Editor_GetText(hEditor);
                    if (content) {
                        if (FileIO_WriteFile(path, content, ENCODING_UTF8)) {
                            MessageBoxW(hwnd, L"Note exported successfully!", APP_NAME, MB_ICONINFORMATION);
                            App_AddRecentFile(path);
                        } else {
                            MessageBoxW(hwnd, L"Failed to export note.", APP_NAME, MB_ICONERROR);
                        }
                        free(content);
                    }
                }
            }
            break;

        case IDM_FILE_CLOSE_TAB:
            if (g_app->tabCount > 0) {
                App_CloseTab(g_app->activeTab);
            }
            break;

        case IDM_FILE_PRINT:
            if (hEditor) {
                PRINTDLGW pd = {
                    .lStructSize = sizeof(pd),
                    .hwndOwner = hwnd,
                    .Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION
                };

                if (PrintDlgW(&pd)) {
                    HDC hDC = pd.hDC;
                    DOCINFOW di = {
                        .cbSize = sizeof(di),
                        .lpszDocName = doc ? Document_GetTitle(doc) : L"SuperNote Document"
                    };

                    if (StartDocW(hDC, &di) > 0) {
                        // Get text
                        WCHAR* text = Editor_GetText(hEditor);
                        if (text) {
                            // Simple print - one page with text
                            StartPage(hDC);

                            // Set up font
                            HFONT hPrintFont = CreateFontW(
                                -MulDiv(10, GetDeviceCaps(hDC, LOGPIXELSY), 72),
                                0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
                            );
                            HFONT hOldFont = (HFONT)SelectObject(hDC, hPrintFont);

                            // Calculate margins
                            int marginX = GetDeviceCaps(hDC, LOGPIXELSX);  // 1 inch
                            int marginY = GetDeviceCaps(hDC, LOGPIXELSY);  // 1 inch
                            int pageWidth = GetDeviceCaps(hDC, HORZRES) - 2 * marginX;
                            int pageHeight = GetDeviceCaps(hDC, VERTRES) - 2 * marginY;

                            RECT rc = { marginX, marginY, marginX + pageWidth, marginY + pageHeight };

                            // Draw text (wrapping)
                            DrawTextW(hDC, text, -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EXPANDTABS);

                            SelectObject(hDC, hOldFont);
                            DeleteObject(hPrintFont);

                            EndPage(hDC);
                            free(text);
                        }
                        EndDoc(hDC);
                    }
                    DeleteDC(hDC);
                }

                if (pd.hDevMode) GlobalFree(pd.hDevMode);
                if (pd.hDevNames) GlobalFree(pd.hDevNames);
            }
            break;

        case IDM_FILE_PRINT_PREVIEW:
            if (hEditor) {
                Dialogs_PrintPreview(hwnd, hEditor);
            }
            break;

        case IDM_FILE_EXIT:
            SendMessageW(hwnd, WM_CLOSE, 0, 0);
            break;

        // Edit menu
        case IDM_EDIT_UNDO:
            if (hEditor) Editor_Undo(hEditor);
            break;

        case IDM_EDIT_REDO:
            if (hEditor) Editor_Redo(hEditor);
            break;

        case IDM_EDIT_CUT:
            if (hEditor) Editor_Cut(hEditor);
            break;

        case IDM_EDIT_COPY:
            if (hEditor) Editor_Copy(hEditor);
            break;

        case IDM_EDIT_PASTE:
            if (hEditor) Editor_Paste(hEditor);
            break;

        case IDM_EDIT_DELETE:
            if (hEditor) SendMessageW(hEditor, WM_CLEAR, 0, 0);
            break;

        case IDM_EDIT_FIND:
            Dialogs_Find(hwnd);
            break;

        case IDM_EDIT_FIND_NEXT:
            if (hEditor && g_app->findText[0]) {
                Editor_FindText(hEditor, g_app->findText, g_app->matchCase, g_app->wholeWord, TRUE);
            }
            break;

        case IDM_EDIT_FIND_PREV:
            if (hEditor && g_app->findText[0]) {
                Editor_FindText(hEditor, g_app->findText, g_app->matchCase, g_app->wholeWord, FALSE);
            }
            break;

        case IDM_EDIT_REPLACE:
            Dialogs_Replace(hwnd);
            break;

        case IDM_EDIT_FIND_IN_TABS:
            Dialogs_FindInTabs(hwnd);
            break;

        case IDM_EDIT_REPLACE_IN_TABS:
            Dialogs_ReplaceInTabs(hwnd);
            break;

        case IDM_EDIT_GOTO:
            {
                int line = 0;
                if (Dialogs_GoToLine(hwnd, &line) && hEditor) {
                    Editor_GotoLine(hEditor, line);
                }
            }
            break;

        case IDM_EDIT_SELECT_ALL:
            if (hEditor) {
                SendMessageW(hEditor, EM_SETSEL, 0, -1);
            }
            break;

        case IDM_EDIT_TIME_DATE:
            if (hEditor) {
                Editor_InsertTimeDate(hEditor);
            }
            break;

        // Format menu
        case IDM_FORMAT_WORDWRAP:
            g_app->wordWrap = !g_app->wordWrap;
            for (int i = 0; i < MAX_TABS; i++) {
                if (g_app->tabs[i] && g_app->tabs[i]->hEditor) {
                    Editor_SetWordWrap(g_app->tabs[i]->hEditor, g_app->wordWrap);
                }
            }
            break;

        case IDM_FORMAT_FONT:
            if (Dialogs_Font(hwnd, &g_app->editorFont)) {
                if (g_app->hEditorFont) DeleteObject(g_app->hEditorFont);
                g_app->hEditorFont = CreateFontIndirectW(&g_app->editorFont);
                for (int i = 0; i < MAX_TABS; i++) {
                    if (g_app->tabs[i] && g_app->tabs[i]->hEditor) {
                        Editor_SetFont(g_app->tabs[i]->hEditor, g_app->hEditorFont);
                    }
                }
            }
            break;

        case IDM_FORMAT_TABSIZE_2:
            g_app->tabSize = 2;
            for (int i = 0; i < MAX_TABS; i++) {
                if (g_app->tabs[i] && g_app->tabs[i]->hEditor) {
                    Editor_SetTabSize(g_app->tabs[i]->hEditor, g_app->tabSize);
                }
            }
            break;

        case IDM_FORMAT_TABSIZE_4:
            g_app->tabSize = 4;
            for (int i = 0; i < MAX_TABS; i++) {
                if (g_app->tabs[i] && g_app->tabs[i]->hEditor) {
                    Editor_SetTabSize(g_app->tabs[i]->hEditor, g_app->tabSize);
                }
            }
            break;

        case IDM_FORMAT_TABSIZE_8:
            g_app->tabSize = 8;
            for (int i = 0; i < MAX_TABS; i++) {
                if (g_app->tabs[i] && g_app->tabs[i]->hEditor) {
                    Editor_SetTabSize(g_app->tabs[i]->hEditor, g_app->tabSize);
                }
            }
            break;

        // View menu
        case IDM_VIEW_ZOOM_IN:
            g_app->zoomLevel = min(500, g_app->zoomLevel + 10);
            if (hEditor) Editor_SetZoom(hEditor, g_app->zoomLevel);
            break;

        case IDM_VIEW_ZOOM_OUT:
            g_app->zoomLevel = max(10, g_app->zoomLevel - 10);
            if (hEditor) Editor_SetZoom(hEditor, g_app->zoomLevel);
            break;

        case IDM_VIEW_ZOOM_RESET:
            g_app->zoomLevel = 100;
            if (hEditor) Editor_SetZoom(hEditor, g_app->zoomLevel);
            break;

        case IDM_VIEW_STATUSBAR:
            g_app->showStatusBar = !g_app->showStatusBar;
            StatusBar_Show(g_app->showStatusBar);
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                MainWindow_OnSize(hwnd, SIZE_RESTORED, rc.right, rc.bottom);
            }
            break;

        case IDM_VIEW_NOTES_BROWSER:
            {
                int noteId = 0;
                if (Dialogs_NotesBrowser(hwnd, &noteId) && noteId > 0) {
                    SendMessageW(hwnd, WM_COMMAND, IDM_FILE_OPEN_NOTE, 0);
                }
            }
            break;

        case IDM_VIEW_ALWAYS_ON_TOP:
            g_app->alwaysOnTop = !g_app->alwaysOnTop;
            SetWindowPos(hwnd, g_app->alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                         0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            break;

        // Settings menu
        case IDM_SETTINGS_DEFAULTS:
            Dialogs_Defaults(hwnd);
            break;

        // Help menu
        case IDM_HELP_ABOUT:
            Dialogs_About(hwnd);
            break;

        default:
            // Handle recent files (IDs 6000-6009)
            if (id >= 6000 && id < 6000 + MAX_RECENT) {
                int index = id - 6000;
                if (index < g_app->recentCount && g_app->recentFiles[index][0]) {
                    WCHAR* path = g_app->recentFiles[index];
                    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
                        Document* newDoc = Document_CreateFromFile(path);
                        if (newDoc) {
                            if (tab && doc && doc->isNew && !doc->modified) {
                                Document_Destroy(doc);
                                tab->document = newDoc;
                                Document_Load(newDoc, hEditor);
                            } else {
                                int idx = App_CreateTab(NULL);
                                if (idx >= 0) {
                                    Document_Destroy(g_app->tabs[idx]->document);
                                    g_app->tabs[idx]->document = newDoc;
                                    Document_Load(newDoc, g_app->tabs[idx]->hEditor);
                                }
                            }
                            TabControl_UpdateTabTitle(g_app->activeTab);
                            MainWindow_UpdateTitle();
                            App_AddRecentFile(path);
                        }
                    } else {
                        MessageBoxW(hwnd, L"File not found.", APP_NAME, MB_ICONWARNING);
                    }
                }
            }
            break;
    }
}

// Update window title
void MainWindow_UpdateTitle(void) {
    WCHAR title[MAX_PATH + 64];
    Document* doc = App_GetActiveDocument();

    if (doc) {
        if (doc->modified) {
            swprintf_s(title, sizeof(title)/sizeof(WCHAR), L"*%s - %s", Document_GetTitle(doc), APP_NAME);
        } else {
            swprintf_s(title, sizeof(title)/sizeof(WCHAR), L"%s - %s", Document_GetTitle(doc), APP_NAME);
        }
    } else {
        wcscpy_s(title, sizeof(title)/sizeof(WCHAR), APP_NAME);
    }

    SetWindowTextW(g_app->hMainWindow, title);
}

// Update menu state
void MainWindow_UpdateMenuState(void) {
    // Menu state is updated in WM_INITMENUPOPUP handlers in menubar.c
}

// Get word at position for spell check
static WCHAR* GetWordAtPosition(HWND hEditor, int pos, int* wordStart, int* wordEnd) {
    int lineNum = (int)SendMessage(hEditor, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)SendMessage(hEditor, SCI_POSITIONFROMLINE, lineNum, 0);
    int lineEnd = (int)SendMessage(hEditor, SCI_GETLINEENDPOSITION, lineNum, 0);

    // Get line text
    int lineLen = lineEnd - lineStart;
    if (lineLen <= 0) return NULL;

    char* lineUtf8 = (char*)malloc(lineLen + 1);
    if (!lineUtf8) return NULL;

    struct Sci_TextRange tr;
    tr.chrg.cpMin = lineStart;
    tr.chrg.cpMax = lineEnd;
    tr.lpstrText = lineUtf8;
    SendMessage(hEditor, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);

    // Find word boundaries
    int relPos = pos - lineStart;
    int wStart = relPos;
    int wEnd = relPos;

    // Find start of word
    while (wStart > 0 && (isalnum((unsigned char)lineUtf8[wStart - 1]) || lineUtf8[wStart - 1] == '\'')) {
        wStart--;
    }

    // Find end of word
    while (wEnd < lineLen && (isalnum((unsigned char)lineUtf8[wEnd]) || lineUtf8[wEnd] == '\'')) {
        wEnd++;
    }

    if (wStart >= wEnd) {
        free(lineUtf8);
        return NULL;
    }

    // Extract word
    char wordUtf8[256];
    int wordLen = wEnd - wStart;
    if (wordLen >= sizeof(wordUtf8)) wordLen = sizeof(wordUtf8) - 1;
    memcpy(wordUtf8, lineUtf8 + wStart, wordLen);
    wordUtf8[wordLen] = '\0';

    free(lineUtf8);

    *wordStart = lineStart + wStart;
    *wordEnd = lineStart + wEnd;

    // Convert to wide
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, wordUtf8, -1, NULL, 0);
    WCHAR* word = (WCHAR*)malloc(wideLen * sizeof(WCHAR));
    if (word) {
        MultiByteToWideChar(CP_UTF8, 0, wordUtf8, -1, word, wideLen);
    }
    return word;
}

// Show editor context menu
void MainWindow_ShowEditorContextMenu(HWND hwnd, int x, int y) {
    Tab* tab = App_GetActiveTab();
    HWND hEditor = tab ? tab->hEditor : NULL;
    if (!hEditor) return;

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Check selection state
    int start, end;
    Editor_GetSelection(hEditor, &start, &end);
    BOOL hasSelection = (start != end);
    BOOL canPaste = IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(CF_TEXT);

    // Get position under cursor for spell check (x,y are screen coords, function handles conversion)
    int clickPos = Editor_GetPositionFromPoint(hEditor, x, y);

    // Check if there's a spell error at this position (indicator 9)
    int indicators = (int)SendMessage(hEditor, SCI_INDICATORALLONFOR, clickPos, 0);
    BOOL hasSpellError = (indicators & (1 << 9)) != 0;  // INDICATOR_SPELL = 9

    // Spell suggestions at top if there's an error
    WCHAR* misspelledWord = NULL;
    WCHAR** suggestions = NULL;
    int suggestionCount = 0;
    int spellWordStart = 0, spellWordEnd = 0;

    if (hasSpellError) {
        misspelledWord = GetWordAtPosition(hEditor, clickPos, &spellWordStart, &spellWordEnd);
        if (misspelledWord) {
            suggestions = Editor_GetSpellSuggestions(misspelledWord, &suggestionCount);

            if (suggestionCount > 0) {
                for (int i = 0; i < suggestionCount && i < 5; i++) {
                    AppendMenuW(hMenu, MF_STRING, IDM_SPELL_SUGGESTION_BASE + i, suggestions[i]);
                }
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            }

            // Add to dictionary / Ignore options
            WCHAR addText[MAX_TITLE_LEN];
            swprintf_s(addText, MAX_TITLE_LEN, L"Add \"%s\" to Dictionary", misspelledWord);
            AppendMenuW(hMenu, MF_STRING, IDM_SPELL_ADD_DICT, addText);
            AppendMenuW(hMenu, MF_STRING, IDM_SPELL_IGNORE, L"Ignore");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        }
    }

    // Standard edit items
    AppendMenuW(hMenu, MF_STRING | (Editor_CanUndo(hEditor) ? 0 : MF_GRAYED), IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");
    AppendMenuW(hMenu, MF_STRING | (Editor_CanRedo(hEditor) ? 0 : MF_GRAYED), IDM_EDIT_REDO, L"&Redo\tCtrl+Y");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenuW(hMenu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenuW(hMenu, MF_STRING | (canPaste ? 0 : MF_GRAYED), IDM_EDIT_PASTE, L"&Paste\tCtrl+V");
    AppendMenuW(hMenu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_EDIT_DELETE, L"&Delete\tDel");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EDIT_SELECT_ALL, L"Select &All\tCtrl+A");

    // Spell check document option
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_SPELL_CHECK_DOC, L"Check &Spelling");

    // Link submenu - only if there's a selection
    if (hasSelection) {
        HMENU hLinkMenu = CreatePopupMenu();
        int linkItems = 0;

        // Add tabs as link targets
        for (int i = 0; i < MAX_TABS; i++) {
            if (i != g_app->activeTab && g_app->tabs[i] && g_app->tabs[i]->document) {
                WCHAR title[MAX_TITLE_LEN + 8];
                swprintf_s(title, sizeof(title)/sizeof(WCHAR), L"&%d. %s", linkItems + 1,
                          Document_GetTitle(g_app->tabs[i]->document));
                AppendMenuW(hLinkMenu, MF_STRING, IDM_LINK_TAB_BASE + i, title);
                linkItems++;
            }
        }

        // Add URL option
        if (linkItems > 0) {
            AppendMenuW(hLinkMenu, MF_SEPARATOR, 0, NULL);
        }
        AppendMenuW(hLinkMenu, MF_STRING, IDM_LINK_URL, L"&URL...");

        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hLinkMenu, L"&Link to");

        // Send to Shell submenu
        HMENU hShellMenu = CreatePopupMenu();
        AppendMenuW(hShellMenu, MF_STRING, IDM_SHELL_CMD, L"Run in &CMD");
        AppendMenuW(hShellMenu, MF_STRING, IDM_SHELL_CMD_ADMIN, L"Run in CMD (Admin)");
        AppendMenuW(hShellMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hShellMenu, MF_STRING, IDM_SHELL_POWERSHELL, L"Run in &PowerShell");
        AppendMenuW(hShellMenu, MF_STRING, IDM_SHELL_PS_ADMIN, L"Run in PowerShell (Admin)");
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hShellMenu, L"&Send to Shell");
    }

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hwnd, NULL);

    if (cmd) {
        // Handle spell suggestion commands
        if (cmd >= IDM_SPELL_SUGGESTION_BASE && cmd < IDM_SPELL_SUGGESTION_BASE + 10) {
            int suggIdx = cmd - IDM_SPELL_SUGGESTION_BASE;
            if (suggIdx < suggestionCount && suggestions[suggIdx]) {
                // Replace the misspelled word with suggestion
                SendMessage(hEditor, SCI_SETTARGETSTART, spellWordStart, 0);
                SendMessage(hEditor, SCI_SETTARGETEND, spellWordEnd, 0);

                char suggUtf8[256];
                WideCharToMultiByte(CP_UTF8, 0, suggestions[suggIdx], -1, suggUtf8, sizeof(suggUtf8), NULL, NULL);
                SendMessage(hEditor, SCI_REPLACETARGET, -1, (LPARAM)suggUtf8);

                // Re-check spelling
                Editor_CheckSpelling(hEditor);
            }
        }
        // Handle add to dictionary
        else if (cmd == IDM_SPELL_ADD_DICT) {
            if (misspelledWord) {
                Editor_AddWordToDictionary(misspelledWord);
                Editor_CheckSpelling(hEditor);
            }
        }
        // Handle ignore
        else if (cmd == IDM_SPELL_IGNORE) {
            // Clear indicator for this word only
            SendMessage(hEditor, SCI_SETINDICATORCURRENT, 9, 0);  // INDICATOR_SPELL
            SendMessage(hEditor, SCI_INDICATORCLEARRANGE, spellWordStart, spellWordEnd - spellWordStart);
        }
        // Handle spell check document
        else if (cmd == IDM_SPELL_CHECK_DOC) {
            Editor_CheckSpelling(hEditor);
        }
        // Handle link to URL
        else if (cmd == IDM_LINK_URL) {
            WCHAR* selected = Editor_GetSelectedText(hEditor);
            if (selected && tab->document) {
                // Prompt for URL
                WCHAR url[1024] = L"https://";
                if (Dialogs_InputBox(hwnd, L"Link to URL", L"Enter URL:", url, 1024)) {
                    Document* srcDoc = tab->document;

                    // Create the link in database (target_type = 2 for URL)
                    int linkId = Links_CreateURL(
                        srcDoc->type,
                        srcDoc->type == DOC_TYPE_FILE ? srcDoc->filePath : NULL,
                        srcDoc->type == DOC_TYPE_NOTE ? srcDoc->noteId : 0,
                        selected,
                        start, end,
                        url
                    );

                    if (linkId > 0) {
                        Editor_AddLinkIndicator(hEditor, start, end);
                        WCHAR msg[512];
                        swprintf_s(msg, 512, L"Link created: \"%s\" now links to:\n%s",
                                  selected, url);
                        MessageBoxW(hwnd, msg, L"Link Created", MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hwnd, L"Failed to create link.", APP_NAME, MB_ICONERROR);
                    }
                }
                free(selected);
            }
        }
        // Handle link to tab commands
        else if (cmd >= IDM_LINK_TAB_BASE && cmd < IDM_LINK_TAB_BASE + MAX_TABS) {
            int targetTab = cmd - IDM_LINK_TAB_BASE;
            if (g_app->tabs[targetTab] && g_app->tabs[targetTab]->document && tab->document) {
                WCHAR* selected = Editor_GetSelectedText(hEditor);
                if (selected) {
                    Document* srcDoc = tab->document;
                    Document* tgtDoc = g_app->tabs[targetTab]->document;

                    // Create the link in database
                    int linkId = Links_Create(
                        srcDoc->type,
                        srcDoc->type == DOC_TYPE_FILE ? srcDoc->filePath : NULL,
                        srcDoc->type == DOC_TYPE_NOTE ? srcDoc->noteId : 0,
                        selected,
                        start, end,
                        tgtDoc->type,
                        tgtDoc->type == DOC_TYPE_FILE ? tgtDoc->filePath : NULL,
                        tgtDoc->type == DOC_TYPE_NOTE ? tgtDoc->noteId : 0
                    );

                    if (linkId > 0) {
                        // Add visual indicator
                        Editor_AddLinkIndicator(hEditor, start, end);

                        WCHAR msg[512];
                        swprintf_s(msg, 512, L"Link created: \"%s\" now links to \"%s\"\n\nClick on the linked text to navigate.",
                                  selected, Document_GetTitle(tgtDoc));
                        MessageBoxW(hwnd, msg, L"Link Created", MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hwnd, L"Failed to create link.", APP_NAME, MB_ICONERROR);
                    }

                    free(selected);
                }
            }
        }
        // Handle shell commands
        else if (cmd == IDM_SHELL_CMD || cmd == IDM_SHELL_POWERSHELL ||
                 cmd == IDM_SHELL_CMD_ADMIN || cmd == IDM_SHELL_PS_ADMIN) {
            WCHAR* selected = Editor_GetSelectedText(hEditor);
            if (selected) {
                // Trim whitespace
                WCHAR* trimmed = selected;
                while (*trimmed == L' ' || *trimmed == L'\t' || *trimmed == L'\r' || *trimmed == L'\n') trimmed++;
                WCHAR* endTrim = trimmed + wcslen(trimmed) - 1;
                while (endTrim > trimmed && (*endTrim == L' ' || *endTrim == L'\t' || *endTrim == L'\r' || *endTrim == L'\n')) {
                    *endTrim = L'\0';
                    endTrim--;
                }

                if (*trimmed) {
                    BOOL isAdmin = (cmd == IDM_SHELL_CMD_ADMIN || cmd == IDM_SHELL_PS_ADMIN);
                    BOOL isPowerShell = (cmd == IDM_SHELL_POWERSHELL || cmd == IDM_SHELL_PS_ADMIN);

                    if (isAdmin) {
                        // Use ShellExecuteEx with runas for admin elevation
                        WCHAR params[MAX_PATH * 2];
                        if (isPowerShell) {
                            swprintf_s(params, MAX_PATH * 2, L"-NoExit -Command \"& '%s'\"", trimmed);
                        } else {
                            swprintf_s(params, MAX_PATH * 2, L"/c \"%s\" & pause", trimmed);
                        }

                        SHELLEXECUTEINFOW sei = {
                            .cbSize = sizeof(sei),
                            .fMask = SEE_MASK_NOCLOSEPROCESS,
                            .hwnd = hwnd,
                            .lpVerb = L"runas",
                            .lpFile = isPowerShell ? L"powershell.exe" : L"cmd.exe",
                            .lpParameters = params,
                            .nShow = SW_SHOWNORMAL
                        };

                        if (!ShellExecuteExW(&sei)) {
                            DWORD err = GetLastError();
                            if (err != ERROR_CANCELLED) {
                                MessageBoxW(hwnd, L"Failed to execute command as administrator.", APP_NAME, MB_ICONERROR);
                            }
                        } else if (sei.hProcess) {
                            CloseHandle(sei.hProcess);
                        }
                    } else {
                        // Regular execution
                        WCHAR cmdLine[MAX_PATH * 2];
                        if (isPowerShell) {
                            swprintf_s(cmdLine, MAX_PATH * 2, L"powershell.exe -NoExit -Command \"& '%s'\"", trimmed);
                        } else {
                            swprintf_s(cmdLine, MAX_PATH * 2, L"cmd.exe /c \"%s\" & pause", trimmed);
                        }

                        STARTUPINFOW si = { .cb = sizeof(si) };
                        PROCESS_INFORMATION pi = {0};

                        if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
                                           CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);
                        } else {
                            MessageBoxW(hwnd, L"Failed to execute command.", APP_NAME, MB_ICONERROR);
                        }
                    }
                }
                free(selected);
            }
        }
        else {
            SendMessageW(hwnd, WM_COMMAND, cmd, 0);
        }
    }

    // Cleanup
    if (suggestions) {
        Editor_FreeSpellSuggestions(suggestions, suggestionCount);
    }
    free(misspelledWord);
    DestroyMenu(hMenu);
}
