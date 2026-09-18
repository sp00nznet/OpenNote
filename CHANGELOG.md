# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.5.0] - 2026-09-18

**The WordPad replacement.** Windows 11 24H2 removed WordPad; this is the milestone that
answers it. OpenNote now opens, edits, prints and saves rich text documents.

### Added
- **Rich text documents.** `.rtf` files open in a RichEdit 4.1 view alongside the existing
  plain text view, load and save through `EM_STREAMIN`/`EM_STREAMOUT`, and are offered in
  the Open and Save dialogs. **File > New Rich Text Document** starts an empty one.
- **Character formatting**: font family and size, text colour, bold, italic, underline,
  strikethrough, superscript and subscript.
- **Paragraph formatting**: left/centre/right/justified alignment, single, 1.5 and double
  line spacing, bulleted and numbered lists, and increase/decrease indent.
- **Formatting toolbar**, shown only while a rich text tab is active, with font and size
  combos that follow the caret. Its buttons are drawn rather than loaded from a bitmap, so
  they follow the system text colour.
- **Insert picture** (uncompressed bitmaps), embedded as RTF so the image survives being
  opened in other readers.
- **Page Setup**, and **printing that pages properly** — the rich view renders through
  `EM_FORMATRANGE` across as many sheets as the document needs, with margins measured from
  the paper edge rather than from wherever the driver starts.
- `Ctrl+B`, `Ctrl+I` and `Ctrl+U` accelerators. **Clear Formatting** on the Format menu.
- Self-check coverage for the rich view, run by CI: RTF round trips through both a string
  and a real file with bold, italic and font size intact; alignment and bullets round trip;
  replace-all terminates when the replacement contains the search term; word lookup
  reports correct bounds.

### Changed
- `Editor_*` now dispatches between the two views, so everything outside `editor.c` keeps
  calling the same functions with an `HWND` and does not know which control is behind it.
- Opening a document reuses the current tab only when that tab's view matches the
  document's format. Four copies of that logic became one function; previously an `.rtf`
  dropped onto a plain tab would have shown its markup.
- The editor font, word wrap and tab size settings apply to the plain text view only. A
  rich document carries its own fonts, and forcing the code font over it flattened the
  document. New rich documents start on Calibri 11, as WordPad did.

### Known issues
- The toolbar's font and size combos show their value highlighted until first clicked.
  Cosmetic only — the values are correct and editing works.
- The rich view always wraps to the window. Turning wrapping off needs a page width to
  wrap to instead, which arrives with pagination in v0.6.
- Tables are weak and the view flows rather than showing page boundaries. Both are
  RichEdit's limits, and both are lifted by the layout engine `.docx` requires — see
  `ROADMAP.md`.

## [Unreleased]

### Added
- `LICENSE` (MIT), `CHANGELOG.md`, `ROADMAP.md`, `SECURITY.md`, `CONTRIBUTING.md`.
- GitHub Actions pipeline (`.github/workflows/ci.yml`): builds on every push and pull
  request to `main`, publishes a GitHub Release with both the bare executable and the
  installer on a `v*` tag.
- Installer now registers OpenNote as an *additional* handler for `.txt`, `.log`, `.md`
  and `.ini` rather than only adding a generic "Open with" verb, and no longer seizes
  the default association for any extension.

### Changed
- Releases are built by CI and published on GitHub Releases. Downloads no longer come
  from a personal host. No binaries will be published until v0.5.0; the pipeline only
  fires on a `v*` tag and none will be cut before then.
- Installer is per-user by default and no longer requires administrator rights; an
  all-users install is still available from the elevation prompt.
- Installer version is supplied by the build rather than hardcoded.
- Google Drive sync now needs a client ID and secret from the user's own Google Cloud
  project, entered in Settings. Google issues no credential that a public client can
  safely ship, so the alternative was shipping one anyway.

### Changed
- Every editing operation now goes through the `Editor_*` API. `mainwindow.c` no longer
  sends Scintilla messages directly; `Editor_HasIndicatorAt`, `Editor_ReplaceRange`,
  `Editor_ClearSpellIndicatorRange` and `Editor_GetWordAt` cover what it was reaching for.
  The only Scintilla-specific code left outside `editor.c` is the notification dispatch.

### Fixed
- Startup no longer fails when `Msftedit.dll` is unavailable. The RichEdit control has
  been unused since the Scintilla port; the library was still being loaded, and a load
  failure aborted the application for no reason.

### Removed
- GitLab CI configuration, which carried an internal host address and share credentials
  in its deploy stage.

### Security
- **OAuth client secrets are no longer compiled into the binary.** GitHub now uses the
  device authorization grant (RFC 8628), which needs only a public client ID. Google uses
  PKCE (RFC 7636) with the user's own Google Cloud credentials, stored encrypted. The
  build now fails outright if `GH_OAUTH_CLIENT_SECRET` or `GOOGLE_CLIENT_SECRET` is
  passed, so it cannot regress. CI consequently needs no secrets, and a published binary
  is reproducible from its tagged commit.
- **Access tokens are wrapped with DPAPI** (`CryptProtectData`) before reaching the
  database, tying them to the Windows account. Anyone who connected an account before
  v0.2.0 should revoke that token and reconnect — the old value was written in cleartext.
- **Settings access binds its parameters.** `Database_SetSetting` / `Database_GetSetting`
  replace the formatted `INSERT OR REPLACE` the token path used.
- `OpenNote.exe --selftest` checks the PKCE S256 challenge against the RFC 7636 test
  vector, the DPAPI round trip, and the JSON field reader. CI runs it on every build.
