Hemp0x Core
==============

Setup
---------------------
Hemp0x Core is the original Hemp0x client and it builds the backbone of the network. It downloads and, by default, stores the entire history of Hemp0x transactions; depending on hardware and network conditions, initial synchronization can take several hours or longer.

To download compiled daemon and command-line binaries, visit the project release page. Core Next release builds provide daemon, CLI, and transaction-utility binaries; wallet RPC support remains available in wallet-enabled daemon builds. Users who want a graphical wallet should use Hemp0x Commander with a compatible `hemp0xd` backend.

Running
---------------------
The following are some helpful notes on how to run Hemp0x on your native platform.

### Linux

1) Download and extract binaries to desired folder.

2) Install distribution-specific dependencies listed below.

3) Run the Hemp0x Core daemon

   `./hemp0xd -daemon`

#### Ubuntu 22.04 and later

Update apt cache and install general dependencies:

```
sudo apt update
sudo apt install libevent-dev libboost-all-dev libminiupnpc-dev libzmq3-dev software-properties-common
```

The legacy wallet backend requires Berkeley DB 4.8 for portable `wallet.dat` compatibility. The recommended way to get it for source builds is to use the tracked `depends` system.

#### Fedora

Install general dependencies:

`sudo dnf install zeromq libevent boost libdb-cxx miniupnpc`

#### CentOS Stream / RHEL 9

Install general dependencies:

```
sudo dnf install zeromq libevent boost libdb-cxx miniupnpc
```

### macOS

Build from source and run `./src/hemp0xd`.

### Windows

1) Download windows-x86_64.zip and unpack executables to desired folder.

2) Run `hemp0xd.exe` or control a running daemon with `hemp0x-cli.exe`.

### Need Help?

- Ask for help on [Discord](https://discord.gg/Eu4UsYPMGS) or [Reddit](https://www.reddit.com/r/Hemp0x/).

Building from source
---------------------
The following are developer notes on how to build the Hemp0x core software on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [Linux Build Notes](build-linux.md)
- [Windows Build Notes](build-windows.md)
- [macOS Build Notes](build-osx.md)
- [Configuration Notes](hemp0x-conf.md)
- [Wallet Migration Notes](wallet-migration.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [FreeBSD Build Notes](build-freebsd.md)
- [Raspberry Pi Build Notes](build-raspberrypi.md)

Development
---------------------
The [root README](../README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- Source Code Documentation: generate locally with Doxygen when needed.
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Resources
- Discuss on [Discord](https://discord.gg/Eu4UsYPMGS) or [Reddit](https://www.reddit.com/r/Hemp0x/).
- Visit the project home [hemp0x.com](https://hemp0x.com)

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](../COPYING).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
