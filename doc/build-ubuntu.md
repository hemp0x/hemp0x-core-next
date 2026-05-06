Build Hemp0x Core on Ubuntu
===========================

These instructions build `hemp0xd`, `hemp0x-cli`, and optionally `hemp0x-tx`
from source without installing them or touching an existing node data directory.

The recommended path is to use the repository build helper. It builds the
project through the depends system so the result does not depend on old system
Berkeley DB, Boost, OpenSSL, or miniupnpc packages.

Install host build tools
------------------------

On Ubuntu 22.04 or later:

```bash
sudo apt update
sudo apt install -y build-essential autoconf automake libtool pkg-config \
  bsdmainutils curl python3 gawk ca-certificates
```

Build native Linux binaries
---------------------------

From the repository root:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
```

The binaries are written to:

```text
src/hemp0xd
src/hemp0x-cli
src/hemp0x-tx
```

Manual build
------------

The helper script runs the same basic commands:

```bash
./autogen.sh
make -C depends -j"$(nproc)"

CONFIG_SITE="$PWD/depends/x86_64-pc-linux-gnu/share/config.site" \
./configure --enable-reduce-exports --with-tx

make -j"$(nproc)"
make check
make -C src check-security
make -C src check-symbols
```

Notes
-----

Do not use `sudo make install` for development or release validation. Copy the
binaries from `src/` into a staging directory instead.

If you need debug binaries, use:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --debug
```
