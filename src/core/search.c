#include "supernote.h"

// Case-insensitive comparison
int Search_CompareNoCase(const WCHAR* s1, const WCHAR* s2, int len) {
    for (int i = 0; i < len; i++) {
        WCHAR c1 = towlower(s1[i]);
        WCHAR c2 = towlower(s2[i]);
        if (c1 != c2) return c1 - c2;
    }
    return 0;
}

// Check if position is a word boundary
BOOL Search_IsWordBoundary(const WCHAR* text, int textLen, int pos) {
    if (pos <= 0 || pos >= textLen) return TRUE;

    BOOL prevAlnum = iswalnum(text[pos - 1]);
    BOOL currAlnum = iswalnum(text[pos]);

    return prevAlnum != currAlnum;
}

// Check if match is a whole word
BOOL Search_IsWholeWord(const WCHAR* text, int textLen, int matchStart, int matchLen) {
    // Check before match
    if (matchStart > 0 && iswalnum(text[matchStart - 1])) {
        return FALSE;
    }

    // Check after match
    int matchEnd = matchStart + matchLen;
    if (matchEnd < textLen && iswalnum(text[matchEnd])) {
        return FALSE;
    }

    return TRUE;
}

// Find text forward
int Search_Find(const WCHAR* text, int textLen, const WCHAR* pattern, int start, SearchOptions* options) {
    if (!text || !pattern || textLen <= 0) return -1;

    int patternLen = (int)wcslen(pattern);
    if (patternLen <= 0 || patternLen > textLen) return -1;

    for (int i = start; i <= textLen - patternLen; i++) {
        BOOL match;
        if (options && options->matchCase) {
            match = (wcsncmp(text + i, pattern, patternLen) == 0);
        } else {
            match = (Search_CompareNoCase(text + i, pattern, patternLen) == 0);
        }

        if (match) {
            if (options && options->wholeWord) {
                if (!Search_IsWholeWord(text, textLen, i, patternLen)) {
                    continue;
                }
            }
            return i;
        }
    }

    return -1;
}

// Find text backward
int Search_FindReverse(const WCHAR* text, int textLen, const WCHAR* pattern, int start, SearchOptions* options) {
    if (!text || !pattern || textLen <= 0) return -1;

    int patternLen = (int)wcslen(pattern);
    if (patternLen <= 0 || patternLen > textLen) return -1;

    if (start < 0 || start > textLen) {
        start = textLen;
    }

    for (int i = start - patternLen; i >= 0; i--) {
        BOOL match;
        if (options && options->matchCase) {
            match = (wcsncmp(text + i, pattern, patternLen) == 0);
        } else {
            match = (Search_CompareNoCase(text + i, pattern, patternLen) == 0);
        }

        if (match) {
            if (options && options->wholeWord) {
                if (!Search_IsWholeWord(text, textLen, i, patternLen)) {
                    continue;
                }
            }
            return i;
        }
    }

    return -1;
}
