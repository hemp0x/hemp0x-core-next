Legacy static build workflow
============================

The old static-build workflow is retired for Hemp0x Core Next. It referenced
obsolete Ubuntu releases, Qt wallet installer artifacts, Win32 packages, and
macOS DMG signing steps that are no longer part of the release process.

Use the maintained platform build notes under `doc/` and the CI scripts under
`.github/scripts/` when preparing release artifacts. Core Next release artifacts
should contain command-line binaries and required runtime files only; do not
publish Qt wallet installers or local mining binaries.
