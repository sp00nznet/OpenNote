#include "supernote.h"
#include "res/resource.h"

// About dialog
void Dialogs_About(HWND hParent) {
    DialogBoxW(g_app->hInstance, MAKEINTRESOURCEW(IDD_ABOUT), hParent, AboutProc);
}

// About dialog procedure
INT_PTR CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (msg) {
        case WM_INITDIALOG:
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

// Go To Line dialog
BOOL Dialogs_GoToLine(HWND hParent, int* line) {
    return DialogBoxParamW(g_app->hInstance, MAKEINTRESOURCEW(IDD_GOTO), hParent, GoToLineProc, (LPARAM)line) == IDOK;
}

// Go To Line procedure
INT_PTR CALLBACK GoToLineProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int* pLine;

    switch (msg) {
        case WM_INITDIALOG:
            pLine = (int*)lParam;
            SetFocus(GetDlgItem(hwnd, IDC_GOTO_LINE));
            return FALSE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    {
                        WCHAR buffer[32];
                        GetDlgItemTextW(hwnd, IDC_GOTO_LINE, buffer, 32);
                        *pLine = _wtoi(buffer);
                        if (*pLine > 0) {
                            EndDialog(hwnd, IDOK);
                        } else {
                            MessageBoxW(hwnd, L"Invalid line number.", APP_NAME, MB_ICONWARNING);
                        }
                    }
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

// Find/Replace modeless dialog procedure
static UINT WM_FINDREPLACE;
static FINDREPLACEW g_fr;
static WCHAR g_findBuffer[256];
static WCHAR g_replaceBuffer[256];

static LRESULT CALLBACK FindReplaceSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)uIdSubclass;
    (void)dwRefData;

    if (msg == WM_FINDREPLACE) {
        if (g_fr.Flags & FR_DIALOGTERM) {
            g_app->hFindDialog = NULL;
            return 0;
        }

        Tab* tab = App_GetActiveTab();
        HWND hEditor = tab ? tab->hEditor : NULL;
        if (!hEditor) return 0;

        // Update search options
        wcscpy_s(g_app->findText, MAX_PATH, g_fr.lpstrFindWhat);
        wcscpy_s(g_app->replaceText, MAX_PATH, g_fr.lpstrReplaceWith);
        g_app->matchCase = (g_fr.Flags & FR_MATCHCASE) != 0;
        g_app->wholeWord = (g_fr.Flags & FR_WHOLEWORD) != 0;

        if (g_fr.Flags & FR_FINDNEXT) {
            BOOL forward = !(g_fr.Flags & FR_DOWN) ? FALSE : TRUE;
            Editor_FindText(hEditor, g_app->findText, g_app->matchCase, g_app->wholeWord, forward);
        } else if (g_fr.Flags & FR_REPLACE) {
            Editor_ReplaceText(hEditor, g_app->findText, g_app->replaceText, g_app->matchCase, g_app->wholeWord);
        } else if (g_fr.Flags & FR_REPLACEALL) {
            Editor_ReplaceAll(hEditor, g_app->findText, g_app->replaceText, g_app->matchCase, g_app->wholeWord);
        }

        return 0;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Find dialog
void Dialogs_Find(HWND hParent) {
    if (g_app->hFindDialog) {
        SetFocus(g_app->hFindDialog);
        return;
    }

    // Register message
    if (!WM_FINDREPLACE) {
        WM_FINDREPLACE = RegisterWindowMessageW(FINDMSGSTRINGW);
    }

    // Get selected text for find
    Tab* tab = App_GetActiveTab();
    if (tab && tab->hEditor) {
        WCHAR* selected = Editor_GetSelectedText(tab->hEditor);
        if (selected && selected[0] && wcslen(selected) < 256) {
            wcscpy_s(g_findBuffer, 256, selected);
        }
        free(selected);
    }

    // Initialize structure
    ZeroMemory(&g_fr, sizeof(g_fr));
    g_fr.lStructSize = sizeof(g_fr);
    g_fr.hwndOwner = hParent;
    g_fr.lpstrFindWhat = g_findBuffer;
    g_fr.lpstrReplaceWith = g_replaceBuffer;
    g_fr.wFindWhatLen = 256;
    g_fr.wReplaceWithLen = 256;
    g_fr.Flags = FR_DOWN;
    if (g_app->matchCase) g_fr.Flags |= FR_MATCHCASE;
    if (g_app->wholeWord) g_fr.Flags |= FR_WHOLEWORD;

    g_app->hFindDialog = FindTextW(&g_fr);

    // Subclass main window to receive find messages
    SetWindowSubclass(hParent, FindReplaceSubclass, 1, 0);
}

// Replace dialog
void Dialogs_Replace(HWND hParent) {
    if (g_app->hFindDialog) {
        SetFocus(g_app->hFindDialog);
        return;
    }

    // Register message
    if (!WM_FINDREPLACE) {
        WM_FINDREPLACE = RegisterWindowMessageW(FINDMSGSTRINGW);
    }

    // Get selected text for find
    Tab* tab = App_GetActiveTab();
    if (tab && tab->hEditor) {
        WCHAR* selected = Editor_GetSelectedText(tab->hEditor);
        if (selected && selected[0] && wcslen(selected) < 256) {
            wcscpy_s(g_findBuffer, 256, selected);
        }
        free(selected);
    }

    // Initialize structure
    ZeroMemory(&g_fr, sizeof(g_fr));
    g_fr.lStructSize = sizeof(g_fr);
    g_fr.hwndOwner = hParent;
    g_fr.lpstrFindWhat = g_findBuffer;
    g_fr.lpstrReplaceWith = g_replaceBuffer;
    g_fr.wFindWhatLen = 256;
    g_fr.wReplaceWithLen = 256;
    g_fr.Flags = FR_DOWN;
    if (g_app->matchCase) g_fr.Flags |= FR_MATCHCASE;
    if (g_app->wholeWord) g_fr.Flags |= FR_WHOLEWORD;

    g_app->hFindDialog = ReplaceTextW(&g_fr);

    // Subclass main window to receive find messages
    SetWindowSubclass(hParent, FindReplaceSubclass, 1, 0);
}

// Font dialog
BOOL Dialogs_Font(HWND hParent, LOGFONTW* font) {
    CHOOSEFONTW cf = {
        .lStructSize = sizeof(cf),
        .hwndOwner = hParent,
        .lpLogFont = font,
        .Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FIXEDPITCHONLY
    };

    return ChooseFontW(&cf);
}

// Open file dialog
BOOL Dialogs_OpenFile(HWND hParent, WCHAR* pathBuffer, int bufferSize) {
    pathBuffer[0] = L'\0';  // Must be initialized

    // Comprehensive file filter like Notepad++
    static const WCHAR filter[] =
        L"All Supported Files\0*.txt;*.c;*.cpp;*.h;*.hpp;*.cs;*.java;*.py;*.js;*.ts;*.json;*.xml;*.html;*.htm;*.css;*.md;*.yaml;*.yml;*.ini;*.cfg;*.conf;*.log;*.sql;*.sh;*.bat;*.cmd;*.ps1;*.rb;*.php;*.go;*.rs;*.swift;*.kt;*.lua;*.pl;*.r;*.m;*.asm;*.s\0"
        L"Text Files (*.txt)\0*.txt\0"
        L"C/C++ Files (*.c;*.cpp;*.h;*.hpp)\0*.c;*.cpp;*.h;*.hpp\0"
        L"C# Files (*.cs)\0*.cs\0"
        L"Java Files (*.java)\0*.java\0"
        L"Python Files (*.py)\0*.py\0"
        L"JavaScript/TypeScript (*.js;*.ts)\0*.js;*.ts\0"
        L"JSON Files (*.json)\0*.json\0"
        L"XML Files (*.xml)\0*.xml\0"
        L"HTML Files (*.html;*.htm)\0*.html;*.htm\0"
        L"CSS Files (*.css)\0*.css\0"
        L"Markdown Files (*.md)\0*.md\0"
        L"YAML Files (*.yaml;*.yml)\0*.yaml;*.yml\0"
        L"INI/Config Files (*.ini;*.cfg;*.conf)\0*.ini;*.cfg;*.conf\0"
        L"Log Files (*.log)\0*.log\0"
        L"SQL Files (*.sql)\0*.sql\0"
        L"Shell Scripts (*.sh;*.bat;*.cmd;*.ps1)\0*.sh;*.bat;*.cmd;*.ps1\0"
        L"Ruby Files (*.rb)\0*.rb\0"
        L"PHP Files (*.php)\0*.php\0"
        L"Go Files (*.go)\0*.go\0"
        L"Rust Files (*.rs)\0*.rs\0"
        L"All Files (*.*)\0*.*\0";

    OPENFILENAMEW ofn = {
        .lStructSize = sizeof(ofn),
        .hwndOwner = hParent,
        .lpstrFilter = filter,
        .nFilterIndex = 1,
        .lpstrFile = pathBuffer,
        .nMaxFile = bufferSize,
        .Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT
    };

    return GetOpenFileNameW(&ofn);
}

// Save file dialog
BOOL Dialogs_SaveFile(HWND hParent, WCHAR* pathBuffer, int bufferSize, const WCHAR* defaultName) {
    if (defaultName) {
        wcscpy_s(pathBuffer, bufferSize, defaultName);
    }

    // Determine default extension from filename if provided
    const WCHAR* defExt = L"txt";
    int filterIndex = 1;

    if (defaultName) {
        const WCHAR* ext = wcsrchr(defaultName, L'.');
        if (ext) {
            ext++;  // Skip the dot
            if (_wcsicmp(ext, L"c") == 0 || _wcsicmp(ext, L"cpp") == 0 ||
                _wcsicmp(ext, L"h") == 0 || _wcsicmp(ext, L"hpp") == 0) {
                filterIndex = 2;
                defExt = ext;
            } else if (_wcsicmp(ext, L"py") == 0) {
                filterIndex = 3;
                defExt = L"py";
            } else if (_wcsicmp(ext, L"js") == 0 || _wcsicmp(ext, L"ts") == 0) {
                filterIndex = 4;
                defExt = ext;
            } else if (_wcsicmp(ext, L"json") == 0) {
                filterIndex = 5;
                defExt = L"json";
            } else if (_wcsicmp(ext, L"html") == 0 || _wcsicmp(ext, L"htm") == 0) {
                filterIndex = 6;
                defExt = ext;
            } else if (_wcsicmp(ext, L"css") == 0) {
                filterIndex = 7;
                defExt = L"css";
            } else if (_wcsicmp(ext, L"md") == 0) {
                filterIndex = 8;
                defExt = L"md";
            } else if (_wcsicmp(ext, L"xml") == 0) {
                filterIndex = 9;
                defExt = L"xml";
            } else if (_wcsicmp(ext, L"sql") == 0) {
                filterIndex = 10;
                defExt = L"sql";
            }
        }
    }

    static const WCHAR filter[] =
        L"Text Files (*.txt)\0*.txt\0"
        L"C/C++ Files (*.c;*.cpp;*.h;*.hpp)\0*.c;*.cpp;*.h;*.hpp\0"
        L"Python Files (*.py)\0*.py\0"
        L"JavaScript/TypeScript (*.js;*.ts)\0*.js;*.ts\0"
        L"JSON Files (*.json)\0*.json\0"
        L"HTML Files (*.html;*.htm)\0*.html;*.htm\0"
        L"CSS Files (*.css)\0*.css\0"
        L"Markdown Files (*.md)\0*.md\0"
        L"XML Files (*.xml)\0*.xml\0"
        L"SQL Files (*.sql)\0*.sql\0"
        L"All Files (*.*)\0*.*\0";

    OPENFILENAMEW ofn = {
        .lStructSize = sizeof(ofn),
        .hwndOwner = hParent,
        .lpstrFilter = filter,
        .nFilterIndex = filterIndex,
        .lpstrFile = pathBuffer,
        .nMaxFile = bufferSize,
        .lpstrDefExt = defExt,
        .Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST
    };

    return GetSaveFileNameW(&ofn);
}

// Save changes confirmation dialog
int Dialogs_SaveChanges(HWND hParent, const WCHAR* filename) {
    WCHAR message[MAX_PATH + 64];
    swprintf_s(message, sizeof(message)/sizeof(WCHAR),
               L"Do you want to save changes to %s?", filename ? filename : L"Untitled");

    return MessageBoxW(hParent, message, APP_NAME, MB_YESNOCANCEL | MB_ICONWARNING);
}

// Notes Browser dialog
BOOL Dialogs_NotesBrowser(HWND hParent, int* noteId) {
    return DialogBoxParamW(g_app->hInstance, MAKEINTRESOURCEW(IDD_NOTES_BROWSER),
                           hParent, NotesBrowserProc, (LPARAM)noteId) == IDOK;
}

// Notes Browser procedure
INT_PTR CALLBACK NotesBrowserProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int* pNoteId;
    static HWND hList;
    static HWND hSearch;

    switch (msg) {
        case WM_INITDIALOG:
            pNoteId = (int*)lParam;
            hList = GetDlgItem(hwnd, IDC_NOTES_LIST);
            hSearch = GetDlgItem(hwnd, IDC_NOTES_SEARCH);

            // Set up list view columns
            {
                LVCOLUMNW col = {
                    .mask = LVCF_TEXT | LVCF_WIDTH,
                    .cx = 200,
                    .pszText = L"Title"
                };
                ListView_InsertColumn(hList, 0, &col);

                col.cx = 120;
                col.pszText = L"Updated";
                ListView_InsertColumn(hList, 1, &col);
            }

            // Load notes
            {
                NoteListItem* items = NULL;
                int count = 0;
                Notes_GetList(&items, &count);

                for (int i = 0; i < count; i++) {
                    LVITEMW item = {
                        .mask = LVIF_TEXT | LVIF_PARAM,
                        .iItem = i,
                        .pszText = items[i].title,
                        .lParam = items[i].id
                    };
                    int idx = ListView_InsertItem(hList, &item);
                    ListView_SetItemText(hList, idx, 1, items[i].updatedAt);
                }

                Notes_FreeList(items);
            }

            ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_NOTES_SEARCH:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        WCHAR query[256];
                        GetWindowTextW(hSearch, query, 256);

                        ListView_DeleteAllItems(hList);

                        NoteListItem* items = NULL;
                        int count = 0;

                        if (query[0]) {
                            Notes_Search(query, &items, &count);
                        } else {
                            Notes_GetList(&items, &count);
                        }

                        for (int i = 0; i < count; i++) {
                            LVITEMW item = {
                                .mask = LVIF_TEXT | LVIF_PARAM,
                                .iItem = i,
                                .pszText = items[i].title,
                                .lParam = items[i].id
                            };
                            int idx = ListView_InsertItem(hList, &item);
                            ListView_SetItemText(hList, idx, 1, items[i].updatedAt);
                        }

                        Notes_FreeList(items);
                    }
                    return TRUE;

                case IDC_NOTES_NEW:
                    {
                        int newId = Notes_Create(L"New Note", L"");
                        if (newId > 0) {
                            *pNoteId = newId;
                            EndDialog(hwnd, IDOK);
                        }
                    }
                    return TRUE;

                case IDC_NOTES_DELETE:
                    {
                        int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
                        if (sel >= 0) {
                            LVITEMW item = { .mask = LVIF_PARAM, .iItem = sel };
                            ListView_GetItem(hList, &item);
                            if (MessageBoxW(hwnd, L"Delete this note?", APP_NAME, MB_YESNO | MB_ICONQUESTION) == IDYES) {
                                Notes_Delete((int)item.lParam);
                                ListView_DeleteItem(hList, sel);
                            }
                        }
                    }
                    return TRUE;

                case IDOK:
                    {
                        int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
                        if (sel >= 0) {
                            LVITEMW item = { .mask = LVIF_PARAM, .iItem = sel };
                            ListView_GetItem(hList, &item);
                            *pNoteId = (int)item.lParam;
                            EndDialog(hwnd, IDOK);
                        }
                    }
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;

        case WM_NOTIFY:
            if (((LPNMHDR)lParam)->idFrom == IDC_NOTES_LIST) {
                if (((LPNMHDR)lParam)->code == NM_DBLCLK) {
                    SendMessageW(hwnd, WM_COMMAND, IDOK, 0);
                    return TRUE;
                }
            }
            break;

        case WM_SIZE:
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                // Resize list view
                SetWindowPos(hList, NULL, 10, 30, rc.right - 20, rc.bottom - 70, SWP_NOZORDER);
            }
            return TRUE;
    }
    return FALSE;
}

// Create find dialog (legacy - now using Windows common dialogs)
HWND CreateFindDialog(HWND hParent, BOOL showReplace) {
    if (showReplace) {
        Dialogs_Replace(hParent);
    } else {
        Dialogs_Find(hParent);
    }
    return g_app->hFindDialog;
}

// Set find text
void FindDialog_SetFindText(const WCHAR* text) {
    if (text) {
        wcscpy_s(g_app->findText, MAX_PATH, text);
    }
}

// Defaults dialog
void Dialogs_Defaults(HWND hParent) {
    DialogBoxW(g_app->hInstance, MAKEINTRESOURCEW(IDD_DEFAULTS), hParent, DefaultsProc);
}

// Defaults dialog procedure
INT_PTR CALLBACK DefaultsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG:
            {
                // Font size combobox
                HWND hFontCombo = GetDlgItem(hwnd, IDC_DEFAULTS_FONTSIZE);
                int sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32};
                int currentSize = -g_app->editorFont.lfHeight;
                int fontSelectIndex = 4;

                for (int i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
                    WCHAR text[16];
                    swprintf_s(text, 16, L"%d", sizes[i]);
                    int idx = (int)SendMessageW(hFontCombo, CB_ADDSTRING, 0, (LPARAM)text);
                    SendMessageW(hFontCombo, CB_SETITEMDATA, idx, sizes[i]);
                    if (sizes[i] == currentSize) {
                        fontSelectIndex = idx;
                    }
                }
                SendMessageW(hFontCombo, CB_SETCURSEL, fontSelectIndex, 0);

                // Theme combobox
                HWND hThemeCombo = GetDlgItem(hwnd, IDC_DEFAULTS_THEME);
                if (hThemeCombo) {
                    int themeCount = Editor_GetThemeCount();
                    for (int i = 0; i < themeCount; i++) {
                        SendMessageW(hThemeCombo, CB_ADDSTRING, 0, (LPARAM)Editor_GetThemeName(i));
                    }
                    SendMessageW(hThemeCombo, CB_SETCURSEL, g_app->themeIndex, 0);
                }
            }
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    {
                        // Get font size
                        HWND hFontCombo = GetDlgItem(hwnd, IDC_DEFAULTS_FONTSIZE);
                        int fontSel = (int)SendMessageW(hFontCombo, CB_GETCURSEL, 0, 0);
                        if (fontSel != CB_ERR) {
                            int newSize = (int)SendMessageW(hFontCombo, CB_GETITEMDATA, fontSel, 0);
                            g_app->editorFont.lfHeight = -newSize;
                            if (g_app->hEditorFont) DeleteObject(g_app->hEditorFont);
                            g_app->hEditorFont = CreateFontIndirectW(&g_app->editorFont);
                        }

                        // Get theme
                        HWND hThemeCombo = GetDlgItem(hwnd, IDC_DEFAULTS_THEME);
                        int themeSel = (int)SendMessageW(hThemeCombo, CB_GETCURSEL, 0, 0);
                        BOOL themeChanged = (themeSel != CB_ERR && themeSel != g_app->themeIndex);
                        if (themeSel != CB_ERR) {
                            g_app->themeIndex = themeSel;
                        }

                        // Apply to all open editors
                        for (int i = 0; i < MAX_TABS; i++) {
                            if (g_app->tabs[i] && g_app->tabs[i]->hEditor) {
                                if (themeChanged) {
                                    // Apply theme (which also applies font)
                                    const WCHAR* filename = NULL;
                                    if (g_app->tabs[i]->document) {
                                        if (g_app->tabs[i]->document->type == DOC_TYPE_FILE) {
                                            filename = g_app->tabs[i]->document->filePath;
                                        } else {
                                            filename = g_app->tabs[i]->document->noteTitle;
                                        }
                                    }
                                    Editor_ApplyTheme(g_app->tabs[i]->hEditor, filename);
                                } else {
                                    // Just apply font
                                    Editor_SetFont(g_app->tabs[i]->hEditor, g_app->hEditorFont);
                                }
                            }
                        }

                        EndDialog(hwnd, IDOK);
                    }
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

// Find in Tabs dialog
void Dialogs_FindInTabs(HWND hParent) {
    DialogBoxW(g_app->hInstance, MAKEINTRESOURCEW(IDD_FIND_IN_TABS), hParent, FindInTabsProc);
}

// Helper to populate tabs list
static void PopulateTabsList(HWND hList) {
    // Set up list view columns
    LVCOLUMNW col = {
        .mask = LVCF_TEXT | LVCF_WIDTH,
        .cx = 200,
        .pszText = L"Tab"
    };
    ListView_InsertColumn(hList, 0, &col);

    // Add tabs
    for (int i = 0; i < MAX_TABS; i++) {
        if (g_app->tabs[i] && g_app->tabs[i]->document) {
            WCHAR title[MAX_TITLE_LEN + 8];
            swprintf_s(title, sizeof(title)/sizeof(WCHAR), L"%d. %s",
                      i + 1, Document_GetTitle(g_app->tabs[i]->document));

            LVITEMW item = {
                .mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE,
                .iItem = ListView_GetItemCount(hList),
                .pszText = title,
                .lParam = i,
                .state = LVIS_SELECTED,
                .stateMask = LVIS_SELECTED
            };
            ListView_InsertItem(hList, &item);
        }
    }

    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);

    // Check all items by default
    int count = ListView_GetItemCount(hList);
    for (int i = 0; i < count; i++) {
        ListView_SetCheckState(hList, i, TRUE);
    }
}

// Find in Tabs dialog procedure
INT_PTR CALLBACK FindInTabsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hList;
    static HWND hFindText;

    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG:
            hList = GetDlgItem(hwnd, IDC_TABS_LIST);
            hFindText = GetDlgItem(hwnd, IDC_FIND_IN_TABS_TEXT);

            PopulateTabsList(hList);

            // Set initial text from current find text
            if (g_app->findText[0]) {
                SetWindowTextW(hFindText, g_app->findText);
            }

            // Set checkboxes
            CheckDlgButton(hwnd, IDC_MATCH_CASE, g_app->matchCase ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_WHOLE_WORD, g_app->wholeWord ? BST_CHECKED : BST_UNCHECKED);

            SetFocus(hFindText);
            return FALSE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_SELECT_ALL_TABS:
                    {
                        int count = ListView_GetItemCount(hList);
                        BOOL allChecked = TRUE;
                        for (int i = 0; i < count; i++) {
                            if (!ListView_GetCheckState(hList, i)) {
                                allChecked = FALSE;
                                break;
                            }
                        }
                        for (int i = 0; i < count; i++) {
                            ListView_SetCheckState(hList, i, !allChecked);
                        }
                    }
                    return TRUE;

                case IDC_FIND_ALL_TABS:
                    {
                        WCHAR findText[256];
                        GetWindowTextW(hFindText, findText, 256);
                        if (!findText[0]) {
                            MessageBoxW(hwnd, L"Please enter text to find.", APP_NAME, MB_ICONWARNING);
                            return TRUE;
                        }

                        wcscpy_s(g_app->findText, MAX_PATH, findText);
                        g_app->matchCase = IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED;
                        g_app->wholeWord = IsDlgButtonChecked(hwnd, IDC_WHOLE_WORD) == BST_CHECKED;

                        int foundCount = 0;
                        int tabsSearched = 0;
                        WCHAR firstFoundTab[MAX_TITLE_LEN] = {0};

                        // Search in selected tabs
                        int count = ListView_GetItemCount(hList);
                        for (int i = 0; i < count; i++) {
                            if (ListView_GetCheckState(hList, i)) {
                                LVITEMW item = { .mask = LVIF_PARAM, .iItem = i };
                                ListView_GetItem(hList, &item);
                                int tabIndex = (int)item.lParam;

                                Tab* tab = g_app->tabs[tabIndex];
                                if (tab && tab->hEditor) {
                                    tabsSearched++;

                                    // Count occurrences in this tab
                                    WCHAR* text = Editor_GetText(tab->hEditor);
                                    if (text) {
                                        WCHAR* search = text;
                                        while ((search = wcsstr(search, findText)) != NULL) {
                                            if (foundCount == 0 && firstFoundTab[0] == 0) {
                                                wcscpy_s(firstFoundTab, MAX_TITLE_LEN,
                                                        Document_GetTitle(tab->document));
                                            }
                                            foundCount++;
                                            search++;
                                        }
                                        free(text);
                                    }
                                }
                            }
                        }

                        WCHAR resultMsg[256];
                        if (foundCount > 0) {
                            swprintf_s(resultMsg, 256, L"Found %d occurrence(s) in %d tab(s).\nFirst match in: %s",
                                      foundCount, tabsSearched, firstFoundTab);
                        } else {
                            swprintf_s(resultMsg, 256, L"No matches found in %d tab(s).", tabsSearched);
                        }
                        MessageBoxW(hwnd, resultMsg, APP_NAME, MB_ICONINFORMATION);
                    }
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

// Replace in Tabs dialog
void Dialogs_ReplaceInTabs(HWND hParent) {
    DialogBoxW(g_app->hInstance, MAKEINTRESOURCEW(IDD_REPLACE_IN_TABS), hParent, ReplaceInTabsProc);
}

// Replace in Tabs dialog procedure
INT_PTR CALLBACK ReplaceInTabsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hList;
    static HWND hFindText;
    static HWND hReplaceText;

    (void)lParam;

    switch (msg) {
        case WM_INITDIALOG:
            hList = GetDlgItem(hwnd, IDC_TABS_LIST);
            hFindText = GetDlgItem(hwnd, IDC_FIND_IN_TABS_TEXT);
            hReplaceText = GetDlgItem(hwnd, IDC_REPLACE_IN_TABS_TEXT);

            PopulateTabsList(hList);

            // Set initial text
            if (g_app->findText[0]) {
                SetWindowTextW(hFindText, g_app->findText);
            }
            if (g_app->replaceText[0]) {
                SetWindowTextW(hReplaceText, g_app->replaceText);
            }

            // Set checkboxes
            CheckDlgButton(hwnd, IDC_MATCH_CASE, g_app->matchCase ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_WHOLE_WORD, g_app->wholeWord ? BST_CHECKED : BST_UNCHECKED);

            SetFocus(hFindText);
            return FALSE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_SELECT_ALL_TABS:
                    {
                        int count = ListView_GetItemCount(hList);
                        BOOL allChecked = TRUE;
                        for (int i = 0; i < count; i++) {
                            if (!ListView_GetCheckState(hList, i)) {
                                allChecked = FALSE;
                                break;
                            }
                        }
                        for (int i = 0; i < count; i++) {
                            ListView_SetCheckState(hList, i, !allChecked);
                        }
                    }
                    return TRUE;

                case IDC_REPLACE_ALL_TABS:
                    {
                        WCHAR findText[256];
                        WCHAR replaceText[256];
                        GetWindowTextW(hFindText, findText, 256);
                        GetWindowTextW(hReplaceText, replaceText, 256);

                        if (!findText[0]) {
                            MessageBoxW(hwnd, L"Please enter text to find.", APP_NAME, MB_ICONWARNING);
                            return TRUE;
                        }

                        wcscpy_s(g_app->findText, MAX_PATH, findText);
                        wcscpy_s(g_app->replaceText, MAX_PATH, replaceText);
                        g_app->matchCase = IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED;
                        g_app->wholeWord = IsDlgButtonChecked(hwnd, IDC_WHOLE_WORD) == BST_CHECKED;

                        int replacements = 0;
                        int tabsModified = 0;

                        // Replace in selected tabs
                        int count = ListView_GetItemCount(hList);
                        for (int i = 0; i < count; i++) {
                            if (ListView_GetCheckState(hList, i)) {
                                LVITEMW item = { .mask = LVIF_PARAM, .iItem = i };
                                ListView_GetItem(hList, &item);
                                int tabIndex = (int)item.lParam;

                                Tab* tab = g_app->tabs[tabIndex];
                                if (tab && tab->hEditor) {
                                    int tabReplacements = Editor_ReplaceAll(tab->hEditor, findText,
                                                                           replaceText, g_app->matchCase,
                                                                           g_app->wholeWord);
                                    if (tabReplacements > 0) {
                                        replacements += tabReplacements;
                                        tabsModified++;
                                        if (tab->document) {
                                            tab->document->modified = TRUE;
                                            TabControl_UpdateTabTitle(tabIndex);
                                        }
                                    }
                                }
                            }
                        }

                        WCHAR resultMsg[256];
                        if (replacements > 0) {
                            swprintf_s(resultMsg, 256, L"Replaced %d occurrence(s) in %d tab(s).",
                                      replacements, tabsModified);
                        } else {
                            swprintf_s(resultMsg, 256, L"No matches found.");
                        }
                        MessageBoxW(hwnd, resultMsg, APP_NAME, MB_ICONINFORMATION);

                        MainWindow_UpdateTitle();
                    }
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

// Compare window class name
#define COMPARE_CLASS_NAME L"OpenNoteCompareWindow"

// Compare window data
typedef struct {
    HWND hEditor1;
    HWND hEditor2;
    HWND hLabel1;
    HWND hLabel2;
    WCHAR title1[MAX_TITLE_LEN];
    WCHAR title2[MAX_TITLE_LEN];
} CompareWindowData;

// Compare window procedure
static LRESULT CALLBACK CompareWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CompareWindowData* data = (CompareWindowData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE:
            {
                CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
                data = (CompareWindowData*)cs->lpCreateParams;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)data);

                // Create labels
                data->hLabel1 = CreateWindowExW(0, L"STATIC", data->title1,
                    WS_CHILD | WS_VISIBLE | SS_CENTER,
                    0, 0, 100, 20, hwnd, NULL, g_app->hInstance, NULL);
                data->hLabel2 = CreateWindowExW(0, L"STATIC", data->title2,
                    WS_CHILD | WS_VISIBLE | SS_CENTER,
                    100, 0, 100, 20, hwnd, NULL, g_app->hInstance, NULL);

                // Create read-only editors
                data->hEditor1 = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, NULL,
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                    0, 20, 100, 100, hwnd, NULL, g_app->hInstance, NULL);
                data->hEditor2 = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, NULL,
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                    100, 20, 100, 100, hwnd, NULL, g_app->hInstance, NULL);

                // Set font
                SendMessageW(data->hLabel1, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
                SendMessageW(data->hLabel2, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
                if (g_app->hEditorFont) {
                    Editor_SetFont(data->hEditor1, g_app->hEditorFont);
                    Editor_SetFont(data->hEditor2, g_app->hEditorFont);
                }
            }
            return 0;

        case WM_SIZE:
            {
                int cx = LOWORD(lParam);
                int cy = HIWORD(lParam);
                int half = cx / 2;
                int labelHeight = 24;
                int gap = 4;

                SetWindowPos(data->hLabel1, NULL, 0, 0, half - gap, labelHeight, SWP_NOZORDER);
                SetWindowPos(data->hLabel2, NULL, half + gap, 0, half - gap, labelHeight, SWP_NOZORDER);
                SetWindowPos(data->hEditor1, NULL, 0, labelHeight, half - gap, cy - labelHeight, SWP_NOZORDER);
                SetWindowPos(data->hEditor2, NULL, half + gap, labelHeight, half - gap, cy - labelHeight, SWP_NOZORDER);
            }
            return 0;

        case WM_DESTROY:
            free(data);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Register compare window class
static BOOL RegisterCompareClass(void) {
    static BOOL registered = FALSE;
    if (registered) return TRUE;

    WNDCLASSEXW wc = {
        .cbSize = sizeof(wc),
        .lpfnWndProc = CompareWndProc,
        .hInstance = g_app->hInstance,
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
        .lpszClassName = COMPARE_CLASS_NAME
    };

    registered = (RegisterClassExW(&wc) != 0);
    return registered;
}

// Show compare window
void Dialogs_Compare(HWND hParent, int tab1Index, int tab2Index) {
    if (!RegisterCompareClass()) return;

    Tab* tab1 = g_app->tabs[tab1Index];
    Tab* tab2 = g_app->tabs[tab2Index];
    if (!tab1 || !tab2 || !tab1->document || !tab2->document) return;

    // Allocate window data
    CompareWindowData* data = (CompareWindowData*)calloc(1, sizeof(CompareWindowData));
    if (!data) return;

    wcscpy_s(data->title1, MAX_TITLE_LEN, Document_GetTitle(tab1->document));
    wcscpy_s(data->title2, MAX_TITLE_LEN, Document_GetTitle(tab2->document));

    // Create window title
    WCHAR windowTitle[MAX_TITLE_LEN * 2 + 32];
    swprintf_s(windowTitle, sizeof(windowTitle)/sizeof(WCHAR),
               L"Compare: %s vs %s", data->title1, data->title2);

    // Get parent window position for centering
    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int width = 1000;
    int height = 600;
    int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

    HWND hCompare = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        COMPARE_CLASS_NAME,
        windowTitle,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, width, height,
        NULL,  // No parent - independent window
        NULL,
        g_app->hInstance,
        data
    );

    if (hCompare && data->hEditor1 && data->hEditor2) {
        // Copy content from tabs to compare editors
        WCHAR* text1 = Editor_GetText(tab1->hEditor);
        WCHAR* text2 = Editor_GetText(tab2->hEditor);

        if (text1) {
            Editor_SetText(data->hEditor1, text1);
            free(text1);
        }
        if (text2) {
            Editor_SetText(data->hEditor2, text2);
            free(text2);
        }
    }
}

// Input box dialog data
typedef struct {
    const WCHAR* title;
    const WCHAR* prompt;
    WCHAR* buffer;
    int bufferSize;
} InputBoxData;

// Input box dialog procedure
static INT_PTR CALLBACK InputBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static InputBoxData* data;

    switch (msg) {
        case WM_INITDIALOG:
            data = (InputBoxData*)lParam;
            SetWindowTextW(hwnd, data->title);
            SetDlgItemTextW(hwnd, IDC_STATIC, data->prompt);
            SetDlgItemTextW(hwnd, IDC_GOTO_LINE, data->buffer);  // Reuse IDC_GOTO_LINE for input
            SendDlgItemMessageW(hwnd, IDC_GOTO_LINE, EM_SETSEL, 0, -1);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    GetDlgItemTextW(hwnd, IDC_GOTO_LINE, data->buffer, data->bufferSize);
                    EndDialog(hwnd, IDOK);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

// Simple input box dialog
BOOL Dialogs_InputBox(HWND hParent, const WCHAR* title, const WCHAR* prompt, WCHAR* buffer, int bufferSize) {
    InputBoxData data = {
        .title = title,
        .prompt = prompt,
        .buffer = buffer,
        .bufferSize = bufferSize
    };

    // Use the Go To dialog as a template since it has the right layout
    return DialogBoxParamW(g_app->hInstance, MAKEINTRESOURCEW(IDD_GOTO), hParent, InputBoxProc, (LPARAM)&data) == IDOK;
}
