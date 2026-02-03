#ifndef GOOGLE_SYNC_H
#define GOOGLE_SYNC_H

#include <windows.h>

// Sync a single note to Google Drive
// Returns TRUE on success, FALSE on failure
BOOL GoogleSync_UploadNote(int noteId, const WCHAR* title, const WCHAR* content);

// Download all notes from Google Drive
// Returns number of notes imported
int GoogleSync_DownloadAll(HWND hParent);

// Sync all notes (upload local changes, download remote changes)
BOOL GoogleSync_SyncAll(HWND hParent);

// Delete a file from Google Drive associated with a note
BOOL GoogleSync_DeleteFile(int noteId);

#endif // GOOGLE_SYNC_H
