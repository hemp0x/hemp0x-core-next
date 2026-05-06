Build Hemp0x Core for Windows
=============================

The supported Windows release path is cross-compilation from Linux with the
depends system and the POSIX MinGW-w64 toolchain.

The Windows build produces:

```text
src/hemp0xd.exe
src/hemp0x-cli.exe
src/hemp0x-tx.exe
```

Core Next does not ship the old bundled desktop GUI wallet. Users who want a
graphical wallet should use Hemp0x Commander with a compatible `hemp0xd`
backend.

Recommended build helper
------------------------

From the repository root, run:

```bash
contrib/build-hemp0x-core.sh
```

Choose `Windows` from the menu. The helper checks for the MinGW-w64 toolchain,
verifies that the POSIX thread model is selected, offers to install missing
packages on supported Linux distributions, builds dependencies, strips release
binaries, and can run the Windows binary security check.
Choose `Release binaries` for normal builds. Choose `Dev/debug binaries` only
when debugging or testing developer integrations. The helper selects a parallel
job count automatically from the host CPU count; use `--jobs N` only when you
need to override it.

For a one-command Windows release-style build:

```bash
contrib/build-hemp0x-core.sh --target windows --with-tx --run-tests
```

Ubuntu packages
---------------

If you prefer to install build tools yourself first:

```bash
sudo apt update
sudo apt install -y build-essential autoconf automake libtool pkg-config \
  bsdmainutils curl python3 gawk ca-certificates \
  gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix \
  binutils-mingw-w64-x86-64 mingw-w64
```

Select the POSIX MinGW variant if alternatives are present:

```bash
sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
```

Verify the toolchain:

```bash
x86_64-w64-mingw32-g++ -v 2>&1 | grep -i 'Thread model'
```

Expected:

```text
Thread model: posix
```

Manual build
------------

The helper runs the same basic commands:

```bash
./autogen.sh
make -C depends HOST=x86_64-w64-mingw32 -j"$(nproc)"

CONFIG_SITE="$PWD/depends/x86_64-w64-mingw32/share/config.site" \
./configure --host=x86_64-w64-mingw32 --enable-reduce-exports \
  --with-tx --without-libs --disable-bench

make -j"$(nproc)"
x86_64-w64-mingw32-strip src/hemp0xd.exe src/hemp0x-cli.exe src/hemp0x-tx.exe
make -C src check-security
```

Troubleshooting
---------------

If `x86_64-w64-mingw32-g++` is missing, install the MinGW-w64 packages listed
above.

If the compiler reports `Thread model: win32`, switch to the POSIX variant. The
win32 variant does not provide the standard C++ condition-variable support used
by Hemp0x Core.

If switching between native Linux and Windows builds in the same checkout, run
a clean build. The helper does this by default.
