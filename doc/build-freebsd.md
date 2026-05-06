FreeBSD Build Notes
=================================

FreeBSD is not part of the validated Hemp0x Core Next release matrix. The notes
below are a best-effort community reference. Linux and Windows are the
validated release platforms.

FreeBSD 13.0
---------------------------------
Install build tools:

```bash
pkg install autoconf automake git gmake libtool pkgconf python3 gawk
```

Directory structure
------------------
Hemp0x sources in `$HOME/src`.


Hemp0x
------------------

Start in $HOME

Make the directory for sources and go into it.

`mkdir src`

`cd src`

__Download Hemp0x source.__

`git clone https://github.com/beyondcr/hemp0x-core-next.git`

`cd hemp0x-core-next`

__Build dependencies__

For Core Next builds, use the tracked `depends` system so Berkeley DB and other
third-party libraries are built consistently:

```bash
gmake -C depends HOST=x86_64-unknown-freebsd -j"$(sysctl -n hw.ncpu)"
```

__The build process:__

`./autogen.sh`

```bash
CONFIG_SITE="$PWD/depends/x86_64-unknown-freebsd/share/config.site" \
./configure --enable-reduce-exports --with-tx --without-libs --disable-bench
```

`gmake -j$(sysctl -n hw.ncpu)`

hemp0xd and hemp0x-cli are in `src/`


__Optional:__

`gmake install` is available, but for release validation copy binaries from
`src/` into a staging directory instead.

