# Cloud Sync Setup

To enable cloud sync, you need to configure API keys.

## GitHub Sync

1. Create a GitHub OAuth App at https://github.com/settings/developers
2. Set the callback URL to `http://localhost:8547/callback`
3. Add your credentials to the build (see CI/CD variables below)

## Google Drive Sync

1. Create a Google Cloud project at https://console.cloud.google.com
2. Enable the Google Drive API
3. Create OAuth 2.0 credentials (Web Application type)
4. Add `http://localhost:8547/callback` to Authorized redirect URIs
5. Add your credentials to the build

## CI/CD Variables (GitLab)

Configure these in **Settings > CI/CD > Variables**:

| Variable | Description |
|----------|-------------|
| `GH_OAUTH_CLIENT_ID` | GitHub OAuth App Client ID |
| `GH_OAUTH_CLIENT_SECRET` | GitHub OAuth App Client Secret |
| `GOOGLE_CLIENT_ID` | Google OAuth Client ID |
| `GOOGLE_CLIENT_SECRET` | Google OAuth Client Secret |

> **Note:** The app builds and runs without API keys - cloud sync will simply be disabled.

## Building with Credentials Locally

```bash
cmake -B build -DGOOGLE_CLIENT_ID="your_client_id" -DGOOGLE_CLIENT_SECRET="your_secret" -DGH_OAUTH_CLIENT_ID="your_gh_id" -DGH_OAUTH_CLIENT_SECRET="your_gh_secret"
cmake --build build --config Release
```
