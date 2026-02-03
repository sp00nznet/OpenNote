#include "supernote.h"

// Create a new note
int Notes_Create(const WCHAR* title, const WCHAR* content) {
    sqlite3* db = Database_GetHandle();
    if (!db) return -1;

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO notes (title, content) VALUES (?, ?)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    // Convert title to UTF-8
    char titleUtf8[MAX_TITLE_LEN * 3];
    WideCharToMultiByte(CP_UTF8, 0, title ? title : L"Untitled", -1,
                        titleUtf8, sizeof(titleUtf8), NULL, NULL);
    sqlite3_bind_text(stmt, 1, titleUtf8, -1, SQLITE_STATIC);

    // Convert content to UTF-8
    if (content && content[0]) {
        int contentLen = (int)wcslen(content);
        int bufSize = contentLen * 3 + 1;
        char* contentUtf8 = (char*)malloc(bufSize);
        if (contentUtf8) {
            WideCharToMultiByte(CP_UTF8, 0, content, -1, contentUtf8, bufSize, NULL, NULL);
            sqlite3_bind_text(stmt, 2, contentUtf8, -1, SQLITE_TRANSIENT);
            free(contentUtf8);
        } else {
            sqlite3_bind_text(stmt, 2, "", -1, SQLITE_STATIC);
        }
    } else {
        sqlite3_bind_text(stmt, 2, "", -1, SQLITE_STATIC);
    }

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        return -1;
    }

    return (int)sqlite3_last_insert_rowid(db);
}

// Read a note by ID
BOOL Notes_Read(int id, Note* note) {
    if (!note) return FALSE;

    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, title, content, created_at, updated_at, is_pinned, is_archived, category_id "
                      "FROM notes WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return FALSE;
    }

    note->id = sqlite3_column_int(stmt, 0);

    const char* title = (const char*)sqlite3_column_text(stmt, 1);
    MultiByteToWideChar(CP_UTF8, 0, title ? title : "", -1, note->title, MAX_TITLE_LEN);

    const char* content = (const char*)sqlite3_column_text(stmt, 2);
    if (content) {
        int len = MultiByteToWideChar(CP_UTF8, 0, content, -1, NULL, 0);
        note->content = (WCHAR*)malloc(len * sizeof(WCHAR));
        if (note->content) {
            MultiByteToWideChar(CP_UTF8, 0, content, -1, note->content, len);
        }
    } else {
        note->content = NULL;
    }

    const char* createdAt = (const char*)sqlite3_column_text(stmt, 3);
    MultiByteToWideChar(CP_UTF8, 0, createdAt ? createdAt : "", -1, note->createdAt, 32);

    const char* updatedAt = (const char*)sqlite3_column_text(stmt, 4);
    MultiByteToWideChar(CP_UTF8, 0, updatedAt ? updatedAt : "", -1, note->updatedAt, 32);

    note->isPinned = sqlite3_column_int(stmt, 5);
    note->isArchived = sqlite3_column_int(stmt, 6);
    note->categoryId = sqlite3_column_int(stmt, 7);

    sqlite3_finalize(stmt);
    return TRUE;
}

// Update a note
BOOL Notes_Update(int id, const WCHAR* title, const WCHAR* content) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE notes SET title = ?, content = ?, updated_at = datetime('now') WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    // Convert title to UTF-8
    char titleUtf8[MAX_TITLE_LEN * 3];
    WideCharToMultiByte(CP_UTF8, 0, title ? title : L"Untitled", -1,
                        titleUtf8, sizeof(titleUtf8), NULL, NULL);
    sqlite3_bind_text(stmt, 1, titleUtf8, -1, SQLITE_STATIC);

    // Convert content to UTF-8
    if (content && content[0]) {
        int contentLen = (int)wcslen(content);
        int bufSize = contentLen * 3 + 1;
        char* contentUtf8 = (char*)malloc(bufSize);
        if (contentUtf8) {
            WideCharToMultiByte(CP_UTF8, 0, content, -1, contentUtf8, bufSize, NULL, NULL);
            sqlite3_bind_text(stmt, 2, contentUtf8, -1, SQLITE_TRANSIENT);
            free(contentUtf8);
        } else {
            sqlite3_bind_text(stmt, 2, "", -1, SQLITE_STATIC);
        }
    } else {
        sqlite3_bind_text(stmt, 2, "", -1, SQLITE_STATIC);
    }

    sqlite3_bind_int(stmt, 3, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

// Delete a note
BOOL Notes_Delete(int id) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM notes WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

// Get note content
WCHAR* Notes_GetContent(int id) {
    sqlite3* db = Database_GetHandle();
    if (!db) return NULL;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT content FROM notes WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, id);

    WCHAR* result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* content = (const char*)sqlite3_column_text(stmt, 0);
        if (content) {
            int len = MultiByteToWideChar(CP_UTF8, 0, content, -1, NULL, 0);
            result = (WCHAR*)malloc(len * sizeof(WCHAR));
            if (result) {
                MultiByteToWideChar(CP_UTF8, 0, content, -1, result, len);
            }
        } else {
            result = _wcsdup(L"");
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

// Set note content
BOOL Notes_SetContent(int id, const WCHAR* content) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE notes SET content = ?, updated_at = datetime('now') WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    // Convert content to UTF-8
    if (content && content[0]) {
        int contentLen = (int)wcslen(content);
        int bufSize = contentLen * 3 + 1;
        char* contentUtf8 = (char*)malloc(bufSize);
        if (contentUtf8) {
            WideCharToMultiByte(CP_UTF8, 0, content, -1, contentUtf8, bufSize, NULL, NULL);
            sqlite3_bind_text(stmt, 1, contentUtf8, -1, SQLITE_TRANSIENT);
            free(contentUtf8);
        } else {
            sqlite3_bind_text(stmt, 1, "", -1, SQLITE_STATIC);
        }
    } else {
        sqlite3_bind_text(stmt, 1, "", -1, SQLITE_STATIC);
    }

    sqlite3_bind_int(stmt, 2, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

// Get note title
BOOL Notes_GetTitle(int id, WCHAR* buffer, int bufferSize) {
    if (!buffer || bufferSize <= 0) return FALSE;

    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT title FROM notes WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, id);

    BOOL success = FALSE;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* title = (const char*)sqlite3_column_text(stmt, 0);
        if (title) {
            MultiByteToWideChar(CP_UTF8, 0, title, -1, buffer, bufferSize);
            success = TRUE;
        }
    }

    sqlite3_finalize(stmt);
    return success;
}

// Set note title
BOOL Notes_SetTitle(int id, const WCHAR* title) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE notes SET title = ?, updated_at = datetime('now') WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    char titleUtf8[MAX_TITLE_LEN * 3];
    WideCharToMultiByte(CP_UTF8, 0, title ? title : L"Untitled", -1,
                        titleUtf8, sizeof(titleUtf8), NULL, NULL);
    sqlite3_bind_text(stmt, 1, titleUtf8, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

// Get list of all notes
int Notes_GetList(NoteListItem** items, int* count) {
    if (!items || !count) return -1;

    *items = NULL;
    *count = 0;

    sqlite3* db = Database_GetHandle();
    if (!db) return -1;

    // First, count notes
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM notes WHERE is_archived = 0", -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (total == 0) return 0;

    // Allocate array
    *items = (NoteListItem*)calloc(total, sizeof(NoteListItem));
    if (!*items) return -1;

    // Fetch notes
    const char* sql = "SELECT id, title, updated_at, is_pinned FROM notes "
                      "WHERE is_archived = 0 ORDER BY is_pinned DESC, updated_at DESC";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(*items);
        *items = NULL;
        return -1;
    }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < total) {
        (*items)[i].id = sqlite3_column_int(stmt, 0);

        const char* title = (const char*)sqlite3_column_text(stmt, 1);
        MultiByteToWideChar(CP_UTF8, 0, title ? title : "", -1, (*items)[i].title, MAX_TITLE_LEN);

        const char* updatedAt = (const char*)sqlite3_column_text(stmt, 2);
        MultiByteToWideChar(CP_UTF8, 0, updatedAt ? updatedAt : "", -1, (*items)[i].updatedAt, 32);

        (*items)[i].isPinned = sqlite3_column_int(stmt, 3);
        i++;
    }

    sqlite3_finalize(stmt);
    *count = i;

    return i;
}

// Search notes using FTS5
int Notes_Search(const WCHAR* query, NoteListItem** items, int* count) {
    if (!items || !count || !query) return -1;

    *items = NULL;
    *count = 0;

    sqlite3* db = Database_GetHandle();
    if (!db) return -1;

    // Convert query to UTF-8
    char queryUtf8[512];
    WideCharToMultiByte(CP_UTF8, 0, query, -1, queryUtf8, sizeof(queryUtf8), NULL, NULL);

    // Build FTS5 query
    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT n.id, n.title, n.updated_at, n.is_pinned FROM notes n "
             "INNER JOIN notes_fts f ON n.id = f.rowid "
             "WHERE notes_fts MATCH '%s*' AND n.is_archived = 0 "
             "ORDER BY n.is_pinned DESC, rank",
             queryUtf8);

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        // Fall back to LIKE search if FTS fails
        snprintf(sql, sizeof(sql),
                 "SELECT id, title, updated_at, is_pinned FROM notes "
                 "WHERE (title LIKE '%%%s%%' OR content LIKE '%%%s%%') AND is_archived = 0 "
                 "ORDER BY is_pinned DESC, updated_at DESC",
                 queryUtf8, queryUtf8);

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            return -1;
        }
    }

    // Count results first
    int capacity = 100;
    *items = (NoteListItem*)calloc(capacity, sizeof(NoteListItem));
    if (!*items) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < capacity) {
        (*items)[i].id = sqlite3_column_int(stmt, 0);

        const char* title = (const char*)sqlite3_column_text(stmt, 1);
        MultiByteToWideChar(CP_UTF8, 0, title ? title : "", -1, (*items)[i].title, MAX_TITLE_LEN);

        const char* updatedAt = (const char*)sqlite3_column_text(stmt, 2);
        MultiByteToWideChar(CP_UTF8, 0, updatedAt ? updatedAt : "", -1, (*items)[i].updatedAt, 32);

        (*items)[i].isPinned = sqlite3_column_int(stmt, 3);
        i++;
    }

    sqlite3_finalize(stmt);
    *count = i;

    return i;
}

// Set pinned status
BOOL Notes_SetPinned(int id, BOOL pinned) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE notes SET is_pinned = ? WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, pinned ? 1 : 0);
    sqlite3_bind_int(stmt, 2, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

// Set archived status
BOOL Notes_SetArchived(int id, BOOL archived) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE notes SET is_archived = ? WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, archived ? 1 : 0);
    sqlite3_bind_int(stmt, 2, id);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

// Free note structure
void Notes_FreeNote(Note* note) {
    if (note && note->content) {
        free(note->content);
        note->content = NULL;
    }
}

// Free note list
void Notes_FreeList(NoteListItem* items) {
    if (items) {
        free(items);
    }
}
