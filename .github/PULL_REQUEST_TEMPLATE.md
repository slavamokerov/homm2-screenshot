<!-- Thank you for contributing! Keep the checklist below in mind. CI builds the
     desktop versions on Windows, Linux and macOS, and the web version. -->

## Description

What does this change do, and why? If it fixes an issue, link it with `Fixes #N`.

## Checklist

- [ ] Target branch is `main`.
- [ ] Code comments are in English.
- [ ] Core changes (`vendor/fh2core`, `vendor/h2core`) are made in their own
      repositories and synced with `scripts/sync-core.sh` /
      `scripts/sync-h2core.sh` (never edited in `vendor/` directly).
- [ ] `ctest --test-dir build` is green, and the CI checks pass.
- [ ] For render changes: a poster was compared before/after (native CLI vs
      `node web/run_node.cjs`).

## Testing

- Platforms checked: (e.g. macOS arm64, Windows, Linux, web)

## Screenshots

<!-- Drag & drop screenshots if the change affects the rendered poster. -->
