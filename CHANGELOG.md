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
- Google Drive sync now needs a client ID and secret from the user's own Google Cloud
  project, entered in Settings. Google issues no credential that a public client can
  safely ship, so the alternative was shipping one anyway.

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
