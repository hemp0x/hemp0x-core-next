Build Hemp0x Core on Linux
==========================

This document explains how to build Hemp0x Core on Linux without installing
the binaries system-wide or touching an existing node data directory.

The normal Linux build produces:

```text
src/hemp0xd
src/hemp0x-cli
src/hemp0x-tx
```

`hemp0xd` is the daemon/full node. `hemp0x-cli` is the RPC command-line tool.
`hemp0x-tx` is an optional offline transaction utility.

Recommended Build Helper
------------------------

From the repository root, run:

```bash
contrib/build-hemp0x-core.sh
```

The helper asks what to build, checks for required tools, offers to install
missing build packages on supported Linux distributions, builds dependencies
through `depends/`, strips release binaries, and can run build checks.

Choose `Release binaries` for normal builds. Choose `Dev/debug binaries` only
when debugging or testing developer integrations. The helper selects a parallel
job count automatically from the host CPU count; use `--jobs N` only when you
need to override it.

For a one-command Linux release-style build:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
```

The helper uses the depends system by default. This avoids fragile system
package combinations for Berkeley DB, Boost, OpenSSL, miniupnpc, ZeroMQ, and
other libraries.

Supported Package Managers
--------------------------

The helper can suggest and, in interactive mode, install build packages on
systems that use:

- `apt-get` for Ubuntu, Debian, Linux Mint, and related distributions
- `dnf` for Fedora, Nobara, CentOS Stream, RHEL-family systems with DNF, and
  related distributions
- `pacman` for Arch Linux and related distributions
- `zypper` for openSUSE and SUSE-family systems

Other Linux distributions can still build Hemp0x Core. Install the equivalent
compiler, autotools, libtool, pkg-config, curl, Python 3, gawk, patch, hexdump,
and standard build utilities, then run the helper again.

Manual Package Installation
---------------------------

Ubuntu, Debian, Linux Mint:

```bash
sudo apt update
sudo apt install -y build-essential autoconf automake libtool pkg-config \
  bsdmainutils curl python3 gawk ca-certificates binutils file
```

Fedora, Nobara:

```bash
sudo dnf install -y gcc gcc-c++ make autoconf automake libtool pkgconf \
  curl python3 gawk diffutils patch findutils util-linux binutils file
```

Arch Linux:

```bash
sudo pacman -S --needed base-devel autoconf automake libtool pkgconf \
  curl python gawk patch util-linux binutils file
```

openSUSE:

```bash
sudo zypper install -y gcc gcc-c++ make autoconf automake libtool \
  pkg-config curl python3 gawk patch util-linux binutils file
```

Manual Build
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

Debug Builds
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
