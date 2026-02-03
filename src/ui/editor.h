#ifndef EDITOR_H
#define EDITOR_H

// Editor control creation
HWND Editor_Create(HWND hParent);

// Text operations
void Editor_SetText(HWND hEditor, const WCHAR* text);
WCHAR* Editor_GetText(HWND hEditor);  // Caller must free
int Editor_GetTextLength(HWND hEditor);

// Selection
void Editor_GetSelection(HWND hEditor, int* start, int* end);
void Editor_SetSelection(HWND hEditor, int start, int end);
void Editor_ReplaceSelection(HWND hEditor, const WCHAR* text);
WCHAR* Editor_GetSelectedText(HWND hEditor);  // Caller must free

// Cursor position
void Editor_GetCursorPos(HWND hEditor, int* line, int* column);
void Editor_GotoLine(HWND hEditor, int line);
int Editor_GetLineCount(HWND hEditor);
int Editor_GetCurrentLine(HWND hEditor);

// Undo/Redo
BOOL Editor_CanUndo(HWND hEditor);
BOOL Editor_CanRedo(HWND hEditor);
void Editor_Undo(HWND hEditor);
void Editor_Redo(HWND hEditor);

// Clipboard
void Editor_Cut(HWND hEditor);
void Editor_Copy(HWND hEditor);
void Editor_Paste(HWND hEditor);

// Formatting
void Editor_SetWordWrap(HWND hEditor, BOOL wrap);
void Editor_SetFont(HWND hEditor, HFONT hFont);
void Editor_SetTabSize(HWND hEditor, int tabSize);
void Editor_SetZoom(HWND hEditor, int zoomPercent);

// Modification state
BOOL Editor_GetModified(HWND hEditor);
void Editor_SetModified(HWND hEditor, BOOL modified);

// Find and replace
int Editor_FindText(HWND hEditor, const WCHAR* text, BOOL matchCase, BOOL wholeWord, BOOL forward);
int Editor_ReplaceText(HWND hEditor, const WCHAR* findText, const WCHAR* replaceText, BOOL matchCase, BOOL wholeWord);
int Editor_ReplaceAll(HWND hEditor, const WCHAR* findText, const WCHAR* replaceText, BOOL matchCase, BOOL wholeWord);

// Insert time/date
void Editor_InsertTimeDate(HWND hEditor);

// Syntax highlighting
void Editor_SetLexerFromExtension(HWND hEditor, const WCHAR* filename);

// Theme support
int Editor_GetThemeCount(void);
const WCHAR* Editor_GetThemeName(int index);
void Editor_ApplyTheme(HWND hEditor, const WCHAR* filename);

// Link indicators
void Editor_SetupLinkIndicator(HWND hEditor);
void Editor_AddLinkIndicator(HWND hEditor, int start, int end);
void Editor_ClearLinkIndicators(HWND hEditor);
void Editor_RefreshLinks(HWND hEditor, Document* doc);
int Editor_GetPositionFromPoint(HWND hEditor, int x, int y);

// Spell check
void Editor_InitSpellCheck(void);
void Editor_ShutdownSpellCheck(void);
void Editor_CheckSpelling(HWND hEditor);
void Editor_AddSpellIndicator(HWND hEditor, int start, int end);
void Editor_ClearSpellIndicators(HWND hEditor);
WCHAR** Editor_GetSpellSuggestions(const WCHAR* word, int* count);
void Editor_FreeSpellSuggestions(WCHAR** suggestions, int count);
BOOL Editor_AddWordToDictionary(const WCHAR* word);

#endif // EDITOR_H
