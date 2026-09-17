# Security

OpenNote stores your notes, your editor session and — if you connect a sync provider —
an OAuth access token, in a SQLite database at `%APPDATA%\OpenNote\opennote.db`.

That file is as sensitive as the notes you put in it. This document says plainly what
protects it today and what does not.

## Reporting a vulnerability

Open a GitHub issue at <https://github.com/sp00nznet/opennote/issues>. If the issue
would put existing users at risk before a fix ships, use GitHub's **private vulnerability
reporting** on the repository's Security tab instead.

There is no bounty. Expect a reply within a week.

## Known issues, and what is being done about them

These are real, they are in the shipped code today, and they are the reason v0.2.0 exists.

### 1. OAuth client secrets are compiled into the executable

`CMakeLists.txt` accepts `GH_OAUTH_CLIENT_SECRET` and `GOOGLE_CLIENT_SECRET` and bakes
them into the binary as preprocessor defines. A secret inside a program distributed to
users is not a secret — `strings OpenNote.exe` recovers it.

**Fix in progress:** GitHub's [device authorization flow][device], which is designed for
clients that cannot hold a secret, and [PKCE][pkce] for Google. Neither needs a client
secret, which also means CI needs no credentials and anyone can reproduce the exact
binary that is published.

**Until then:** a build made without those CMake variables has no secret in it and simply
does not offer cloud sync. That is the recommended way to build from source.

### 2. Access tokens are stored in cleartext

`OAuth_SaveToken` writes the token into the `settings` table as plain text. Anything that
can read `%APPDATA%\OpenNote\opennote.db` — any process running as you, any backup, any
sync tool that happens to pick that directory up — can read the token and use it against
your GitHub or Google account.

**Fix in progress:** `CryptProtectData` (DPAPI), which ties the stored blob to your
Windows account.

**Until then:** treat a connected account as a credential stored on disk. Revoking it is
one click in your provider's settings, and worth doing if the machine is shared.

### 3. The token write path builds SQL by string formatting

The same function formats the token directly into an `INSERT OR REPLACE` statement rather
than binding it as a parameter. The value is one OpenNote itself received from the
provider, so this is not currently a path an attacker controls — but it is the wrong
construction and it is being replaced with a bound parameter.

### 4. Notes are not encrypted at rest

The notes database is an ordinary SQLite file. Anyone with the file has the notes.

This is not a bug, it is the current design, and it is stated here so nobody assumes
otherwise. Encryption at rest is scheduled for v0.3.0 — see `ROADMAP.md`.

## What OpenNote does not do

- No telemetry, no analytics, no crash reporting, no update check. The application makes
  no network request at all unless you connect a sync provider.
- No account. There is nothing to sign up for.
- Sync talks to GitHub or Google directly. There is no server in between, and none is
  operated by this project.

## Verifying what you run

Released binaries are built by GitHub Actions from a tagged commit — see
`.github/workflows/ci.yml`. The build log for any release is public and shows exactly
which commit produced it.

The executables are **not** code-signed. A certificate costs money annually and this
project does not take money, so SmartScreen will warn on first run. Building from source
takes one command and avoids the question entirely.

[device]: https://docs.github.com/apps/oauth-apps/building-oauth-apps/authorizing-oauth-apps#device-flow
[pkce]: https://developers.google.com/identity/protocols/oauth2/native-app
