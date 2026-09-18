# Roadmap

Where this is going, in order, and what is deliberately not being built.

The short version: **Windows shipped a rich text editor for thirty years and removed it
in 2024.** WordPad is gone from Windows 11 24H2 and from Windows Server 2025. The
replacement Microsoft points you at is a subscription. OpenNote is aiming at that hole.

**v0.6.0 is released.** Published binaries — a bare executable and an installer — are
built by CI from the tagged commit.

Nothing below is a promise of a date.

---

## v0.2.0 — Compliance and security

Housekeeping and the three security problems described in `SECURITY.md`. No user-visible
features.

- [x] `LICENSE`, `CHANGELOG.md`, `ROADMAP.md`, `SECURITY.md`, `CONTRIBUTING.md`
- [x] GitHub Actions replacing GitLab CI; releases built by CI, not a workstation, and
      held unpublished until v0.5.0
- [x] Remove personal-host references from the README and the installer
- [x] Remove the dead `Msftedit.dll` load that could abort startup for nothing
- [x] GitHub device flow and Google PKCE — no client secret in the shipped binary
- [x] DPAPI for token storage; bound parameters on all settings access
- [ ] Rename the repository to `opennote`
- [x] Route every editing operation through the `Editor_*` API (the Scintilla
      coupling turned out to be far thinner than feared — see below)

On that last item: the coupling was much thinner than a first look suggested. Scintilla
was never spread across `mainwindow.c`, `tabcontrol.c` and `editor.c` — it was 282 uses
inside `editor.c` and eleven leaks in `mainwindow.c`, with everything else already talking
through `Editor_*(HWND, ...)`, which is view-agnostic as it stands. The eleven leaks are
now closed, so the only Scintilla-specific code left outside `editor.c` is the
`SCNotification` dispatch — the control's notification protocol rather than an editing
operation, and the single place that needs a branch when a second view lands.

No dispatch layer has been built, deliberately. There is one view type; a factory for one
product is scaffolding. The seam is where it needs to be and the rest waits for v0.5.

## v0.3.0 — Notes vault

The notes half of OpenNote aimed squarely at what Evernote (~$130/yr, free tier capped at
50 notes), Obsidian Sync ($4–8/mo for sync and version history alone) and Standard Notes
(~$90/yr, where encryption *is* the paid tier) charge for.

- [ ] Encryption at rest — AES-GCM via CNG, key derived from a passphrase
- [ ] Version history for every note, in the same SQLite file
- [ ] Conflict-safe sync — content hashing and three-way merge, not last-writer-wins
- [ ] Export to a plain Markdown folder in one action, so the data is never hostage
- [ ] Harness: kill the process mid-sync across a corpus, assert zero note loss

## v0.4.0 — Editor tier

The part UltraEdit (~$80/yr), EmEditor ($40–80/yr) and Beyond Compare ($35–70) sell.

- [ ] Open multi-gigabyte files — memory-mapped, chunked, lazy line index
- [ ] Column/block selection and multiple carets
- [ ] Three-way diff and folder comparison
- [ ] Regex find-in-files across a directory tree
- [ ] Harness: fixed file corpus with open time and peak working set tracked in CI, and
      the current figures in the README

## v0.5.0 — The WordPad replacement — **done**

RTF is WordPad's native format, its specification (RTF 1.9.1) is published, and Windows'
in-box RichEdit control reads and writes it — which is essentially how WordPad itself
worked.

- [x] Rich text document type alongside the plain text one
- [x] RTF round-trip: character and paragraph formatting, lists, images
- [x] Formatting toolbar
- [x] Print and page setup, paging across as many sheets as the document needs
- [ ] Ruler and tab stops — deferred, they want the pagination v0.6 brings
- [ ] Spell check in the rich view — the `ISpellChecker` plumbing is shared, but the
      squiggle indicators are Scintilla's; the rich view needs its own

Built on RichEdit deliberately, to get a working replacement out while the removal is
still recent. RichEdit's ceiling is real — weak tables, no true pagination, and word wrap
that cannot be turned off without a page width to wrap to — and the upgrade path is the
DirectWrite engine in v0.6.0.

## v0.6.0 — `.docx` — **done**

This version originally bundled `.docx`, a layout engine and `.doc` together. That was
three separate efforts in one entry, so they are now three versions.

- [x] `.docx` reading — ECMA-376, via `IOpcFactory` and `IXmlReader`
- [x] `.docx` writing, with the Save As file type choosing the storage format
- [x] Conformance harness with a pass/fail count, run in CI, figure in the README
- [x] Corpus generated rather than committed
- [ ] `styles.xml` resolution, `numbering.xml` list markers, images

## v0.7.0 — The layout engine

- [ ] DirectWrite layout engine — pagination, floats, text wrap, proper tables
- [ ] Real table support on write, which needs that table model
- [ ] Ruler and tab stops
- [ ] Print-to-PDF export via the in-box PDF printer

## v0.8.0 and beyond — `.doc`, and the rest of Word

- [ ] `.doc` read and write — [MS-DOC] over [MS-CFB]. Twenty-five years of files that
      nothing free reads well, and the last in-box reader left with WordPad
- [ ] Track changes, comments, footnotes, table of contents

Everything needed for the layout is already in Windows and already paid for: DirectWrite
for shaping, line breaking, justification and font fallback; `ISpellChecker` for spelling;
Microsoft Print to PDF for export.

The honest risk is fidelity. LibreOffice has worked on `.docx` for twenty years and still
mangles complex documents. The discipline is scope: be excellent on the documents people
actually exchange — letters, reports, resumes, contracts — and state plainly what is not
proven rather than letting a feature list imply it.

---

## Deferred

- Code signing. A certificate is an annual fee and this project does not take money.
- Plugin API. Not until the core is stable enough that an API would not be rewritten.
- Macros and scripting.

## Out of scope

- **Being a code editor.** VS Code and Notepad++ are free, excellent and enormously
  further along. Syntax highlighting exists here because Scintilla provides it, not
  because this competes.
- **A hosted sync service.** Sync talks to your GitHub or Google account directly. No
  server exists and none will.
- Mobile, web, and non-Windows builds.
- Mail merge, equation editing, and the rest of the Word long tail.
