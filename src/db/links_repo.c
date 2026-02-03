#include "supernote.h"

// Create a new link
int Links_Create(int sourceType, const WCHAR* sourcePath, int sourceNoteId,
                 const WCHAR* linkText, int startPos, int endPos,
                 int targetType, const WCHAR* targetPath, int targetNoteId) {
    sqlite3* db = Database_GetHandle();
    if (!db) return -1;

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO links (source_type, source_path, source_note_id, "
                      "link_text, start_pos, end_pos, target_type, target_path, target_note_id) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    // Convert strings to UTF-8
    char sourcePathUtf8[MAX_PATH * 3] = {0};
    char targetPathUtf8[MAX_PATH * 3] = {0};
    char linkTextUtf8[MAX_TITLE_LEN * 3] = {0};

    if (sourcePath && sourcePath[0]) {
        WideCharToMultiByte(CP_UTF8, 0, sourcePath, -1, sourcePathUtf8, sizeof(sourcePathUtf8), NULL, NULL);
    }
    if (targetPath && targetPath[0]) {
        WideCharToMultiByte(CP_UTF8, 0, targetPath, -1, targetPathUtf8, sizeof(targetPathUtf8), NULL, NULL);
    }
    if (linkText && linkText[0]) {
        WideCharToMultiByte(CP_UTF8, 0, linkText, -1, linkTextUtf8, sizeof(linkTextUtf8), NULL, NULL);
    }

    sqlite3_bind_int(stmt, 1, sourceType);
    sqlite3_bind_text(stmt, 2, sourcePathUtf8[0] ? sourcePathUtf8 : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, sourceNoteId);
    sqlite3_bind_text(stmt, 4, linkTextUtf8, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, startPos);
    sqlite3_bind_int(stmt, 6, endPos);
    sqlite3_bind_int(stmt, 7, targetType);
    sqlite3_bind_text(stmt, 8, targetPathUtf8[0] ? targetPathUtf8 : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 9, targetNoteId);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result = (int)sqlite3_last_insert_rowid(db);
    }

    sqlite3_finalize(stmt);
    return result;
}

// Create a new link to URL
int Links_CreateURL(int sourceType, const WCHAR* sourcePath, int sourceNoteId,
                    const WCHAR* linkText, int startPos, int endPos,
                    const WCHAR* targetURL) {
    sqlite3* db = Database_GetHandle();
    if (!db) return -1;

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO links (source_type, source_path, source_note_id, "
                      "link_text, start_pos, end_pos, target_type, target_url) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    // Convert strings to UTF-8
    char sourcePathUtf8[MAX_PATH * 3] = {0};
    char targetURLUtf8[1024 * 3] = {0};
    char linkTextUtf8[MAX_TITLE_LEN * 3] = {0};

    if (sourcePath && sourcePath[0]) {
        WideCharToMultiByte(CP_UTF8, 0, sourcePath, -1, sourcePathUtf8, sizeof(sourcePathUtf8), NULL, NULL);
    }
    if (targetURL && targetURL[0]) {
        WideCharToMultiByte(CP_UTF8, 0, targetURL, -1, targetURLUtf8, sizeof(targetURLUtf8), NULL, NULL);
    }
    if (linkText && linkText[0]) {
        WideCharToMultiByte(CP_UTF8, 0, linkText, -1, linkTextUtf8, sizeof(linkTextUtf8), NULL, NULL);
    }

    sqlite3_bind_int(stmt, 1, sourceType);
    sqlite3_bind_text(stmt, 2, sourcePathUtf8[0] ? sourcePathUtf8 : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, sourceNoteId);
    sqlite3_bind_text(stmt, 4, linkTextUtf8, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, startPos);
    sqlite3_bind_int(stmt, 6, endPos);
    sqlite3_bind_int(stmt, 7, LINK_TARGET_URL);
    sqlite3_bind_text(stmt, 8, targetURLUtf8, -1, SQLITE_STATIC);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result = (int)sqlite3_last_insert_rowid(db);
    }

    sqlite3_finalize(stmt);
    return result;
}

// Delete a link
BOOL Links_Delete(int linkId) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM links WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, linkId);

    BOOL success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

// Delete all links for a source document
BOOL Links_DeleteForSource(int sourceType, const WCHAR* sourcePath, int sourceNoteId) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    sqlite3_stmt* stmt;
    const char* sql;

    if (sourceType == DOC_TYPE_FILE) {
        sql = "DELETE FROM links WHERE source_type = 0 AND source_path = ?";
    } else {
        sql = "DELETE FROM links WHERE source_type = 1 AND source_note_id = ?";
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    if (sourceType == DOC_TYPE_FILE) {
        char pathUtf8[MAX_PATH * 3];
        WideCharToMultiByte(CP_UTF8, 0, sourcePath, -1, pathUtf8, sizeof(pathUtf8), NULL, NULL);
        sqlite3_bind_text(stmt, 1, pathUtf8, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_int(stmt, 1, sourceNoteId);
    }

    BOOL success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

// Get all links for a source document
LinkArray* Links_GetForSource(int sourceType, const WCHAR* sourcePath, int sourceNoteId) {
    sqlite3* db = Database_GetHandle();
    if (!db) return NULL;

    LinkArray* array = (LinkArray*)calloc(1, sizeof(LinkArray));
    if (!array) return NULL;

    array->capacity = 16;
    array->items = (Link*)calloc(array->capacity, sizeof(Link));
    if (!array->items) {
        free(array);
        return NULL;
    }

    sqlite3_stmt* stmt;
    const char* sql;
    BOOL hasPath = (sourcePath && sourcePath[0]);

    if (sourceType == DOC_TYPE_FILE) {
        if (hasPath) {
            sql = "SELECT id, source_type, source_path, source_note_id, link_text, "
                  "start_pos, end_pos, target_type, target_path, target_note_id, target_url "
                  "FROM links WHERE source_type = 0 AND source_path = ?";
        } else {
            sql = "SELECT id, source_type, source_path, source_note_id, link_text, "
                  "start_pos, end_pos, target_type, target_path, target_note_id, target_url "
                  "FROM links WHERE source_type = 0 AND source_path IS NULL";
        }
    } else {
        sql = "SELECT id, source_type, source_path, source_note_id, link_text, "
              "start_pos, end_pos, target_type, target_path, target_note_id, target_url "
              "FROM links WHERE source_type = 1 AND source_note_id = ?";
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(array->items);
        free(array);
        return NULL;
    }

    if (sourceType == DOC_TYPE_FILE && hasPath) {
        char pathUtf8[MAX_PATH * 3];
        WideCharToMultiByte(CP_UTF8, 0, sourcePath, -1, pathUtf8, sizeof(pathUtf8), NULL, NULL);
        sqlite3_bind_text(stmt, 1, pathUtf8, -1, SQLITE_STATIC);
    } else if (sourceType == DOC_TYPE_NOTE) {
        sqlite3_bind_int(stmt, 1, sourceNoteId);
    }
    // No binding needed for IS NULL case

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Grow array if needed
        if (array->count >= array->capacity) {
            array->capacity *= 2;
            Link* newItems = (Link*)realloc(array->items, array->capacity * sizeof(Link));
            if (!newItems) break;
            array->items = newItems;
        }

        Link* link = &array->items[array->count];
        memset(link, 0, sizeof(Link));

        link->id = sqlite3_column_int(stmt, 0);
        link->sourceType = sqlite3_column_int(stmt, 1);

        const char* path = (const char*)sqlite3_column_text(stmt, 2);
        if (path) {
            MultiByteToWideChar(CP_UTF8, 0, path, -1, link->sourcePath, MAX_PATH);
        }

        link->sourceNoteId = sqlite3_column_int(stmt, 3);

        const char* text = (const char*)sqlite3_column_text(stmt, 4);
        if (text) {
            MultiByteToWideChar(CP_UTF8, 0, text, -1, link->linkText, MAX_TITLE_LEN);
        }

        link->startPos = sqlite3_column_int(stmt, 5);
        link->endPos = sqlite3_column_int(stmt, 6);
        link->targetType = sqlite3_column_int(stmt, 7);

        const char* targetPath = (const char*)sqlite3_column_text(stmt, 8);
        if (targetPath) {
            MultiByteToWideChar(CP_UTF8, 0, targetPath, -1, link->targetPath, MAX_PATH);
        }

        link->targetNoteId = sqlite3_column_int(stmt, 9);

        const char* targetUrl = (const char*)sqlite3_column_text(stmt, 10);
        if (targetUrl) {
            MultiByteToWideChar(CP_UTF8, 0, targetUrl, -1, link->targetURL, 1024);
        }

        array->count++;
    }

    sqlite3_finalize(stmt);
    return array;
}

// Get link at specific position in source document
Link* Links_GetAtPosition(int sourceType, const WCHAR* sourcePath, int sourceNoteId, int position) {
    sqlite3* db = Database_GetHandle();
    if (!db) return NULL;

    sqlite3_stmt* stmt;
    const char* sql;
    BOOL hasPath = (sourcePath && sourcePath[0]);

    if (sourceType == DOC_TYPE_FILE) {
        if (hasPath) {
            sql = "SELECT id, source_type, source_path, source_note_id, link_text, "
                  "start_pos, end_pos, target_type, target_path, target_note_id, target_url "
                  "FROM links WHERE source_type = 0 AND source_path = ? "
                  "AND start_pos <= ? AND end_pos > ?";
        } else {
            // Handle unsaved documents with NULL path
            sql = "SELECT id, source_type, source_path, source_note_id, link_text, "
                  "start_pos, end_pos, target_type, target_path, target_note_id, target_url "
                  "FROM links WHERE source_type = 0 AND source_path IS NULL "
                  "AND start_pos <= ? AND end_pos > ?";
        }
    } else {
        sql = "SELECT id, source_type, source_path, source_note_id, link_text, "
              "start_pos, end_pos, target_type, target_path, target_note_id, target_url "
              "FROM links WHERE source_type = 1 AND source_note_id = ? "
              "AND start_pos <= ? AND end_pos > ?";
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }

    if (sourceType == DOC_TYPE_FILE) {
        if (hasPath) {
            char pathUtf8[MAX_PATH * 3];
            WideCharToMultiByte(CP_UTF8, 0, sourcePath, -1, pathUtf8, sizeof(pathUtf8), NULL, NULL);
            sqlite3_bind_text(stmt, 1, pathUtf8, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, position);
            sqlite3_bind_int(stmt, 3, position);
        } else {
            // No path parameter needed for IS NULL query
            sqlite3_bind_int(stmt, 1, position);
            sqlite3_bind_int(stmt, 2, position);
        }
    } else {
        sqlite3_bind_int(stmt, 1, sourceNoteId);
        sqlite3_bind_int(stmt, 2, position);
        sqlite3_bind_int(stmt, 3, position);
    }

    Link* link = NULL;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        link = (Link*)calloc(1, sizeof(Link));
        if (link) {
            link->id = sqlite3_column_int(stmt, 0);
            link->sourceType = sqlite3_column_int(stmt, 1);

            const char* path = (const char*)sqlite3_column_text(stmt, 2);
            if (path) {
                MultiByteToWideChar(CP_UTF8, 0, path, -1, link->sourcePath, MAX_PATH);
            }

            link->sourceNoteId = sqlite3_column_int(stmt, 3);

            const char* text = (const char*)sqlite3_column_text(stmt, 4);
            if (text) {
                MultiByteToWideChar(CP_UTF8, 0, text, -1, link->linkText, MAX_TITLE_LEN);
            }

            link->startPos = sqlite3_column_int(stmt, 5);
            link->endPos = sqlite3_column_int(stmt, 6);
            link->targetType = sqlite3_column_int(stmt, 7);

            const char* targetPath = (const char*)sqlite3_column_text(stmt, 8);
            if (targetPath) {
                MultiByteToWideChar(CP_UTF8, 0, targetPath, -1, link->targetPath, MAX_PATH);
            }

            link->targetNoteId = sqlite3_column_int(stmt, 9);

            const char* targetUrl = (const char*)sqlite3_column_text(stmt, 10);
            if (targetUrl) {
                MultiByteToWideChar(CP_UTF8, 0, targetUrl, -1, link->targetURL, 1024);
            }
        }
    }

    sqlite3_finalize(stmt);
    return link;
}

// Update link positions after text edit
BOOL Links_UpdatePositions(int sourceType, const WCHAR* sourcePath, int sourceNoteId,
                          int editPos, int deltaLen) {
    sqlite3* db = Database_GetHandle();
    if (!db) return FALSE;

    // Delete links that are within the edited range (if deleting)
    if (deltaLen < 0) {
        sqlite3_stmt* delStmt;
        const char* delSql;

        if (sourceType == DOC_TYPE_FILE) {
            delSql = "DELETE FROM links WHERE source_type = 0 AND source_path = ? "
                     "AND start_pos >= ? AND start_pos < ?";
        } else {
            delSql = "DELETE FROM links WHERE source_type = 1 AND source_note_id = ? "
                     "AND start_pos >= ? AND start_pos < ?";
        }

        if (sqlite3_prepare_v2(db, delSql, -1, &delStmt, NULL) == SQLITE_OK) {
            if (sourceType == DOC_TYPE_FILE) {
                char pathUtf8[MAX_PATH * 3];
                WideCharToMultiByte(CP_UTF8, 0, sourcePath, -1, pathUtf8, sizeof(pathUtf8), NULL, NULL);
                sqlite3_bind_text(delStmt, 1, pathUtf8, -1, SQLITE_STATIC);
            } else {
                sqlite3_bind_int(delStmt, 1, sourceNoteId);
            }
            sqlite3_bind_int(delStmt, 2, editPos);
            sqlite3_bind_int(delStmt, 3, editPos - deltaLen);
            sqlite3_step(delStmt);
            sqlite3_finalize(delStmt);
        }
    }

    // Update positions for links after the edit point
    sqlite3_stmt* stmt;
    const char* sql;

    if (sourceType == DOC_TYPE_FILE) {
        sql = "UPDATE links SET start_pos = start_pos + ?, end_pos = end_pos + ? "
              "WHERE source_type = 0 AND source_path = ? AND start_pos >= ?";
    } else {
        sql = "UPDATE links SET start_pos = start_pos + ?, end_pos = end_pos + ? "
              "WHERE source_type = 1 AND source_note_id = ? AND start_pos >= ?";
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return FALSE;
    }

    sqlite3_bind_int(stmt, 1, deltaLen);
    sqlite3_bind_int(stmt, 2, deltaLen);

    if (sourceType == DOC_TYPE_FILE) {
        char pathUtf8[MAX_PATH * 3];
        WideCharToMultiByte(CP_UTF8, 0, sourcePath, -1, pathUtf8, sizeof(pathUtf8), NULL, NULL);
        sqlite3_bind_text(stmt, 3, pathUtf8, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, editPos);
    } else {
        sqlite3_bind_int(stmt, 3, sourceNoteId);
        sqlite3_bind_int(stmt, 4, editPos);
    }

    BOOL success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

// Free link array
void Links_FreeArray(LinkArray* array) {
    if (array) {
        free(array->items);
        free(array);
    }
}

// Free single link
void Links_Free(Link* link) {
    free(link);
}
