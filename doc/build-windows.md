Build Hemp0x Core for Windows
=============================

The supported release path for Windows binaries is cross-compilation from Linux
using the depends system and the POSIX MinGW-w64 toolchain. The Qt wallet GUI is
not part of Core Next release builds.

Install cross-build tools on Ubuntu
-----------------------------------

On Ubuntu 22.04 or later:

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

The expected output is:

```text
Thread model: posix
```

Build Windows binaries
----------------------

From the repository root:

```bash
contrib/build-hemp0x-core.sh --target windows --with-tx --run-tests
```

The binaries are written to:

```text
src/hemp0xd.exe
src/hemp0x-cli.exe
src/hemp0x-tx.exe
```

Manual build
------------

The helper script runs the same basic commands:

```bash
./autogen.sh
make -C depends HOST=x86_64-w64-mingw32 -j"$(nproc)"

CONFIG_SITE="$PWD/depends/x86_64-w64-mingw32/share/config.site" \
./configure --host=x86_64-w64-mingw32 --enable-reduce-exports --with-tx

make -j"$(nproc)"
make -C src check-security
```

Troubleshooting
---------------

If the build reports that `x86_64-w64-mingw32-g++` is missing, install the
MinGW-w64 packages listed above.

If the compiler uses `Thread model: win32`, switch to the POSIX variant. The
win32 variant does not provide the standard C++ condition-variable support that
Hemp0x Core needs.

If switching from a native Linux build to a Windows cross-build in the same
checkout, run:

```bash
make distclean || true
```

before configuring for Windows.
