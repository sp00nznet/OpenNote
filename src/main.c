#include "supernote.h"
#include "res/resource.h"
#include "sync/crypto.h"
#include "sync/oauth.h"

// Global application state
AppState* g_app = NULL;

// --selftest: run the checks that guard the crypto and parsing paths, print the
// result and exit. This is a /SUBSYSTEM:WINDOWS binary with no console of its
// own, so borrow the caller's when there is one; CI reads the exit code either
// way. 0 means every check held.
static int RunSelfTest(void) {
    // Only borrow a console when there is genuinely nowhere to write. If the
    // caller already redirected stdout -- a pipe, a file, a CI log -- then
    // reopening it on CONOUT$ would throw that output away.
    BOOL attached = FALSE;
    FILE* out = NULL;
    if (GetStdHandle(STD_OUTPUT_HANDLE) == NULL) {
        attached = AttachConsole(ATTACH_PARENT_PROCESS);
        if (attached) freopen_s(&out, "CONOUT$", "w", stdout);
    }

    struct {
        const char* name;
        BOOL (*run)(char*, size_t);
    } checks[] = {
        { "crypto", Crypto_SelfTest },
        { "oauth",  OAuth_SelfTest  },
    };

    int failed = 0;
    for (size_t i = 0; i < ARRAYSIZE(checks); i++) {
        char failure[256] = {0};
        if (checks[i].run(failure, sizeof(failure))) {
            printf("ok    %s\n", checks[i].name);
        } else {
            printf("FAIL  %s: %s\n", checks[i].name, failure);
            failed++;
        }
    }

    printf("%s\n", failed ? "SELFTEST FAILED" : "selftest passed");
    fflush(stdout);
    if (attached) FreeConsole();

    return failed ? 1 : 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;

    if (lpCmdLine && wcsstr(lpCmdLine, L"--selftest")) {
        return RunSelfTest();
    }

    // Initialize common controls
    INITCOMMONCONTROLSEX icex = {
        .dwSize = sizeof(icex),
        .dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES
    };
    InitCommonControlsEx(&icex);

    // ponytail: no Msftedit.dll load here. The RichEdit control has been unused since
    // the Scintilla port, but the library was still being loaded on startup and a load
    // failure aborted the application for nothing. It comes back deliberately in v0.5
    // when the rich text document type needs it -- see ROADMAP.md.

    // Initialize application
    if (!App_Initialize(hInstance)) {
        MessageBoxW(NULL, L"Failed to initialize application", APP_NAME, MB_ICONERROR);
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

    return result ? 0 : 1;
}
