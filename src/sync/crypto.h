#ifndef CRYPTO_H
#define CRYPTO_H

#include <windows.h>

// DPAPI wrapping. The ciphertext is tied to the current Windows user account,
// so a copy of opennote.db taken to another machine or opened by another user
// yields nothing. Output is base64 so it can live in a TEXT column.
BOOL Crypto_ProtectToBase64(const char* plain, char* out, size_t outSize);
BOOL Crypto_UnprotectFromBase64(const char* base64, char* out, size_t outSize);

// PKCE helpers (RFC 7636). The verifier is base64url of `nbytes` random bytes;
// the challenge is base64url of its SHA-256, which is the S256 method.
BOOL Crypto_RandomBase64Url(char* out, size_t outSize, size_t nbytes);
BOOL Crypto_Sha256Base64Url(const char* in, char* out, size_t outSize);

// Self-check, run by `OpenNote.exe --selftest`. Returns FALSE and fills
// `failure` with the first check that did not hold.
BOOL Crypto_SelfTest(char* failure, size_t failureSize);

#endif // CRYPTO_H
