#include <winsock2.h>
#include <ws2tcpip.h>
#include "supernote.h"
#include "sync/github_sync.h"
#include "sync/oauth.h"
#include <winhttp.h>
#include <stdio.h>

#pragma comment(lib, "winhttp.lib")

// GitHub API host
#define GITHUB_API_HOST L"api.github.com"

// Get stored OAuth token
static BOOL GetGitHubToken(char* tokenOut, int tokenSize) {
    OAuthToken token;
    if (OAuth_LoadToken("github", &token) && token.isAuthenticated) {
        strncpy_s(tokenOut, tokenSize, token.accessToken, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

// Make GitHub API request
static BOOL GitHubAPI_Request(const WCHAR* method, const WCHAR* path, const char* body,
                               char* responseOut, int responseSize, int* httpStatusOut) {
    char token[1024];
    if (!GetGitHubToken(token, sizeof(token))) {
        return FALSE;
    }

    HINTERNET hSession = WinHttpOpen(L"OpenNote/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return FALSE;

    HINTERNET hConnect = WinHttpConnect(hSession, GITHUB_API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
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
    MultiByteToWideChar(CP_UTF8, 0, token, -1, tokenW, 256);
    swprintf_s(authHeader, 512, L"Authorization: Bearer %s", tokenW);
    WinHttpAddRequestHeaders(hRequest, authHeader, -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, L"Accept: application/vnd.github+json", -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, L"X-GitHub-Api-Version: 2022-11-28", -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, L"User-Agent: OpenNote", -1, WINHTTP_ADDREQ_FLAG_ADD);

    if (body) {
        WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1, WINHTTP_ADDREQ_FLAG_ADD);
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
        if (c == '"' || c == '\\') {
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

    if (*found != '"') return FALSE;
    found++; // Skip opening quote

    // Copy until closing quote, handling escapes
    int j = 0;
    while (*found && *found != '"' && j < valueSize - 1) {
        if (*found == '\\' && *(found + 1)) {
            found++;
            switch (*found) {
                case 'n': valueOut[j++] = '\n'; break;
                case 'r': valueOut[j++] = '\r'; break;
                case 't': valueOut[j++] = '\t'; break;
                case '"': valueOut[j++] = '"'; break;
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

// Get gist ID for a note from database
static BOOL GetNoteGistId(int noteId, char* gistIdOut, int gistIdSize) {
    if (!Database_IsOpen()) return FALSE;

    sqlite3_stmt* stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT gist_id FROM notes WHERE id = %d AND gist_id IS NOT NULL", noteId);

    BOOL found = FALSE;
    if (sqlite3_prepare_v2(Database_GetHandle(), sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* gistId = (const char*)sqlite3_column_text(stmt, 0);
            if (gistId && gistId[0]) {
                strncpy_s(gistIdOut, gistIdSize, gistId, _TRUNCATE);
                found = TRUE;
            }
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

// Save gist ID for a note
static void SaveNoteGistId(int noteId, const char* gistId) {
    if (!Database_IsOpen()) return;

    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE notes SET gist_id = '%s', last_synced = datetime('now') WHERE id = %d",
             gistId, noteId);
    Database_Execute(sql);
}

// Upload a note as a GitHub Gist
BOOL GitHubSync_UploadNote(int noteId, const WCHAR* title, const WCHAR* content) {
    // Convert title to UTF-8
    char titleUtf8[MAX_TITLE_LEN * 3];
    WideCharToMultiByte(CP_UTF8, 0, title, -1, titleUtf8, sizeof(titleUtf8), NULL, NULL);

    // Convert content to UTF-8 (heap allocated)
    int contentUtf8Len = WideCharToMultiByte(CP_UTF8, 0, content, -1, NULL, 0, NULL, NULL);
    if (contentUtf8Len <= 0) return FALSE;

    char* contentUtf8 = (char*)malloc(contentUtf8Len);
    if (!contentUtf8) return FALSE;
    WideCharToMultiByte(CP_UTF8, 0, content, -1, contentUtf8, contentUtf8Len, NULL, NULL);

    // Escape for JSON
    char titleEscaped[MAX_TITLE_LEN * 6];
    char* contentEscaped = (char*)malloc(strlen(contentUtf8) * 2 + 1);
    if (!contentEscaped) {
        free(contentUtf8);
        return FALSE;
    }

    JsonEscape(titleUtf8, titleEscaped, sizeof(titleEscaped));
    JsonEscape(contentUtf8, contentEscaped, (int)(strlen(contentUtf8) * 2 + 1));
    free(contentUtf8);

    // Build filename (use title with .md extension)
    char filename[MAX_TITLE_LEN * 3 + 4];
    snprintf(filename, sizeof(filename), "%s.md", titleUtf8);
    char filenameEscaped[MAX_TITLE_LEN * 6 + 4];
    JsonEscape(filename, filenameEscaped, sizeof(filenameEscaped));

    // Build JSON body
    size_t bodySize = strlen(contentEscaped) + 1024;
    char* body = (char*)malloc(bodySize);
    if (!body) {
        free(contentEscaped);
        return FALSE;
    }

    // Check if we already have a gist for this note
    char existingGistId[64] = {0};
    BOOL isUpdate = GetNoteGistId(noteId, existingGistId, sizeof(existingGistId));

    snprintf(body, bodySize,
             "{\"description\":\"OpenNote: %s\",\"public\":false,\"files\":{\"%s\":{\"content\":\"%s\"}}}",
             titleEscaped, filenameEscaped, contentEscaped);

    free(contentEscaped);

    char response[4096];
    int httpStatus = 0;
    BOOL success = FALSE;

    if (isUpdate) {
        // Update existing gist
        WCHAR path[256];
        WCHAR gistIdW[64];
        MultiByteToWideChar(CP_UTF8, 0, existingGistId, -1, gistIdW, 64);
        swprintf_s(path, 256, L"/gists/%s", gistIdW);
        success = GitHubAPI_Request(L"PATCH", path, body, response, sizeof(response), &httpStatus);
    } else {
        // Create new gist
        success = GitHubAPI_Request(L"POST", L"/gists", body, response, sizeof(response), &httpStatus);

        if (success) {
            // Extract gist ID from response and save it
            char gistId[64];
            if (JsonGetString(response, "id", gistId, sizeof(gistId))) {
                SaveNoteGistId(noteId, gistId);
            }
        }
    }

    free(body);
    return success;
}

// Delete a gist
BOOL GitHubSync_DeleteGist(int noteId) {
    char gistId[64];
    if (!GetNoteGistId(noteId, gistId, sizeof(gistId))) {
        return TRUE; // No gist to delete
    }

    WCHAR path[256];
    WCHAR gistIdW[64];
    MultiByteToWideChar(CP_UTF8, 0, gistId, -1, gistIdW, 64);
    swprintf_s(path, 256, L"/gists/%s", gistIdW);

    int httpStatus = 0;
    return GitHubAPI_Request(L"DELETE", path, NULL, NULL, 0, &httpStatus);
}

// Download all gists and import as notes
int GitHubSync_DownloadAll(HWND hParent) {
    char* response = (char*)malloc(1024 * 1024); // 1MB for gist list
    if (!response) return 0;

    int httpStatus = 0;
    if (!GitHubAPI_Request(L"GET", L"/gists", NULL, response, 1024 * 1024, &httpStatus)) {
        free(response);
        MessageBoxW(hParent, L"Failed to fetch gists from GitHub.", APP_NAME, MB_ICONERROR);
        return 0;
    }

    int imported = 0;

    // Parse gist list (array of gists)
    // Look for each gist's id and fetch its content
    char* ptr = response;
    while ((ptr = strstr(ptr, "\"id\":\"")) != NULL) {
        ptr += 6;
        char gistId[64];
        int i = 0;
        while (*ptr && *ptr != '"' && i < 63) {
            gistId[i++] = *ptr++;
        }
        gistId[i] = '\0';

        // Check if this gist is already imported
        char sql[256];
        snprintf(sql, sizeof(sql), "SELECT id FROM notes WHERE gist_id = '%s'", gistId);
        sqlite3_stmt* stmt;
        BOOL alreadyExists = FALSE;
        if (sqlite3_prepare_v2(Database_GetHandle(), sql, -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                alreadyExists = TRUE;
            }
            sqlite3_finalize(stmt);
        }

        if (alreadyExists) continue;

        // Fetch full gist content
        WCHAR gistPath[128];
        WCHAR gistIdW[64];
        MultiByteToWideChar(CP_UTF8, 0, gistId, -1, gistIdW, 64);
        swprintf_s(gistPath, 128, L"/gists/%s", gistIdW);

        char* gistContent = (char*)malloc(512 * 1024);
        if (!gistContent) continue;

        if (GitHubAPI_Request(L"GET", gistPath, NULL, gistContent, 512 * 1024, &httpStatus)) {
            // Extract description as title
            char title[MAX_TITLE_LEN * 3] = "Imported Gist";
            JsonGetString(gistContent, "description", title, sizeof(title));

            // Remove "OpenNote: " prefix if present
            char* titleStart = title;
            if (strncmp(title, "OpenNote: ", 10) == 0) {
                titleStart = title + 10;
            }

            // Find file content - look for "content" inside "files"
            char* filesSection = strstr(gistContent, "\"files\"");
            if (filesSection) {
                char* contentStart = strstr(filesSection, "\"content\":\"");
                if (contentStart) {
                    contentStart += 11;

                    // Parse the content string (handling escapes)
                    char* content = (char*)malloc(512 * 1024);
                    if (content) {
                        int j = 0;
                        while (*contentStart && j < 512 * 1024 - 1) {
                            if (*contentStart == '"') break;
                            if (*contentStart == '\\' && *(contentStart + 1)) {
                                contentStart++;
                                switch (*contentStart) {
                                    case 'n': content[j++] = '\n'; break;
                                    case 'r': content[j++] = '\r'; break;
                                    case 't': content[j++] = '\t'; break;
                                    case '"': content[j++] = '"'; break;
                                    case '\\': content[j++] = '\\'; break;
                                    default: content[j++] = *contentStart; break;
                                }
                            } else {
                                content[j++] = *contentStart;
                            }
                            contentStart++;
                        }
                        content[j] = '\0';

                        // Convert to wide strings
                        WCHAR titleW[MAX_TITLE_LEN];
                        WCHAR* contentW = (WCHAR*)malloc((j + 1) * sizeof(WCHAR));

                        if (contentW) {
                            MultiByteToWideChar(CP_UTF8, 0, titleStart, -1, titleW, MAX_TITLE_LEN);
                            MultiByteToWideChar(CP_UTF8, 0, content, -1, contentW, j + 1);

                            // Create note
                            int noteId = Notes_Create(titleW, contentW);
                            if (noteId > 0) {
                                SaveNoteGistId(noteId, gistId);
                                imported++;
                            }

                            free(contentW);
                        }
                        free(content);
                    }
                }
            }
        }
        free(gistContent);
    }

    free(response);
    return imported;
}

// Sync all notes
BOOL GitHubSync_SyncAll(HWND hParent) {
    // First, upload all local notes that have been modified
    NoteListItem* items = NULL;
    int count = 0;
    Notes_GetList(&items, &count);

    int uploaded = 0;
    int failed = 0;

    for (int i = 0; i < count; i++) {
        WCHAR* content = Notes_GetContent(items[i].id);
        if (content) {
            if (GitHubSync_UploadNote(items[i].id, items[i].title, content)) {
                uploaded++;
            } else {
                failed++;
            }
            free(content);
        }
    }
    Notes_FreeList(items);

    // Then download any new gists
    int downloaded = GitHubSync_DownloadAll(hParent);

    // Show summary
    WCHAR msg[256];
    swprintf_s(msg, 256, L"Sync complete!\n\nUploaded: %d notes\nDownloaded: %d notes\nFailed: %d",
               uploaded, downloaded, failed);
    MessageBoxW(hParent, msg, APP_NAME, MB_ICONINFORMATION);

    return (failed == 0);
}
