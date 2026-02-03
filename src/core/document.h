#ifndef DOCUMENT_H
#define DOCUMENT_H

// Document structure
struct Document {
    DocumentType type;
    TextEncoding encoding;

    // File-based document
    WCHAR filePath[MAX_PATH];

    // Note-based document
    int noteId;
    WCHAR noteTitle[MAX_TITLE_LEN];

    // State
    BOOL modified;
    BOOL isNew;  // Never saved

    // Display title
    WCHAR title[MAX_TITLE_LEN];
};

// Document creation/destruction
Document* Document_Create(void);
Document* Document_CreateFromFile(const WCHAR* path);
Document* Document_CreateFromNote(int noteId);
void Document_Destroy(Document* doc);

// Document operations
BOOL Document_Save(Document* doc, HWND hEditor);
BOOL Document_SaveAs(Document* doc, HWND hEditor, const WCHAR* path);
BOOL Document_Load(Document* doc, HWND hEditor);

// Properties
const WCHAR* Document_GetTitle(Document* doc);
void Document_UpdateTitle(Document* doc);
BOOL Document_IsModified(Document* doc);
void Document_SetModified(Document* doc, BOOL modified);

// Type checking
BOOL Document_IsFile(Document* doc);
BOOL Document_IsNote(Document* doc);
BOOL Document_IsNew(Document* doc);

#endif // DOCUMENT_H
