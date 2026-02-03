#ifndef STATUSBAR_H
#define STATUSBAR_H

// Status bar creation
HWND StatusBar_Create(HWND hParent);

// Status bar updates
void StatusBar_SetText(int part, const WCHAR* text);
void StatusBar_UpdatePosition(int line, int column);
void StatusBar_UpdateEncoding(TextEncoding encoding);
void StatusBar_UpdateModified(BOOL modified);
void StatusBar_SetMessage(const WCHAR* message);
void StatusBar_Clear(void);

// Resize handling
void StatusBar_Resize(HWND hParent);

// Show/hide
void StatusBar_Show(BOOL show);

#endif // STATUSBAR_H
