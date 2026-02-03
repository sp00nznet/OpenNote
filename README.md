# OpenNote

```
   ____                   _   _       _
  / __ \                 | \ | |     | |
 | |  | |_ __   ___ _ __ |  \| | ___ | |_ ___
 | |  | | '_ \ / _ \ '_ \| . ` |/ _ \| __/ _ \
 | |__| | |_) |  __/ | | | |\  | (_) | ||  __/
  \____/| .__/ \___|_| |_|_| \_|\___/ \__\___|
        | |
        |_|
```

[![Build Status](https://buildforever.cloud/sp00nz/OpenNote/badges/main/pipeline.svg)](https://buildforever.cloud/sp00nz/OpenNote/-/pipelines)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](https://github.com/sp00nznet/OpenNote)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![C](https://img.shields.io/badge/language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

> A modern, lightweight tabbed text editor for Windows with SQLite-based note storage. Built as a Notepad replacement with enhanced features for power users.

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| **Tabbed Interface** | Work with multiple documents simultaneously |
| **SQLite Notes** | Store notes in a local database with full-text search |
| **Session Restore** | Auto-save and restore your workspace |
| **Side-by-Side Compare** | Visual diff between any two open documents |
| **Print Preview** | Preview documents before printing |
| **System Tray** | Minimize to tray for quick access |
| **Shell Integration** | Run selected text in CMD or PowerShell |
| **Cross-Tab Search** | Find and replace across all open tabs |

### Core Editing
- Rich text editing powered by Scintilla
- Find & Replace with regex support
- Word wrap, zoom, and customizable fonts
- Multi-level undo/redo history

### Notes Database
- SQLite storage with FTS5 full-text search
- Notes browser with search and file size display
- Import files to notes, export notes to files
- Rename and organize your notes

### Keyboard Shortcuts

| Shortcut | Action | Shortcut | Action |
|----------|--------|----------|--------|
| `Ctrl+N` | New file | `Ctrl+F` | Find |
| `Ctrl+O` | Open file | `Ctrl+H` | Replace |
| `Ctrl+S` | Save | `Ctrl+G` | Go To Line |
| `Ctrl+W` | Close tab | `F3` | Find Next |
| `Ctrl+Tab` | Next tab | `Ctrl++` | Zoom In |
| `Ctrl+Shift+Tab` | Previous tab | `Ctrl+-` | Zoom Out |

---

## 🚀 Quick Start

```bash
# Clone the repository
git clone https://github.com/sp00nznet/OpenNote.git
cd OpenNote

# Build with CMake
cmake -B build
cmake --build build --config Release

# Run
./build/bin/OpenNote.exe
```

**Requirements:** Windows 10/11, Visual Studio 2022, CMake 3.16+

See [BUILDING.md](BUILDING.md) for detailed build instructions.

---

## 📁 Project Structure

```
OpenNote/
├── src/           # Source code
│   ├── ui/        # Window, tabs, dialogs
│   ├── core/      # Document, file I/O, search
│   └── db/        # SQLite, notes repository
├── res/           # Resources (icons, dialogs)
├── lib/           # Third-party (SQLite)
└── include/       # Headers
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed documentation.

---

## 🔧 Configuration

Settings are stored in `%APPDATA%\OpenNote\opennote.db`

- **Auto-save Session** - Save all tabs on exit
- **Minimize to Tray** - Keep running in system tray
- **Default Font Size** - Set preferred editor font size
- **Theme** - Light or dark mode

---

## 🤝 Contributing

Contributions welcome! Please open an issue or pull request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing`)
5. Open a Pull Request

---

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

---

<p align="center">
  Made with ❤️ for Windows power users
</p>
