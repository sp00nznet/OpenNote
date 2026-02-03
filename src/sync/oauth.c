// Winsock must be included before windows.h
#include <winsock2.h>
#include <ws2tcpip.h>

#include "supernote.h"
#include "sync/oauth.h"
#include <winhttp.h>
#include <shellapi.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

// Local server port for OAuth callback
#define OAUTH_CALLBACK_PORT 8547

// GitHub OAuth endpoints
#define GITHUB_AUTH_URL "https://github.com/login/oauth/authorize"
#define GITHUB_TOKEN_URL "https://github.com/login/oauth/access_token"

// Google OAuth endpoints
#define GOOGLE_AUTH_URL "https://accounts.google.com/o/oauth2/v2/auth"
#define GOOGLE_TOKEN_URL "https://oauth2.googleapis.com/token"

// Callback server state
static SOCKET g_listenSocket = INVALID_SOCKET;
static volatile BOOL g_callbackReceived = FALSE;
static char g_authCode[512] = {0};

// Start local callback server
static BOOL StartCallbackServer(void) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return FALSE;
    }

    g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return FALSE;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(OAUTH_CALLBACK_PORT);

    if (bind(g_listenSocket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(g_listenSocket);
        WSACleanup();
        return FALSE;
    }

    if (listen(g_listenSocket, 1) == SOCKET_ERROR) {
        closesocket(g_listenSocket);
        WSACleanup();
        return FALSE;
    }

    return TRUE;
}

// Stop callback server
static void StopCallbackServer(void) {
    if (g_listenSocket != INVALID_SOCKET) {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

// Wait for OAuth callback (blocking with timeout)
static BOOL WaitForCallback(int timeoutSeconds) {
    g_callbackReceived = FALSE;
    g_authCode[0] = '\0';

    // Set socket timeout
    DWORD timeout = timeoutSeconds * 1000;
    setsockopt(g_listenSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    // Accept connection
    SOCKET clientSocket = accept(g_listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        return FALSE;
    }

    // Read request
    char buffer[4096] = {0};
    int received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
        buffer[received] = '\0';

        // Parse auth code from URL: GET /callback?code=XXX&state=YYY HTTP/1.1
        char* codeStart = strstr(buffer, "code=");
        if (codeStart) {
            codeStart += 5;
            char* codeEnd = strpbrk(codeStart, "& \r\n");
            if (codeEnd) {
                int codeLen = (int)(codeEnd - codeStart);
                if (codeLen < sizeof(g_authCode)) {
                    strncpy_s(g_authCode, sizeof(g_authCode), codeStart, codeLen);
                    g_callbackReceived = TRUE;
                }
            }
        }
    }

    // Send response page
    const char* successPage =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html><html><head><title>OpenNote</title>"
        "<style>body{font-family:system-ui;display:flex;justify-content:center;"
        "align-items:center;height:100vh;margin:0;background:#f0f0f0;}"
        ".box{background:white;padding:40px;border-radius:8px;text-align:center;"
        "box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
        "h1{color:#2ecc71;margin:0 0 10px 0;}p{color:#666;}</style></head>"
        "<body><div class='box'><h1>Success!</h1>"
        "<p>You can close this window and return to OpenNote.</p></div></body></html>";

    const char* errorPage =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html><html><head><title>OpenNote</title>"
        "<style>body{font-family:system-ui;display:flex;justify-content:center;"
        "align-items:center;height:100vh;margin:0;background:#f0f0f0;}"
        ".box{background:white;padding:40px;border-radius:8px;text-align:center;"
        "box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
        "h1{color:#e74c3c;margin:0 0 10px 0;}p{color:#666;}</style></head>"
        "<body><div class='box'><h1>Error</h1>"
        "<p>Authorization failed. Please try again.</p></div></body></html>";

    send(clientSocket, g_callbackReceived ? successPage : errorPage,
         (int)strlen(g_callbackReceived ? successPage : errorPage), 0);

    closesocket(clientSocket);
    return g_callbackReceived;
}

// Exchange auth code for tokens (GitHub)
static BOOL ExchangeCodeForToken_GitHub(const char* code, OAuthToken* tokenOut) {
#ifndef GITHUB_CLIENT_ID
    return FALSE;
#else
    HINTERNET hSession = WinHttpOpen(L"OpenNote/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return FALSE;

    HINTERNET hConnect = WinHttpConnect(hSession, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/login/oauth/access_token",
                                             NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Build POST data
    char postData[1024];
    snprintf(postData, sizeof(postData),
             "client_id=%s&client_secret=%s&code=%s",
             GITHUB_CLIENT_ID, GITHUB_CLIENT_SECRET, code);

    // Set headers
    WinHttpAddRequestHeaders(hRequest, L"Accept: application/json", -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/x-www-form-urlencoded", -1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL result = FALSE;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           postData, (DWORD)strlen(postData), (DWORD)strlen(postData), 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            char response[4096] = {0};
            DWORD bytesRead = 0;
            DWORD totalRead = 0;

            while (WinHttpReadData(hRequest, response + totalRead,
                                   sizeof(response) - totalRead - 1, &bytesRead) && bytesRead > 0) {
                totalRead += bytesRead;
            }

            // Parse JSON response (simple parsing)
            char* accessToken = strstr(response, "\"access_token\":\"");
            if (accessToken) {
                accessToken += 16;
                char* tokenEnd = strchr(accessToken, '"');
                if (tokenEnd && (tokenEnd - accessToken) < sizeof(tokenOut->accessToken)) {
                    strncpy_s(tokenOut->accessToken, sizeof(tokenOut->accessToken),
                              accessToken, tokenEnd - accessToken);
                    tokenOut->isAuthenticated = TRUE;
                    strcpy_s(tokenOut->tokenType, sizeof(tokenOut->tokenType), "bearer");
                    result = TRUE;
                }
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
#endif
}

// Exchange auth code for tokens (Google)
static BOOL ExchangeCodeForToken_Google(const char* code, OAuthToken* tokenOut) {
#ifndef GOOGLE_CLIENT_ID
    return FALSE;
#else
    HINTERNET hSession = WinHttpOpen(L"OpenNote/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return FALSE;

    HINTERNET hConnect = WinHttpConnect(hSession, L"oauth2.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/token",
                                             NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Build POST data
    char postData[1024];
    snprintf(postData, sizeof(postData),
             "client_id=%s&client_secret=%s&code=%s&grant_type=authorization_code&redirect_uri=http://localhost:%d/callback",
             GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, code, OAUTH_CALLBACK_PORT);

    // Set headers
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/x-www-form-urlencoded", -1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL result = FALSE;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           postData, (DWORD)strlen(postData), (DWORD)strlen(postData), 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            char response[4096] = {0};
            DWORD bytesRead = 0;
            DWORD totalRead = 0;

            while (WinHttpReadData(hRequest, response + totalRead,
                                   sizeof(response) - totalRead - 1, &bytesRead) && bytesRead > 0) {
                totalRead += bytesRead;
            }

            // Always show response for debugging
            WCHAR debugMsg[2048];
            MultiByteToWideChar(CP_UTF8, 0, response, -1, debugMsg, 2048);
            MessageBoxW(NULL, debugMsg, L"Google Token Response", MB_ICONINFORMATION);

            // Check for error response
            char* error = strstr(response, "\"error\":");
            if (error) {
                // Error already shown above
            }

            // Parse JSON response
            char* accessToken = strstr(response, "\"access_token\"");
            if (accessToken) {
                // Find the value after the colon and opening quote
                accessToken = strchr(accessToken, ':');
                if (accessToken) {
                    accessToken = strchr(accessToken, '"');
                    if (accessToken) {
                        accessToken++; // Skip the quote
                        char* tokenEnd = strchr(accessToken, '"');
                        if (tokenEnd && (tokenEnd - accessToken) < sizeof(tokenOut->accessToken)) {
                            strncpy_s(tokenOut->accessToken, sizeof(tokenOut->accessToken),
                                      accessToken, tokenEnd - accessToken);
                            tokenOut->isAuthenticated = TRUE;
                            strcpy_s(tokenOut->tokenType, sizeof(tokenOut->tokenType), "Bearer");
                            result = TRUE;

                            // Also get refresh token if present
                            char* refreshToken = strstr(response, "\"refresh_token\"");
                            if (refreshToken) {
                                refreshToken = strchr(refreshToken, ':');
                                if (refreshToken) {
                                    refreshToken = strchr(refreshToken, '"');
                                    if (refreshToken) {
                                        refreshToken++;
                                        char* refreshEnd = strchr(refreshToken, '"');
                                        if (refreshEnd && (refreshEnd - refreshToken) < sizeof(tokenOut->refreshToken)) {
                                            strncpy_s(tokenOut->refreshToken, sizeof(tokenOut->refreshToken),
                                                      refreshToken, refreshEnd - refreshToken);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
#endif
}

// GitHub OAuth login
BOOL OAuth_GitHubLogin(HWND hParent, OAuthToken* tokenOut) {
#ifndef GITHUB_CLIENT_ID
    MessageBoxW(hParent, L"GitHub OAuth is not configured in this build.", APP_NAME, MB_ICONWARNING);
    return FALSE;
#else
    memset(tokenOut, 0, sizeof(OAuthToken));

    // Start callback server
    if (!StartCallbackServer()) {
        MessageBoxW(hParent, L"Failed to start authentication server.", APP_NAME, MB_ICONERROR);
        return FALSE;
    }

    // Build authorization URL
    char authUrl[1024];
    snprintf(authUrl, sizeof(authUrl),
             "%s?client_id=%s&redirect_uri=http://localhost:%d/callback&scope=gist&state=opennote",
             GITHUB_AUTH_URL, GITHUB_CLIENT_ID, OAUTH_CALLBACK_PORT);

    // Open browser
    WCHAR authUrlW[1024];
    MultiByteToWideChar(CP_UTF8, 0, authUrl, -1, authUrlW, 1024);
    ShellExecuteW(NULL, L"open", authUrlW, NULL, NULL, SW_SHOWNORMAL);

    // Wait for callback (2 minute timeout)
    BOOL success = FALSE;
    if (WaitForCallback(120)) {
        // Exchange code for token
        success = ExchangeCodeForToken_GitHub(g_authCode, tokenOut);
        if (!success) {
            MessageBoxW(hParent, L"Failed to get access token from GitHub.", APP_NAME, MB_ICONERROR);
        }
    } else {
        MessageBoxW(hParent, L"Authorization timed out or was cancelled.", APP_NAME, MB_ICONWARNING);
    }

    StopCallbackServer();

    if (success) {
        OAuth_SaveToken("github", tokenOut);
    }

    return success;
#endif
}

// Google OAuth login
BOOL OAuth_GoogleLogin(HWND hParent, OAuthToken* tokenOut) {
#ifndef GOOGLE_CLIENT_ID
    MessageBoxW(hParent, L"Google OAuth is not configured in this build.", APP_NAME, MB_ICONWARNING);
    return FALSE;
#else
    memset(tokenOut, 0, sizeof(OAuthToken));

    // Start callback server
    if (!StartCallbackServer()) {
        MessageBoxW(hParent, L"Failed to start authentication server.", APP_NAME, MB_ICONERROR);
        return FALSE;
    }

    // Build authorization URL
    char authUrl[1024];
    snprintf(authUrl, sizeof(authUrl),
             "%s?client_id=%s&redirect_uri=http://localhost:%d/callback"
             "&response_type=code&scope=https://www.googleapis.com/auth/drive.file"
             "&access_type=offline&state=opennote",
             GOOGLE_AUTH_URL, GOOGLE_CLIENT_ID, OAUTH_CALLBACK_PORT);

    // Open browser
    WCHAR authUrlW[1024];
    MultiByteToWideChar(CP_UTF8, 0, authUrl, -1, authUrlW, 1024);
    ShellExecuteW(NULL, L"open", authUrlW, NULL, NULL, SW_SHOWNORMAL);

    // Wait for callback (2 minute timeout)
    BOOL success = FALSE;
    if (WaitForCallback(120)) {
        // Exchange code for token
        success = ExchangeCodeForToken_Google(g_authCode, tokenOut);
        if (!success) {
            MessageBoxW(hParent, L"Failed to get access token from Google.", APP_NAME, MB_ICONERROR);
        }
    } else {
        MessageBoxW(hParent, L"Authorization timed out or was cancelled.", APP_NAME, MB_ICONWARNING);
    }

    StopCallbackServer();

    if (success) {
        OAuth_SaveToken("google", tokenOut);
    }

    return success;
#endif
}

// Save token to database
BOOL OAuth_SaveToken(const char* provider, const OAuthToken* token) {
    if (!Database_IsOpen() || !provider || !token) return FALSE;

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "INSERT OR REPLACE INTO settings (key, value) VALUES ('oauth_%s_token', '%s')",
             provider, token->accessToken);
    Database_Execute(sql);

    if (token->refreshToken[0]) {
        snprintf(sql, sizeof(sql),
                 "INSERT OR REPLACE INTO settings (key, value) VALUES ('oauth_%s_refresh', '%s')",
                 provider, token->refreshToken);
        Database_Execute(sql);
    }

    return TRUE;
}

// Load token from database
BOOL OAuth_LoadToken(const char* provider, OAuthToken* token) {
    if (!Database_IsOpen() || !provider || !token) return FALSE;

    memset(token, 0, sizeof(OAuthToken));

    char key[64];
    snprintf(key, sizeof(key), "oauth_%s_token", provider);

    sqlite3_stmt* stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT value FROM settings WHERE key = '%s'", key);

    if (sqlite3_prepare_v2(Database_GetHandle(), sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* value = (const char*)sqlite3_column_text(stmt, 0);
            if (value && value[0]) {
                strncpy_s(token->accessToken, sizeof(token->accessToken), value, _TRUNCATE);
                token->isAuthenticated = TRUE;
            }
        }
        sqlite3_finalize(stmt);
    }

    // Load refresh token
    snprintf(key, sizeof(key), "oauth_%s_refresh", provider);
    snprintf(sql, sizeof(sql), "SELECT value FROM settings WHERE key = '%s'", key);

    if (sqlite3_prepare_v2(Database_GetHandle(), sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* value = (const char*)sqlite3_column_text(stmt, 0);
            if (value && value[0]) {
                strncpy_s(token->refreshToken, sizeof(token->refreshToken), value, _TRUNCATE);
            }
        }
        sqlite3_finalize(stmt);
    }

    return token->isAuthenticated;
}

// Delete token from database
void OAuth_DeleteToken(const char* provider) {
    if (!Database_IsOpen() || !provider) return;

    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM settings WHERE key LIKE 'oauth_%s_%%'", provider);
    Database_Execute(sql);
}

// Logout functions
void OAuth_GitHubLogout(void) {
    OAuth_DeleteToken("github");
}

void OAuth_GoogleLogout(void) {
    OAuth_DeleteToken("google");
}

// Refresh token implementations (simplified - just re-login for now)
BOOL OAuth_GitHubRefresh(const char* refreshToken, OAuthToken* tokenOut) {
    (void)refreshToken;
    (void)tokenOut;
    // GitHub doesn't use refresh tokens for OAuth apps
    return FALSE;
}

BOOL OAuth_GoogleRefresh(const char* refreshToken, OAuthToken* tokenOut) {
    (void)refreshToken;
    (void)tokenOut;
    // TODO: Implement refresh token exchange
    return FALSE;
}
