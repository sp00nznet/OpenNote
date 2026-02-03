#include "supernote.h"
#include "res/resource.h"

// Global application state
AppState* g_app = NULL;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;

    // Initialize common controls
    INITCOMMONCONTROLSEX icex = {
        .dwSize = sizeof(icex),
        .dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES
    };
    InitCommonControlsEx(&icex);

    // Load RichEdit library
    HMODULE hRichEdit = LoadLibraryW(L"Msftedit.dll");
    if (!hRichEdit) {
        MessageBoxW(NULL, L"Failed to load RichEdit library", APP_NAME, MB_ICONERROR);
        return 1;
    }

    // Initialize application
    if (!App_Initialize(hInstance)) {
        MessageBoxW(NULL, L"Failed to initialize application", APP_NAME, MB_ICONERROR);
        FreeLibrary(hRichEdit);
        return 1;
    }

    // Show the main window
    ShowWindow(g_app->hMainWindow, nCmdShow);
    UpdateWindow(g_app->hMainWindow);

    // Process command line - open file if specified
    if (lpCmdLine && lpCmdLine[0]) {
        // Remove quotes if present
        WCHAR path[MAX_PATH];
        if (lpCmdLine[0] == L'"') {
            wcscpy_s(path, MAX_PATH, lpCmdLine + 1);
            WCHAR* endQuote = wcschr(path, L'"');
            if (endQuote) *endQuote = L'\0';
        } else {
            wcscpy_s(path, MAX_PATH, lpCmdLine);
        }

        // Check if file exists
        if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
            // Open the file in a new tab
            Document* doc = Document_CreateFromFile(path);
            if (doc) {
                Tab* tab = App_GetActiveTab();
                if (tab) {
                    Document_Destroy(tab->document);
                    tab->document = doc;
                    Document_Load(doc, tab->hEditor);
                    TabControl_UpdateTabTitle(tab->index);
                    MainWindow_UpdateTitle();
                }
            }
        }
    }

    // Run message loop
    BOOL result = App_Run();

    // Cleanup
    App_Shutdown();
    FreeLibrary(hRichEdit);

    return result ? 0 : 1;
}
