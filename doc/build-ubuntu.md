Build Hemp0x Core on Ubuntu
===========================

This document explains how to build Hemp0x Core on Ubuntu without installing
the binaries system-wide or touching an existing node data directory.

The normal Linux build produces:

```text
src/hemp0xd
src/hemp0x-cli
src/hemp0x-tx
```

`hemp0xd` is the daemon/full node. `hemp0x-cli` is the RPC command-line tool.
`hemp0x-tx` is an optional transaction utility.

Recommended build helper
------------------------

From the repository root, run:

```bash
contrib/build-hemp0x-core.sh
```

The helper asks what to build, checks for required tools, offers to install
missing build packages on supported Linux distributions, builds dependencies
through `depends/`, strips release binaries, and can run the build checks.

For a one-command Linux release-style build:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
```

The helper uses the depends system by default. This avoids fragile system
package combinations for Berkeley DB, Boost, OpenSSL, miniupnpc, ZeroMQ, and
other libraries.

Ubuntu packages
---------------

If you prefer to install build tools yourself first:

```bash
sudo apt update
sudo apt install -y build-essential autoconf automake libtool pkg-config \
  bsdmainutils curl python3 gawk ca-certificates
```

Manual build
------------

The helper runs the same basic commands:

```bash
./autogen.sh
make -C depends -j"$(nproc)"

CONFIG_SITE="$PWD/depends/x86_64-pc-linux-gnu/share/config.site" \
./configure --enable-reduce-exports --with-tx --without-libs --disable-bench

make -j"$(nproc)"
strip src/hemp0xd src/hemp0x-cli src/hemp0x-tx
make check
make -C src check-security
make -C src check-symbols
```

Debug builds
------------

Debug binaries are larger and are not intended for release packaging:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --debug
```

Notes
-----

Do not use `sudo make install` for release validation. Copy binaries from
`src/` into a staging directory instead.

Core Next does not ship the old bundled desktop GUI wallet. Users who want a
graphical wallet should use Hemp0x Commander with a compatible `hemp0xd`
backend.
