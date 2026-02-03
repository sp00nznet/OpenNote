#include "supernote.h"

// UTF-8 BOM
static const BYTE UTF8_BOM[] = { 0xEF, 0xBB, 0xBF };
// UTF-16 LE BOM
static const BYTE UTF16LE_BOM[] = { 0xFF, 0xFE };
// UTF-16 BE BOM
static const BYTE UTF16BE_BOM[] = { 0xFE, 0xFF };

// Detect encoding from BOM (also returns actual BOM size found)
TextEncoding FileIO_DetectEncodingEx(const BYTE* data, size_t size, size_t* bomSizeOut) {
    if (size >= 3 && memcmp(data, UTF8_BOM, 3) == 0) {
        if (bomSizeOut) *bomSizeOut = 3;
        return ENCODING_UTF8;
    }
    if (size >= 2 && memcmp(data, UTF16LE_BOM, 2) == 0) {
        if (bomSizeOut) *bomSizeOut = 2;
        return ENCODING_UTF16_LE;
    }
    if (size >= 2 && memcmp(data, UTF16BE_BOM, 2) == 0) {
        if (bomSizeOut) *bomSizeOut = 2;
        return ENCODING_UTF16_BE;
    }

    // No BOM found
    if (bomSizeOut) *bomSizeOut = 0;

    // Check for UTF-8 validity (heuristic)
    BOOL isValidUtf8 = TRUE;
    for (size_t i = 0; i < size && i < 4096; i++) {
        BYTE c = data[i];
        if (c > 127) {
            // Check for valid UTF-8 sequence
            int seqLen = 0;
            if ((c & 0xE0) == 0xC0) seqLen = 2;
            else if ((c & 0xF0) == 0xE0) seqLen = 3;
            else if ((c & 0xF8) == 0xF0) seqLen = 4;
            else {
                isValidUtf8 = FALSE;
                break;
            }

            // Check continuation bytes
            for (int j = 1; j < seqLen && (i + j) < size; j++) {
                if ((data[i + j] & 0xC0) != 0x80) {
                    isValidUtf8 = FALSE;
                    break;
                }
            }
            if (!isValidUtf8) break;
            i += seqLen - 1;
        }
    }

    return isValidUtf8 ? ENCODING_UTF8 : ENCODING_ANSI;
}

// Legacy wrapper for compatibility
TextEncoding FileIO_DetectEncoding(const BYTE* data, size_t size) {
    return FileIO_DetectEncodingEx(data, size, NULL);
}

// Get BOM size for encoding
size_t FileIO_GetBOMSize(TextEncoding encoding) {
    switch (encoding) {
        case ENCODING_UTF8:     return 3;
        case ENCODING_UTF16_LE: return 2;
        case ENCODING_UTF16_BE: return 2;
        default:                return 0;
    }
}

// Write BOM to file
void FileIO_WriteBOM(HANDLE hFile, TextEncoding encoding) {
    DWORD written;
    switch (encoding) {
        case ENCODING_UTF8:
            WriteFile(hFile, UTF8_BOM, 3, &written, NULL);
            break;
        case ENCODING_UTF16_LE:
            WriteFile(hFile, UTF16LE_BOM, 2, &written, NULL);
            break;
        case ENCODING_UTF16_BE:
            WriteFile(hFile, UTF16BE_BOM, 2, &written, NULL);
            break;
        default:
            break;
    }
}

// Get encoding name
const WCHAR* FileIO_GetEncodingName(TextEncoding encoding) {
    switch (encoding) {
        case ENCODING_UTF8:     return L"UTF-8";
        case ENCODING_UTF16_LE: return L"UTF-16 LE";
        case ENCODING_UTF16_BE: return L"UTF-16 BE";
        case ENCODING_ANSI:     return L"ANSI";
        default:                return L"Unknown";
    }
}

// Read file to wide string
WCHAR* FileIO_ReadFile(const WCHAR* path, TextEncoding* detectedEncoding) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart > 100 * 1024 * 1024) {
        // Limit to 100MB
        CloseHandle(hFile);
        return NULL;
    }

    DWORD size = (DWORD)fileSize.QuadPart;
    BYTE* buffer = (BYTE*)malloc(size + 4);  // Extra for null terminators
    if (!buffer) {
        CloseHandle(hFile);
        return NULL;
    }

    DWORD bytesRead;
    if (!ReadFile(hFile, buffer, size, &bytesRead, NULL)) {
        free(buffer);
        CloseHandle(hFile);
        return NULL;
    }
    CloseHandle(hFile);

    // Detect encoding and actual BOM size
    size_t bomSize = 0;
    TextEncoding encoding = FileIO_DetectEncodingEx(buffer, bytesRead, &bomSize);
    if (detectedEncoding) {
        *detectedEncoding = encoding;
    }

    WCHAR* result = NULL;

    switch (encoding) {
        case ENCODING_UTF16_LE:
            {
                size_t charCount = (bytesRead - bomSize) / 2;
                result = (WCHAR*)malloc((charCount + 1) * sizeof(WCHAR));
                if (result) {
                    memcpy(result, buffer + bomSize, (bytesRead - bomSize));
                    result[charCount] = L'\0';
                }
            }
            break;

        case ENCODING_UTF16_BE:
            {
                size_t charCount = (bytesRead - bomSize) / 2;
                result = (WCHAR*)malloc((charCount + 1) * sizeof(WCHAR));
                if (result) {
                    const BYTE* src = buffer + bomSize;
                    for (size_t i = 0; i < charCount; i++) {
                        result[i] = (WCHAR)((src[i*2] << 8) | src[i*2+1]);
                    }
                    result[charCount] = L'\0';
                }
            }
            break;

        case ENCODING_UTF8:
        case ENCODING_ANSI:
        default:
            {
                const char* src = (const char*)(buffer + bomSize);
                int srcLen = bytesRead - (int)bomSize;
                UINT codePage = (encoding == ENCODING_UTF8) ? CP_UTF8 : CP_ACP;

                int wideLen = MultiByteToWideChar(codePage, 0, src, srcLen, NULL, 0);
                if (wideLen > 0) {
                    result = (WCHAR*)malloc((wideLen + 1) * sizeof(WCHAR));
                    if (result) {
                        MultiByteToWideChar(codePage, 0, src, srcLen, result, wideLen);
                        result[wideLen] = L'\0';
                    }
                }
            }
            break;
    }

    free(buffer);
    return result;
}

// Write wide string to file
BOOL FileIO_WriteFile(const WCHAR* path, const WCHAR* content, TextEncoding encoding) {
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    BOOL success = FALSE;
    DWORD written;

    switch (encoding) {
        case ENCODING_UTF16_LE:
            FileIO_WriteBOM(hFile, encoding);
            {
                size_t len = wcslen(content) * sizeof(WCHAR);
                success = WriteFile(hFile, content, (DWORD)len, &written, NULL);
            }
            break;

        case ENCODING_UTF16_BE:
            FileIO_WriteBOM(hFile, encoding);
            {
                size_t len = wcslen(content);
                BYTE* buffer = (BYTE*)malloc(len * 2);
                if (buffer) {
                    for (size_t i = 0; i < len; i++) {
                        buffer[i*2] = (BYTE)(content[i] >> 8);
                        buffer[i*2+1] = (BYTE)(content[i] & 0xFF);
                    }
                    success = WriteFile(hFile, buffer, (DWORD)(len * 2), &written, NULL);
                    free(buffer);
                }
            }
            break;

        case ENCODING_UTF8:
            // Write BOM for UTF-8
            FileIO_WriteBOM(hFile, encoding);
            // Fall through
        case ENCODING_ANSI:
        default:
            {
                UINT codePage = (encoding == ENCODING_UTF8) ? CP_UTF8 : CP_ACP;
                int len = WideCharToMultiByte(codePage, 0, content, -1, NULL, 0, NULL, NULL);
                if (len > 0) {
                    char* buffer = (char*)malloc(len);
                    if (buffer) {
                        WideCharToMultiByte(codePage, 0, content, -1, buffer, len, NULL, NULL);
                        // Don't write null terminator
                        success = WriteFile(hFile, buffer, len - 1, &written, NULL);
                        free(buffer);
                    }
                }
            }
            break;
    }

    CloseHandle(hFile);
    return success;
}
