#ifndef GITHUB_SYNC_H
#define GITHUB_SYNC_H

#include <windows.h>

// Sync a single note to GitHub Gist
// Returns TRUE on success, FALSE on failure
BOOL GitHubSync_UploadNote(int noteId, const WCHAR* title, const WCHAR* content);

// Download all gists and import as notes
// Returns number of notes imported
int GitHubSync_DownloadAll(HWND hParent);

// Sync all notes (upload local changes, download remote changes)
BOOL GitHubSync_SyncAll(HWND hParent);

// Delete a gist associated with a note
BOOL GitHubSync_DeleteGist(int noteId);

#endif // GITHUB_SYNC_H
