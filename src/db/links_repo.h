#ifndef LINKS_REPO_H
#define LINKS_REPO_H

// Target types
#define LINK_TARGET_FILE    0
#define LINK_TARGET_NOTE    1
#define LINK_TARGET_URL     2

// Link structure
typedef struct {
    int id;
    int sourceType;         // DOC_TYPE_FILE or DOC_TYPE_NOTE
    WCHAR sourcePath[MAX_PATH];
    int sourceNoteId;
    WCHAR linkText[MAX_TITLE_LEN];
    int startPos;
    int endPos;
    int targetType;         // LINK_TARGET_FILE, LINK_TARGET_NOTE, or LINK_TARGET_URL
    WCHAR targetPath[MAX_PATH];
    int targetNoteId;
    WCHAR targetURL[1024];
} Link;

// Link array result
typedef struct {
    Link* items;
    int count;
    int capacity;
} LinkArray;

// Create a new link to file or note
int Links_Create(int sourceType, const WCHAR* sourcePath, int sourceNoteId,
                 const WCHAR* linkText, int startPos, int endPos,
                 int targetType, const WCHAR* targetPath, int targetNoteId);

// Create a new link to URL
int Links_CreateURL(int sourceType, const WCHAR* sourcePath, int sourceNoteId,
                    const WCHAR* linkText, int startPos, int endPos,
                    const WCHAR* targetURL);

// Delete a link
BOOL Links_Delete(int linkId);

// Delete all links for a source document
BOOL Links_DeleteForSource(int sourceType, const WCHAR* sourcePath, int sourceNoteId);

// Get all links for a source document
LinkArray* Links_GetForSource(int sourceType, const WCHAR* sourcePath, int sourceNoteId);

// Get link at specific position in source document
Link* Links_GetAtPosition(int sourceType, const WCHAR* sourcePath, int sourceNoteId, int position);

// Update link positions after text edit (shift positions)
BOOL Links_UpdatePositions(int sourceType, const WCHAR* sourcePath, int sourceNoteId,
                          int editPos, int deltaLen);

// Free link array
void Links_FreeArray(LinkArray* array);

// Free single link
void Links_Free(Link* link);

#endif // LINKS_REPO_H
