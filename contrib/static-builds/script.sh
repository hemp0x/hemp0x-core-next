#!/usr/bin/env bash
set -euo pipefail

cat >&2 <<'EOF'
The legacy static-build script is retired for Hemp0x Core Next.

Use the platform build notes under doc/ and the maintained CI scripts under
.github/scripts/ when preparing release artifacts. Core Next release artifacts
must not include Qt wallet installers or local mining binaries.
EOF

exit 1
