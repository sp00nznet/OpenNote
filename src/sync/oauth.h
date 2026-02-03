#ifndef OAUTH_H
#define OAUTH_H

#include <windows.h>

// OAuth state
typedef struct {
    char accessToken[1024];
    char refreshToken[1024];
    char tokenType[32];
    int expiresIn;
    BOOL isAuthenticated;
} OAuthToken;

// GitHub OAuth
BOOL OAuth_GitHubLogin(HWND hParent, OAuthToken* tokenOut);
BOOL OAuth_GitHubRefresh(const char* refreshToken, OAuthToken* tokenOut);
void OAuth_GitHubLogout(void);

// Google OAuth
BOOL OAuth_GoogleLogin(HWND hParent, OAuthToken* tokenOut);
BOOL OAuth_GoogleRefresh(const char* refreshToken, OAuthToken* tokenOut);
void OAuth_GoogleLogout(void);

// Token storage
BOOL OAuth_SaveToken(const char* provider, const OAuthToken* token);
BOOL OAuth_LoadToken(const char* provider, OAuthToken* token);
void OAuth_DeleteToken(const char* provider);

#endif // OAUTH_H
