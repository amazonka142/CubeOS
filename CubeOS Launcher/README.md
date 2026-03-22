# CubeOS Launcher

macOS launcher for [CubeOS](https://github.com/amazonka142/CubeOS) built with `Tauri + SvelteKit`.

It fetches releases from GitHub, installs selected macOS builds side by side, opens the shared CubeOS data folder, and launches installed `.app` bundles directly from the launcher UI.

## Features

- GitHub Releases integration for CubeOS macOS builds
- Side-by-side installs in `~/Library/Application Support/CubeOS Launcher/versions`
- Shared save/settings folder in `~/Library/Application Support/CubeOS`
- Release notes, install status, and one-click launch
- macOS-only install flow for `.dmg`, `.zip`, and `.tar.gz` assets

## Stack

- `SvelteKit 5`
- `Tauri 2`
- `Rust`

## Development

Install dependencies:

```bash
npm install
```

Run the launcher in development mode:

```bash
npm run tauri dev
```

Run type and Svelte checks:

```bash
npm run check
```

Build a debug macOS app bundle:

```bash
npm run tauri build -- --debug
```

## Project Layout

- `src/routes/+page.svelte` — launcher UI
- `src-tauri/src/lib.rs` — native commands for releases, install, launch, and file operations
- `src-tauri/tauri.conf.json` — Tauri window and security config

## Notes

- The launcher currently targets `macOS` only.
- Release metadata is fetched from the public GitHub API for the CubeOS repository.
