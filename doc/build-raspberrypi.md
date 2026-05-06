Raspberry Pi Build Notes
========================

Raspberry Pi builds are not part of the validated Hemp0x Core Next release
matrix. The notes below are a best-effort community reference. Linux and
Windows are the validated release platforms.

These notes cover building Hemp0x Core Next on Raspberry Pi hardware. ARM
builds are best handled through the tracked `depends` system:

```bash
./autogen.sh
make -C depends HOST=arm-linux-gnueabihf -j"$(nproc)"

CONFIG_SITE="$PWD/depends/arm-linux-gnueabihf/share/config.site" \
./configure --enable-reduce-exports --with-tx --without-libs --disable-bench

make -j"$(nproc)"
```

The guided helper currently targets native Linux and Windows cross-builds. Use
the manual commands above for Raspberry Pi builds until ARM targets are added
to the helper.

Dependencies
------------

Install build tools:

```bash
sudo apt-get update
sudo apt-get install -y build-essential autoconf automake libtool pkg-config \
  bsdmainutils curl python3 gawk ca-certificates git
```

Increase swap size for low-memory devices
-----------------------------------------

On devices with limited RAM, increase swap before building:

```bash
sudo nano /etc/dphys-swapfile
# Change CONF_SWAPSIZE=100 to CONF_SWAPSIZE=1000
sudo reboot
```

Notes
-----

- Use `-j2` instead of `-j"$(nproc)"` if the build runs out of memory.
- Do not use `sudo make install` for release validation. Copy binaries from
  `src/` into a staging directory instead.
- Core Next does not ship the old bundled desktop GUI wallet.
- Wallet-enabled builds still require BDB 4.8-compatible wallet support;
  the `depends` system provides this automatically.
- For node-only builds, add `--disable-wallet` to configure.

See [build-linux.md](build-linux.md) for more details on the Linux build process.
