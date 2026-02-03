#ifndef MENUBAR_H
#define MENUBAR_H

// Menu creation
HMENU MenuBar_Create(void);

// Menu updates
void MenuBar_UpdateFileMenu(HMENU hMenu);
void MenuBar_UpdateEditMenu(HMENU hMenu);
void MenuBar_UpdateFormatMenu(HMENU hMenu);
void MenuBar_UpdateViewMenu(HMENU hMenu);
void MenuBar_UpdateSettingsMenu(HMENU hMenu);

// Recent files submenu
void MenuBar_UpdateRecentFiles(HMENU hMenu);

#endif // MENUBAR_H
