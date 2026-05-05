OpenBSD build notes
===================

OpenBSD builds are not part of the regular Hemp0x Core release matrix. The old
OpenBSD guide depended on obsolete compiler and Boost instructions, so it has
been retired rather than kept as misleading build guidance.

Developers who want to maintain OpenBSD support should start from the generic
Unix build notes in [build-unix.md](build-unix.md), use current OpenBSD ports
where possible, and validate the result with:

```bash
./autogen.sh
./configure
gmake
gmake check
```

Wallet-enabled builds still require Berkeley DB 4.8-compatible wallet support.
For node-only builds, use `./configure --disable-wallet`.
