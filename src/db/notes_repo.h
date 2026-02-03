#ifndef NOTES_REPO_H
#define NOTES_REPO_H

// Note structure for queries
typedef struct {
    int id;
    WCHAR title[MAX_TITLE_LEN];
    WCHAR* content;  // Dynamically allocated
    WCHAR createdAt[32];
    WCHAR updatedAt[32];
    BOOL isPinned;
    BOOL isArchived;
    int categoryId;
} Note;

// Note list item for browser
typedef struct {
    int id;
    WCHAR title[MAX_TITLE_LEN];
    WCHAR updatedAt[32];
    BOOL isPinned;
} NoteListItem;

// CRUD operations
int Notes_Create(const WCHAR* title, const WCHAR* content);
BOOL Notes_Read(int id, Note* note);
BOOL Notes_Update(int id, const WCHAR* title, const WCHAR* content);
BOOL Notes_Delete(int id);

// Content operations
WCHAR* Notes_GetContent(int id);  // Caller must free
BOOL Notes_SetContent(int id, const WCHAR* content);

// Title operations
BOOL Notes_GetTitle(int id, WCHAR* buffer, int bufferSize);
BOOL Notes_SetTitle(int id, const WCHAR* title);

// Listing
int Notes_GetList(NoteListItem** items, int* count);  // Returns array, caller must free
int Notes_Search(const WCHAR* query, NoteListItem** items, int* count);  // FTS5 search

// Pin/Archive
BOOL Notes_SetPinned(int id, BOOL pinned);
BOOL Notes_SetArchived(int id, BOOL archived);

// Cleanup
void Notes_FreeNote(Note* note);
void Notes_FreeList(NoteListItem* items);

#endif // NOTES_REPO_H
