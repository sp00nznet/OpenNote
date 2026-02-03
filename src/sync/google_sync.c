#include <winsock2.h>
#include <ws2tcpip.h>
#include "supernote.h"
#include "sync/google_sync.h"
#include "sync/oauth.h"
#include <winhttp.h>
#include <stdio.h>

#pragma comment(lib, "winhttp.lib")

// Google Drive API host
#define GOOGLE_API_HOST L"www.googleapis.com"

// OpenNote folder name in Google Drive
#define OPENNOTE_FOLDER_NAME "OpenNote"

// Get stored OAuth token
static BOOL GetGoogleToken(char* tokenOut, int tokenSize) {
    OAuthToken token;
    if (OAuth_LoadToken("google", &token) && token.isAuthenticated) {
        strncpy_s(tokenOut, tokenSize, token.accessToken, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

// Make Google Drive API request
static BOOL GoogleAPI_Request(const WCHAR* method, const WCHAR* path, const char* body,
                               const char* contentType, char* responseOut, int responseSize, int* httpStatusOut) {
    char token[1024];
    if (!GetGoogleToken(token, sizeof(token))) {
        return FALSE;
    }

    HINTERNET hSession = WinHttpOpen(L"OpenNote/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return FALSE;

    HINTERNET hConnect = WinHttpConnect(hSession, GOOGLE_API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method, path,
                                             NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Build auth header
    WCHAR authHeader[1280];
    WCHAR tokenW[1024];
    MultiByteToWideChar(CP_UTF8, 0, token, -1, tokenW, 512);
    swprintf_s(authHeader, 768, L"Authorization: Bearer %s", tokenW);
    WinHttpAddRequestHeaders(hRequest, authHeader, -1, WINHTTP_ADDREQ_FLAG_ADD);

    // Add content type if specified
    if (contentType) {
        WCHAR contentTypeW[256];
        MultiByteToWideChar(CP_UTF8, 0, contentType, -1, contentTypeW, 256);
        WCHAR contentTypeHeader[300];
        swprintf_s(contentTypeHeader, 300, L"Content-Type: %s", contentTypeW);
        WinHttpAddRequestHeaders(hRequest, contentTypeHeader, -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    BOOL result = FALSE;
    DWORD bodyLen = body ? (DWORD)strlen(body) : 0;

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           (LPVOID)body, bodyLen, bodyLen, 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            // Get status code
            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               NULL, &statusCode, &statusSize, NULL);
            if (httpStatusOut) *httpStatusOut = (int)statusCode;

            // Read response
            if (responseOut && responseSize > 0) {
                DWORD bytesRead = 0;
                DWORD totalRead = 0;
                responseOut[0] = '\0';

                while (WinHttpReadData(hRequest, responseOut + totalRead,
                                       responseSize - totalRead - 1, &bytesRead) && bytesRead > 0) {
                    totalRead += bytesRead;
                    if (totalRead >= (DWORD)(responseSize - 1)) break;
                }
                responseOut[totalRead] = '\0';
            }

            result = (statusCode >= 200 && statusCode < 300);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// Escape string for JSON
static void JsonEscape(const char* input, char* output, int outputSize) {
    int j = 0;
    for (int i = 0; input[i] && j < outputSize - 2; i++) {
        char c = input[i];
        if (c == '\"' || c == '\\') {
            output[j++] = '\\';
            output[j++] = c;
        } else if (c == '\n') {
            output[j++] = '\\';
            output[j++] = 'n';
        } else if (c == '\r') {
            output[j++] = '\\';
            output[j++] = 'r';
        } else if (c == '\t') {
            output[j++] = '\\';
            output[j++] = 't';
        } else if ((unsigned char)c >= 32) {
            output[j++] = c;
        }
    }
    output[j] = '\0';
}

// Parse JSON string value
static BOOL JsonGetString(const char* json, const char* key, char* valueOut, int valueSize) {
    char searchKey[128];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

    char* found = strstr(json, searchKey);
    if (!found) return FALSE;

    // Find the colon after the key
    found = strchr(found + strlen(searchKey), ':');
    if (!found) return FALSE;

    // Skip whitespace
    while (*found && (*found == ':' || *found == ' ' || *found == '\t')) found++;

    if (*found != '\"') return FALSE;
    found++; // Skip opening quote

    // Copy until closing quote, handling escapes
    int j = 0;
    while (*found && *found != '\"' && j < valueSize - 1) {
        if (*found == '\\' && *(found + 1)) {
            found++;
            switch (*found) {
                case 'n': valueOut[j++] = '\n'; break;
                case 'r': valueOut[j++] = '\r'; break;
                case 't': valueOut[j++] = '\t'; break;
                case '\"': valueOut[j++] = '\"'; break;
                case '\\': valueOut[j++] = '\\'; break;
                default: valueOut[j++] = *found; break;
            }
        } else {
            valueOut[j++] = *found;
        }
        found++;
    }
    valueOut[j] = '\0';
    return TRUE;
}

// Get drive file ID for a note from database
static BOOL GetNoteDriveId(int noteId, char* driveIdOut, int driveIdSize) {
    if (!Database_IsOpen()) return FALSE;

    sqlite3_stmt* stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT drive_file_id FROM notes WHERE id = %d AND drive_file_id IS NOT NULL", noteId);

    BOOL found = FALSE;
    if (sqlite3_prepare_v2(Database_GetHandle(), sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* driveId = (const char*)sqlite3_column_text(stmt, 0);
            if (driveId && driveId[0]) {
                strncpy_s(driveIdOut, driveIdSize, driveId, _TRUNCATE);
                found = TRUE;
            }
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

// Save drive file ID for a note
static void SaveNoteDriveId(int noteId, const char* driveId) {
    if (!Database_IsOpen()) return;

    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE notes SET drive_file_id = '%s', last_synced = datetime('now') WHERE id = %d",
             driveId, noteId);
    Database_Execute(sql);
}

// Get or create OpenNote folder in Google Drive
static BOOL GetOrCreateFolder(char* folderIdOut, int folderIdSize) {
    char response[8192];
    int httpStatus = 0;

    // Search for existing OpenNote folder
    if (GoogleAPI_Request(L"GET",
            L"/drive/v3/files?q=name%3D'OpenNote'%20and%20mimeType%3D'application/vnd.google-apps.folder'%20and%20trashed%3Dfalse&fields=files(id,name)",
            NULL, NULL, response, sizeof(response), &httpStatus)) {
        // Check if folder exists in response
        char folderId[128];
        if (JsonGetString(response, "id", folderId, sizeof(folderId))) {
            strncpy_s(folderIdOut, folderIdSize, folderId, _TRUNCATE);
            return TRUE;
        }
    }

    // Create folder if it doesn't exist
    const char* createBody = "{\"name\":\"OpenNote\",\"mimeType\":\"application/vnd.google-apps.folder\"}";

    if (GoogleAPI_Request(L"POST", L"/drive/v3/files?fields=id",
            createBody, "application/json", response, sizeof(response), &httpStatus)) {
        char folderId[128];
        if (JsonGetString(response, "id", folderId, sizeof(folderId))) {
            strncpy_s(folderIdOut, folderIdSize, folderId, _TRUNCATE);
            return TRUE;
        }
    }

    return FALSE;
}

// Upload a note to Google Drive
BOOL GoogleSync_UploadNote(int noteId, const WCHAR* title, const WCHAR* content) {
    // Get or create OpenNote folder
    char folderId[128];
    if (!GetOrCreateFolder(folderId, sizeof(folderId))) {
        return FALSE;
    }

    // Convert to UTF-8
    char titleUtf8[MAX_TITLE_LEN * 3];
    char contentUtf8[1024 * 1024]; // 1MB max
    WideCharToMultiByte(CP_UTF8, 0, title, -1, titleUtf8, sizeof(titleUtf8), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, content, -1, contentUtf8, sizeof(contentUtf8), NULL, NULL);

    // Escape title for JSON
    char titleEscaped[MAX_TITLE_LEN * 6];
    JsonEscape(titleUtf8, titleEscaped, sizeof(titleEscaped));

    // Build filename
    char filename[MAX_TITLE_LEN * 3 + 4];
    snprintf(filename, sizeof(filename), "%s.md", titleUtf8);
    char filenameEscaped[MAX_TITLE_LEN * 6 + 4];
    JsonEscape(filename, filenameEscaped, sizeof(filenameEscaped));

    // Check if we already have a file for this note
    char existingFileId[128] = {0};
    BOOL isUpdate = GetNoteDriveId(noteId, existingFileId, sizeof(existingFileId));

    // Build multipart body
    const char* boundary = "opennote_boundary_12345";
    size_t bodySize = strlen(contentUtf8) + 2048;
    char* body = (char*)malloc(bodySize);
    if (!body) return FALSE;

    if (isUpdate) {
        // For updates, just send the metadata
        snprintf(body, bodySize,
            "--%s\r\n"
            "Content-Type: application/json; charset=UTF-8\r\n\r\n"
            "{\"name\":\"%s\"}\r\n"
            "--%s\r\n"
            "Content-Type: text/markdown\r\n\r\n"
            "%s\r\n"
            "--%s--",
            boundary, filenameEscaped, boundary, contentUtf8, boundary);
    } else {
        // For new files, include parent folder
        snprintf(body, bodySize,
            "--%s\r\n"
            "Content-Type: application/json; charset=UTF-8\r\n\r\n"
            "{\"name\":\"%s\",\"parents\":[\"%s\"]}\r\n"
            "--%s\r\n"
            "Content-Type: text/markdown\r\n\r\n"
            "%s\r\n"
            "--%s--",
            boundary, filenameEscaped, folderId, boundary, contentUtf8, boundary);
    }

    char contentType[128];
    snprintf(contentType, sizeof(contentType), "multipart/related; boundary=%s", boundary);

    char response[4096];
    int httpStatus = 0;
    BOOL success = FALSE;

    if (isUpdate) {
        // Update existing file
        WCHAR path[256];
        WCHAR fileIdW[128];
        MultiByteToWideChar(CP_UTF8, 0, existingFileId, -1, fileIdW, 128);
        swprintf_s(path, 256, L"/upload/drive/v3/files/%s?uploadType=multipart&fields=id", fileIdW);
        success = GoogleAPI_Request(L"PATCH", path, body, contentType, response, sizeof(response), &httpStatus);
    } else {
        // Create new file
        success = GoogleAPI_Request(L"POST", L"/upload/drive/v3/files?uploadType=multipart&fields=id",
                                     body, contentType, response, sizeof(response), &httpStatus);

        if (success) {
            // Extract file ID from response and save it
            char fileId[128];
            if (JsonGetString(response, "id", fileId, sizeof(fileId))) {
                SaveNoteDriveId(noteId, fileId);
            }
        }
    }

    free(body);
    return success;
}

// Delete a file from Google Drive
BOOL GoogleSync_DeleteFile(int noteId) {
    char fileId[128];
    if (!GetNoteDriveId(noteId, fileId, sizeof(fileId))) {
        return TRUE; // No file to delete
    }

    WCHAR path[256];
    WCHAR fileIdW[128];
    MultiByteToWideChar(CP_UTF8, 0, fileId, -1, fileIdW, 128);
    swprintf_s(path, 256, L"/drive/v3/files/%s", fileIdW);

    int httpStatus = 0;
    return GoogleAPI_Request(L"DELETE", path, NULL, NULL, NULL, 0, &httpStatus);
}

// Download all files from OpenNote folder
int GoogleSync_DownloadAll(HWND hParent) {
    // Get OpenNote folder ID
    char folderId[128];
    if (!GetOrCreateFolder(folderId, sizeof(folderId))) {
        MessageBoxW(hParent, L"Failed to access OpenNote folder in Google Drive.", APP_NAME, MB_ICONERROR);
        return 0;
    }

    // List files in folder
    WCHAR listPath[512];
    WCHAR folderIdW[128];
    MultiByteToWideChar(CP_UTF8, 0, folderId, -1, folderIdW, 128);
    swprintf_s(listPath, 512,
        L"/drive/v3/files?q='%s'%%20in%%20parents%%20and%%20trashed%%3Dfalse&fields=files(id,name)",
        folderIdW);

    char* response = (char*)malloc(256 * 1024);
    if (!response) return 0;

    int httpStatus = 0;
    if (!GoogleAPI_Request(L"GET", listPath, NULL, NULL, response, 256 * 1024, &httpStatus)) {
        free(response);
        MessageBoxW(hParent, L"Failed to list files from Google Drive.", APP_NAME, MB_ICONERROR);
        return 0;
    }

    int imported = 0;

    // Parse file list - look for each file's id and name
    char* filesSection = strstr(response, "\"files\"");
    if (!filesSection) {
        free(response);
        return 0;
    }

    char* ptr = filesSection;
    while ((ptr = strstr(ptr, "\"id\":\"")) != NULL) {
        ptr += 6;
        char fileId[128];
        int i = 0;
        while (*ptr && *ptr != '\"' && i < 127) {
            fileId[i++] = *ptr++;
        }
        fileId[i] = '\0';

        // Get filename
        char* namePtr = strstr(ptr, "\"name\":\"");
        if (!namePtr) continue;
        namePtr += 8;
        char filename[MAX_TITLE_LEN * 3];
        i = 0;
        while (*namePtr && *namePtr != '\"' && i < (int)sizeof(filename) - 1) {
            filename[i++] = *namePtr++;
        }
        filename[i] = '\0';

        // Check if this file is already imported
        char sql[256];
        snprintf(sql, sizeof(sql), "SELECT id FROM notes WHERE drive_file_id = '%s'", fileId);
        sqlite3_stmt* stmt;
        BOOL alreadyExists = FALSE;
        if (sqlite3_prepare_v2(Database_GetHandle(), sql, -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                alreadyExists = TRUE;
            }
            sqlite3_finalize(stmt);
        }

        if (alreadyExists) continue;

        // Download file content
        WCHAR downloadPath[256];
        WCHAR fileIdW[128];
        MultiByteToWideChar(CP_UTF8, 0, fileId, -1, fileIdW, 128);
        swprintf_s(downloadPath, 256, L"/drive/v3/files/%s?alt=media", fileIdW);

        char* fileContent = (char*)malloc(512 * 1024);
        if (!fileContent) continue;

        if (GoogleAPI_Request(L"GET", downloadPath, NULL, NULL, fileContent, 512 * 1024, &httpStatus)) {
            // Extract title from filename (remove .md extension)
            char title[MAX_TITLE_LEN * 3];
            strncpy_s(title, sizeof(title), filename, _TRUNCATE);
            size_t len = strlen(title);
            if (len > 3 && _stricmp(title + len - 3, ".md") == 0) {
                title[len - 3] = '\0';
            }

            // Convert to wide strings
            WCHAR titleW[MAX_TITLE_LEN];
            int contentLen = (int)strlen(fileContent);
            WCHAR* contentW = (WCHAR*)malloc((contentLen + 1) * sizeof(WCHAR));

            if (contentW) {
                MultiByteToWideChar(CP_UTF8, 0, title, -1, titleW, MAX_TITLE_LEN);
                MultiByteToWideChar(CP_UTF8, 0, fileContent, -1, contentW, contentLen + 1);

                // Create note
                int noteId = Notes_Create(titleW, contentW);
                if (noteId > 0) {
                    SaveNoteDriveId(noteId, fileId);
                    imported++;
                }

                free(contentW);
            }
        }
        free(fileContent);
    }

    free(response);
    return imported;
}

// Sync all notes
BOOL GoogleSync_SyncAll(HWND hParent) {
    // First, upload all local notes
    NoteListItem* items = NULL;
    int count = 0;
    Notes_GetList(&items, &count);

    int uploaded = 0;
    int failed = 0;

    for (int i = 0; i < count; i++) {
        WCHAR* content = Notes_GetContent(items[i].id);
        if (content) {
            if (GoogleSync_UploadNote(items[i].id, items[i].title, content)) {
                uploaded++;
            } else {
                failed++;
            }
            free(content);
        }
    }
    Notes_FreeList(items);

    // Then download any new files
    int downloaded = GoogleSync_DownloadAll(hParent);

    // Show summary
    WCHAR msg[256];
    swprintf_s(msg, 256, L"Sync complete!\n\nUploaded: %d notes\nDownloaded: %d notes\nFailed: %d",
               uploaded, downloaded, failed);
    MessageBoxW(hParent, msg, APP_NAME, MB_ICONINFORMATION);

    return (failed == 0);
}
