#include "supernote.h"
#include "res/resource.h"

// Create status bar
HWND StatusBar_Create(HWND hParent) {
    HWND hStatus = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hParent,
        NULL,
        g_app->hInstance,
        NULL
    );

    if (hStatus) {
        // Parts will be set up in StatusBar_Resize
        (void)hParent;  // Used later
    }

    return hStatus;
}

// Resize status bar
void StatusBar_Resize(HWND hParent) {
    if (!g_app->hStatusBar) return;

    RECT rc;
    GetClientRect(hParent, &rc);

    // Resize status bar
    SendMessageW(g_app->hStatusBar, WM_SIZE, 0, 0);

    // Calculate part widths
    int width = rc.right;
    int parts[5];
    parts[4] = width - 10;       // Modified
    parts[3] = parts[4] - 70;    // Encoding
    parts[2] = parts[3] - 60;    // Column
    parts[1] = parts[2] - 80;    // Line
    parts[0] = parts[1] - 100;   // Message (remaining)

    SendMessageW(g_app->hStatusBar, SB_SETPARTS, 5, (LPARAM)parts);
}

// Set text in a part
void StatusBar_SetText(int part, const WCHAR* text) {
    if (!g_app->hStatusBar) return;
    SendMessageW(g_app->hStatusBar, SB_SETTEXTW, part, (LPARAM)text);
}

// Update position display
void StatusBar_UpdatePosition(int line, int column) {
    if (!g_app->hStatusBar) return;

    WCHAR lineText[32], colText[32];
    swprintf_s(lineText, sizeof(lineText)/sizeof(WCHAR), L"Ln %d", line);
    swprintf_s(colText, sizeof(colText)/sizeof(WCHAR), L"Col %d", column);

    StatusBar_SetText(SB_PART_LINE, lineText);
    StatusBar_SetText(SB_PART_COLUMN, colText);
}

// Update encoding display
void StatusBar_UpdateEncoding(TextEncoding encoding) {
    if (!g_app->hStatusBar) return;

    const WCHAR* text;
    switch (encoding) {
        case ENCODING_UTF16_LE: text = L"UTF-16 LE"; break;
        case ENCODING_UTF16_BE: text = L"UTF-16 BE"; break;
        case ENCODING_ANSI:     text = L"ANSI"; break;
        case ENCODING_UTF8:
        default:                text = L"UTF-8"; break;
    }

    StatusBar_SetText(SB_PART_ENCODING, text);
}

// Update modified indicator
void StatusBar_UpdateModified(BOOL modified) {
    if (!g_app->hStatusBar) return;
    StatusBar_SetText(SB_PART_MODIFIED, modified ? L"Modified" : L"");
}

// Set message
void StatusBar_SetMessage(const WCHAR* message) {
    if (!g_app->hStatusBar) return;
    StatusBar_SetText(SB_PART_MESSAGE, message ? message : L"");
}

// Clear status bar
void StatusBar_Clear(void) {
    StatusBar_SetText(SB_PART_MESSAGE, L"");
    StatusBar_SetText(SB_PART_LINE, L"");
    StatusBar_SetText(SB_PART_COLUMN, L"");
    StatusBar_SetText(SB_PART_ENCODING, L"");
    StatusBar_SetText(SB_PART_MODIFIED, L"");
}

// Show/hide status bar
void StatusBar_Show(BOOL show) {
    if (!g_app->hStatusBar) return;
    ShowWindow(g_app->hStatusBar, show ? SW_SHOW : SW_HIDE);
}
