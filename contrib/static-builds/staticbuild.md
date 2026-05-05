# Legacy static build workflow

The previous static-build guide has been retired for Hemp0x Core Next. It
described obsolete release infrastructure, including old Ubuntu builders, Qt
wallet installer artifacts, Win32 packages, and macOS DMG signing steps.

For current builds, use the platform guides in `doc/` and the maintained CI
scripts under `.github/scripts/`. Release artifacts should be built from a clean
tag, stripped during staging, checksummed with SHA256, and published without Qt
wallet installers or local mining binaries.
