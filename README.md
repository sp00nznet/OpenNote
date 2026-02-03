# OpenNote

A modern, lightweight tabbed text editor for Windows with SQLite-based note storage. Built as a Notepad replacement with enhanced features for power users.

## Features

### Core Editing
- **Tabbed Interface** - Work with multiple documents simultaneously
- **Rich Text Editing** - Powered by Windows RichEdit control
- **Find & Replace** - Full-featured search with match case, whole word, and wrap-around options
- **Go To Line** - Quick navigation (Ctrl+G)
- **Word Wrap** - Toggle word wrap per preference
- **Zoom** - Adjustable zoom level (Ctrl+Plus/Minus, Ctrl+0 to reset)
- **Undo/Redo** - Multi-level undo history

### File Operations
- **Multiple Encodings** - Support for UTF-8, UTF-16 LE/BE, and ANSI
- **Recent Files** - Quick access to recently opened files
- **Print Support** - Basic printing with proper text wrapping
- **Drag & Drop** - Open files by dropping onto the window (planned)

### Notes Database
- **SQLite Storage** - All notes stored in a local SQLite database
- **Full-Text Search** - Fast search across all notes using FTS5
- **Notes Browser** - Visual interface to browse and manage notes
- **Import/Export** - Save files to notes, export notes to files
- **Pin & Archive** - Organize your notes (planned)

### Session Management
- **Auto-Save on Exit** - Optionally save your entire session (open tabs, content) when closing
- **Restore Session** - Restore previous session with all tabs and unsaved content
- **Settings Persistence** - All preferences saved to database

### Comparison
- **Side-by-Side Compare** - Right-click any tab to compare with another open document
- **Visual Diff** - View two documents simultaneously in a split view

### Keyboard Shortcuts
| Shortcut | Action |
|----------|--------|
| Ctrl+N | New file |
| Ctrl+O | Open file |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+W | Close tab |
| Ctrl+Tab | Next tab |
| Ctrl+Shift+Tab | Previous tab |
| Ctrl+F | Find |
| Ctrl+H | Replace |
| F3 | Find Next |
| Shift+F3 | Find Previous |
| Ctrl+G | Go To Line |
| Ctrl+A | Select All |
| F5 | Insert Date/Time |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+Plus | Zoom In |
| Ctrl+Minus | Zoom Out |
| Ctrl+0 | Reset Zoom |

### Context Menus
- **Tab Context Menu** - Save, Save As, Import to Notes, Compare, Close options
- **Empty Tab Area** - Right-click for New, Open, New Note, Open Note

## Building

### Requirements
- Windows 10/11
- Visual Studio 2022 (or compatible MSVC toolchain)
- CMake 3.16+

### Build Steps
```bash
git clone https://github.com/sp00nznet/OpenNote.git
cd OpenNote
cmake -B build
cmake --build build --config Release
```

The executable will be at `build/bin/OpenNote.exe`.

### Dependencies
- **SQLite** - Included as amalgamation (lib/sqlite/)
- **Windows SDK** - Common controls, common dialogs, RichEdit

## Configuration

Settings are stored in:
- Windows: `%APPDATA%\OpenNote\opennote.db`

### Settings Menu
- **Auto-save Session on Exit** - When enabled, saves all open tabs and their content
- **Restore Previous Session** - Manually restore the last saved session

## Project Structure

```
OpenNote/
├── CMakeLists.txt          # Build configuration
├── include/
│   └── supernote.h         # Main header
├── src/
│   ├── main.c              # Entry point
│   ├── app.c/h             # Application state
│   ├── ui/
│   │   ├── mainwindow.c/h  # Main window
│   │   ├── tabcontrol.c/h  # Tab management
│   │   ├── editor.c/h      # Editor wrapper
│   │   ├── menubar.c/h     # Menus
│   │   ├── statusbar.c/h   # Status bar
│   │   └── dialogs.c/h     # Dialogs
│   ├── core/
│   │   ├── document.c/h    # Document model
│   │   ├── fileio.c/h      # File I/O
│   │   └── search.c/h      # Search engine
│   └── db/
│       ├── database.c/h    # SQLite wrapper
│       └── notes_repo.c/h  # Notes CRUD
├── res/
│   ├── resource.h          # Resource IDs
│   ├── supernote.rc        # Resources
│   └── manifest.xml        # DPI/visual styles
└── lib/sqlite/
    ├── sqlite3.c           # SQLite amalgamation
    └── sqlite3.h
```

## Database Schema

```sql
-- Notes storage with full-text search
CREATE TABLE notes (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at TEXT,
    updated_at TEXT,
    is_pinned INTEGER,
    is_archived INTEGER
);

CREATE VIRTUAL TABLE notes_fts USING fts5(title, content);

-- Session persistence
CREATE TABLE session_tabs (
    tab_index INTEGER PRIMARY KEY,
    doc_type INTEGER,
    file_path TEXT,
    note_id INTEGER,
    title TEXT,
    content TEXT,
    is_modified INTEGER
);

-- Application settings
CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT);
```

## License

MIT License - see LICENSE file for details.

## Contributing

Contributions welcome! Please open an issue or pull request on GitHub.
