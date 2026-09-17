# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

### Fixed
- Startup no longer fails when `Msftedit.dll` is unavailable. The RichEdit control has
  been unused since the Scintilla port; the library was still being loaded, and a load
  failure aborted the application for no reason.

### Removed
- GitLab CI configuration, which carried an internal host address and share credentials
  in its deploy stage.

### Security
- See `SECURITY.md` for the known issues being addressed in v0.2.0: OAuth client secrets
  embedded in the shipped binary, cleartext token storage, and unparameterised SQL on the
  token write path.
