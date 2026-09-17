// Thin wrappers over the crypto Windows already ships: DPAPI for at-rest
// protection of tokens, and CNG for the SHA-256 and random bytes that PKCE
// needs. Nothing here implements a primitive.

#include "supernote.h"
#include "sync/crypto.h"

#include <wincrypt.h>
#include <bcrypt.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

// base64 -> base64url, in place: RFC 4648 section 5, minus the padding that
// OAuth endpoints reject.
static void ToBase64Url(char* s) {
    char* w = s;
    for (char* r = s; *r; r++) {
        if (*r == '+')      *w++ = '-';
        else if (*r == '/') *w++ = '_';
        else if (*r == '=') continue;
        else                *w++ = *r;
    }
    *w = '\0';
}

static BOOL EncodeBase64(const BYTE* data, DWORD len, char* out, size_t outSize) {
    DWORD needed = 0;
    if (!CryptBinaryToStringA(data, len, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              NULL, &needed)) {
        return FALSE;
    }
    if (needed > outSize) return FALSE;

    return CryptBinaryToStringA(data, len, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                out, &needed);
}

BOOL Crypto_ProtectToBase64(const char* plain, char* out, size_t outSize) {
    if (!plain || !out || outSize == 0) return FALSE;
    out[0] = '\0';

    DATA_BLOB in = { (DWORD)strlen(plain) + 1, (BYTE*)plain };
    DATA_BLOB enc = { 0, NULL };

    // CRYPTPROTECT_UI_FORBIDDEN: this runs on a save path, and a surprise
    // credential prompt there would be worse than failing.
    if (!CryptProtectData(&in, L"OpenNote token", NULL, NULL, NULL,
                          CRYPTPROTECT_UI_FORBIDDEN, &enc)) {
        return FALSE;
    }

    BOOL ok = EncodeBase64(enc.pbData, enc.cbData, out, outSize);

    SecureZeroMemory(enc.pbData, enc.cbData);
    LocalFree(enc.pbData);
    return ok;
}

BOOL Crypto_UnprotectFromBase64(const char* base64, char* out, size_t outSize) {
    if (!base64 || !out || outSize == 0) return FALSE;
    out[0] = '\0';

    DWORD binLen = 0;
    if (!CryptStringToBinaryA(base64, 0, CRYPT_STRING_BASE64, NULL, &binLen, NULL, NULL)) {
        return FALSE;
    }

    BYTE* bin = (BYTE*)malloc(binLen);
    if (!bin) return FALSE;

    BOOL ok = FALSE;
    if (CryptStringToBinaryA(base64, 0, CRYPT_STRING_BASE64, bin, &binLen, NULL, NULL)) {
        DATA_BLOB in = { binLen, bin };
        DATA_BLOB dec = { 0, NULL };

        if (CryptUnprotectData(&in, NULL, NULL, NULL, NULL,
                               CRYPTPROTECT_UI_FORBIDDEN, &dec)) {
            // The blob was written with its terminator; do not trust that it
            // still has one.
            size_t len = dec.cbData;
            if (len > 0 && dec.pbData[len - 1] == '\0') len--;
            if (len < outSize) {
                memcpy(out, dec.pbData, len);
                out[len] = '\0';
                ok = TRUE;
            }
            SecureZeroMemory(dec.pbData, dec.cbData);
            LocalFree(dec.pbData);
        }
    }

    free(bin);
    return ok;
}

BOOL Crypto_RandomBase64Url(char* out, size_t outSize, size_t nbytes) {
    if (!out || outSize == 0 || nbytes == 0 || nbytes > 128) return FALSE;
    out[0] = '\0';

    BYTE buf[128];
    if (BCryptGenRandom(NULL, buf, (ULONG)nbytes,
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != STATUS_SUCCESS) {
        return FALSE;
    }

    BOOL ok = EncodeBase64(buf, (DWORD)nbytes, out, outSize);
    SecureZeroMemory(buf, sizeof(buf));
    if (ok) ToBase64Url(out);
    return ok;
}

BOOL Crypto_Sha256Base64Url(const char* in, char* out, size_t outSize) {
    if (!in || !out || outSize == 0) return FALSE;
    out[0] = '\0';

    BYTE digest[32];
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, NULL, 0,
                   (PUCHAR)in, (ULONG)strlen(in),
                   digest, sizeof(digest)) != STATUS_SUCCESS) {
        return FALSE;
    }

    BOOL ok = EncodeBase64(digest, sizeof(digest), out, outSize);
    if (ok) ToBase64Url(out);
    return ok;
}

// ---------------------------------------------------------------------------
// Self-check. Run with: OpenNote.exe --selftest
//
// Deliberately not assert(): this has to keep working in a Release build, where
// NDEBUG would compile assert away and the check would silently pass.
// ---------------------------------------------------------------------------

BOOL Crypto_SelfTest(char* failure, size_t failureSize) {
#define FAIL(msg) do { strncpy_s(failure, failureSize, (msg), _TRUNCATE); return FALSE; } while (0)

    // RFC 7636 appendix B test vector. If base64url encoding or the SHA-256 is
    // wrong in any way, this is the line that catches it.
    const char* rfcVerifier  = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    const char* rfcChallenge = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";

    char challenge[64];
    if (!Crypto_Sha256Base64Url(rfcVerifier, challenge, sizeof(challenge))) {
        FAIL("Crypto_Sha256Base64Url failed");
    }
    if (strcmp(challenge, rfcChallenge) != 0) {
        FAIL("PKCE S256 challenge does not match RFC 7636 appendix B");
    }

    // A verifier must be 43-128 characters of the unreserved set (RFC 7636
    // section 4.1); 32 random bytes base64url-encoded is 43.
    char verifier[128];
    if (!Crypto_RandomBase64Url(verifier, sizeof(verifier), 32)) {
        FAIL("Crypto_RandomBase64Url failed");
    }
    size_t vlen = strlen(verifier);
    if (vlen < 43 || vlen > 128) FAIL("PKCE verifier length out of range");
    if (strpbrk(verifier, "+/=")) FAIL("PKCE verifier is base64, not base64url");

    // Two calls must not agree, or the randomness is not random.
    char verifier2[128];
    if (!Crypto_RandomBase64Url(verifier2, sizeof(verifier2), 32)) {
        FAIL("Crypto_RandomBase64Url failed on second call");
    }
    if (strcmp(verifier, verifier2) == 0) FAIL("PKCE verifier repeated across calls");

    // DPAPI round trip, including a value long enough to exercise the buffers a
    // real access token uses.
    const char* secret = "gho_16C7e42F292c6912E7710c838347Ae178B4a";
    char wrapped[4096], unwrapped[1024];

    if (!Crypto_ProtectToBase64(secret, wrapped, sizeof(wrapped))) {
        FAIL("Crypto_ProtectToBase64 failed");
    }
    if (strstr(wrapped, secret)) FAIL("DPAPI output contains the plaintext");
    if (!Crypto_UnprotectFromBase64(wrapped, unwrapped, sizeof(unwrapped))) {
        FAIL("Crypto_UnprotectFromBase64 failed");
    }
    if (strcmp(secret, unwrapped) != 0) FAIL("DPAPI round trip changed the value");

    // Truncation must fail rather than hand back a short token that would then
    // be sent to a provider as if it were whole.
    char tooSmall[8];
    if (Crypto_UnprotectFromBase64(wrapped, tooSmall, sizeof(tooSmall))) {
        FAIL("Crypto_UnprotectFromBase64 accepted an undersized buffer");
    }

    // Garbage in must not be reported as success.
    if (Crypto_UnprotectFromBase64("not base64 at all !!!", unwrapped, sizeof(unwrapped))) {
        FAIL("Crypto_UnprotectFromBase64 accepted malformed input");
    }

    failure[0] = '\0';
    return TRUE;

#undef FAIL
}
