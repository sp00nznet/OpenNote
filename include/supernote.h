#ifndef SUPERNOTE_H
#define SUPERNOTE_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <richedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "sqlite3.h"

// Application info
#define APP_NAME        L"OpenNote"
#define APP_VERSION     L"1.0.0"
#define APP_CLASS_NAME  L"OpenNoteMainWindow"

// Limits
#define MAX_TABS        64
#define MAX_PATH_LEN    32768
#define MAX_TITLE_LEN   256
#define MAX_RECENT      10

// Document types
typedef enum {
    DOC_TYPE_FILE,      // Regular file from filesystem
    DOC_TYPE_NOTE       // Database note
} DocumentType;

// Document encoding
typedef enum {
    ENCODING_UTF8,
    ENCODING_UTF16_LE,
    ENCODING_UTF16_BE,
    ENCODING_ANSI
} TextEncoding;

// Forward declarations
typedef struct Document Document;
typedef struct Tab Tab;
typedef struct AppState AppState;

// Include component headers
#include "app.h"
#include "ui/mainwindow.h"
#include "ui/tabcontrol.h"
#include "ui/editor.h"
#include "ui/menubar.h"
#include "ui/statusbar.h"
#include "ui/dialogs.h"
#include "core/document.h"
#include "core/fileio.h"
#include "core/search.h"
#include "db/database.h"
#include "db/notes_repo.h"
#include "db/links_repo.h"

// Global application state
extern AppState* g_app;

#endif // SUPERNOTE_H
