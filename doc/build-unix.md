UNIX/LINUX BUILD NOTES
====================

These notes cover generic Unix build details. For validated release platforms,
see [build-linux.md](build-linux.md) (Linux) and [build-windows.md](build-windows.md)
(Windows cross-build). Other Unix platforms are best-effort community builds.
For release-style builds, prefer the tracked `depends` system and the guided
helper documented in those platform-specific guides.

Some notes on how to build Hemp0x Core in *nix.


Note
---------------------
Always use absolute paths to configure and compile Hemp0x Core and the dependencies,
for example, when specifying the path of the dependency:

	../dist/configure --enable-cxx --disable-shared --with-pic --prefix=$BDB_PREFIX

Here BDB_PREFIX must be an absolute path - it is defined using $(pwd) which ensures
the usage of the absolute path.

To Build
---------------------

```bash
./autogen.sh
./configure
make
```

This builds hemp0xd (daemon), hemp0x-cli (RPC client), and hemp0x-tx (offline
transaction utility).

On most Linux distros the "fPIC" flag needs to be set.  If this flag is not specified it is possible that the build will fail with an error similar to:
```bash
relocation R_X86_64_32 against `.rodata' can not be used when making a shared object; recompile with -fPIC
```
 
To resolve or avoid the following build error specify the following configure parameters, make clean, and then build:
```bash
./configure --enable-cxx --disable-shared --with-pic --prefix=$BDB_PREFIX CXXFLAGS="-fPIC" CPPFLAGS="-fPIC"
make clean
make
```

Dependencies
---------------------

These dependencies are required:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 libssl      | Crypto           | Random Number Generation, Elliptic Curve Cryptography
 libboost    | Utility          | Library for threading, data structures, etc
 libevent    | Networking       | OS independent asynchronous networking
 libdb++     | Utility          | Contains headers and static libraries for the Berkeley DB library

Optional dependencies:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 miniupnpc   | UPnP Support     | Firewall-jumping support
 libdb4.8    | Berkeley DB      | Wallet storage (only needed when wallet enabled)
 univalue    | Utility          | JSON parsing and encoding (bundled version will be used unless --with-system-univalue passed to configure)
 libzmq3     | ZMQ notification | Optional, allows generating ZMQ notifications (requires ZMQ version >= 4.x)

For the versions used, see [dependencies.md](dependencies.md)


## Linux Distribution Specific Instructions

### Linux

Linux distribution-specific instructions, see [build-linux.md](build-linux.md).


Notes
-----
Do not use `make install` for release validation. Copy binaries from `src/` into
a staging directory instead. For manual test builds, `strip src/hemp0xd
src/hemp0x-cli src/hemp0x-tx` can be used before local distribution.


miniupnpc
---------

[miniupnpc](http://miniupnp.free.fr/) may be used for UPnP port mapping.  It can be downloaded from [here](
http://miniupnp.tuxfamily.org/files/).  UPnP support is compiled in and
turned off by default.  See the configure options for upnp behavior desired:

	--without-miniupnpc      No UPnP support miniupnp not required
	--disable-upnp-default   (the default) UPnP support turned off by default at runtime
	--enable-upnp-default    UPnP support turned on by default at runtime


Berkeley DB
-----------
It is recommended to use Berkeley DB 4.8. For Core Next source builds the
recommended way to obtain it is through the tracked `depends` system
(see the Linux and Windows build guides).

**Note**: You only need Berkeley DB if the wallet is enabled (see the section *Disable-Wallet mode* below).

Boost
-----
If you need to build Boost yourself for a community build:

	sudo su
	./bootstrap.sh
	./bjam install


Security
--------
To help make your Hemp0x installation more secure by making certain attacks impossible to
exploit even if a vulnerability is found, binaries are hardened by default.
This can be disabled with:

	./configure --disable-hardening


Hardening enables the following features:

* Position Independent Executable
    Build position independent code to take advantage of Address Space Layout Randomization
    offered by some kernels. Attackers who can cause execution of code at an arbitrary memory
    location are thwarted if they don't know where anything useful is located.
    The stack and heap are randomly located by default but this allows the code section to be
    randomly located as well.

    On an AMD64 processor where a library was not compiled with -fPIC, this will cause an error
    such as: "relocation R_X86_64_32 against `......' can not be used when making a shared object;"

    To test that you have built PIE executable, install scanelf, part of paxutils, and use:

	scanelf -e ./hemp0xd

    The output should contain:

     TYPE
    ET_DYN

* Non-executable Stack
    If the stack is executable then trivial stack based buffer overflow exploits are possible if
    vulnerable buffers are found. By default, Hemp0x Core should be built with a non-executable stack
    but if one of the libraries it uses asks for an executable stack or someone makes a mistake
    and uses a compiler extension which requires an executable stack, it will silently build an
    executable without the non-executable stack protection.

    To verify that the stack is non-executable after compiling use:
    `scanelf -e ./hemp0xd`

    the output should contain:
	STK/REL/PTL
	RW- R-- RW-

    The STK RW- means that the stack is readable and writeable but not executable.

Disable-wallet mode
--------------------
When the intention is to run only a P2P node without a wallet, Hemp0x Core may be compiled in
disable-wallet mode with:

    ./configure --disable-wallet

In this case there is no dependency on Berkeley DB 4.8.

External pool software can request block templates in disable-wallet mode with the
`getblocktemplate` RPC. The legacy `getwork` RPC is not supported.

Additional Configure Flags
--------------------------
A list of additional configure flags can be displayed with:

    ./configure --help


Setup and Build Example: Arch Linux
-----------------------------------
This legacy example lists the steps necessary to setup and build a command line
only, non-wallet distribution on Arch Linux. For wallet-enabled release builds,
prefer the tracked `depends` system through [build-linux.md](build-linux.md).

    pacman -S git base-devel boost libevent python
    git clone https://github.com/hemp0x/hemp0x-core.git
    cd hemp0x-core/
    ./autogen.sh
    ./configure --disable-wallet --without-miniupnpc
    make check

Note:
Wallet-enabled release builds should use Berkeley DB 4.8 from the tracked
`depends` system for portable `wallet.dat` compatibility.


ARM Cross-compilation
-------------------
These steps can be performed on any Linux host. The depends system
will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

Make sure you install the build requirements mentioned above.
Then, install the toolchain and curl:

    sudo apt-get install g++-arm-linux-gnueabihf curl

To build executables for ARM:

    cd depends
    make HOST=arm-linux-gnueabihf
    cd ..
    ./autogen.sh
    ./configure --prefix=$PWD/depends/arm-linux-gnueabihf --enable-glibc-back-compat --enable-reduce-exports LDFLAGS=-static-libstdc++
    make


For further documentation on the depends system see [README.md](../depends/README.md) in the depends directory.

Building on FreeBSD and OpenBSD
--------------------

FreeBSD specific instructions, see [build-freebsd.md](build-freebsd.md))

OpenBSD specific instructions, see [build-openbsd.md](build-openbsd.md))
