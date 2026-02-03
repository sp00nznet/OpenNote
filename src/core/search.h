#ifndef SEARCH_H
#define SEARCH_H

// Search options
typedef struct {
    BOOL matchCase;
    BOOL wholeWord;
    BOOL wrapAround;
    BOOL searchUp;
} SearchOptions;

// Search functions
int Search_Find(const WCHAR* text, int textLen, const WCHAR* pattern, int start, SearchOptions* options);
int Search_FindReverse(const WCHAR* text, int textLen, const WCHAR* pattern, int start, SearchOptions* options);

// Word boundary checking
BOOL Search_IsWordBoundary(const WCHAR* text, int textLen, int pos);
BOOL Search_IsWholeWord(const WCHAR* text, int textLen, int matchStart, int matchLen);

// Case-insensitive comparison
int Search_CompareNoCase(const WCHAR* s1, const WCHAR* s2, int len);

#endif // SEARCH_H
