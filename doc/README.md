Hemp0x Core
==============

Setup
---------------------
Hemp0x Core is the original Hemp0x client and it builds the backbone of the network. It downloads and, by default, stores the entire history of Hemp0x transactions; depending on the speed of your computer and network connection, the synchronization process is typically complete in under an hour.

To download compiled binaries of the Hemp0x Core and wallet, visit the [GitHub release page](https://github.com/hemp0x/hemp0x-core/releases).

Running
---------------------
The following are some helpful notes on how to run Hemp0x on your native platform.

### Linux

1) Download and extract binaries to desired folder.

2) Install distribution-specific dependencies listed below.

3) Run the Hemp0x Core daemon

   `./hemp0xd -daemon`

#### Ubuntu 16.04, 17.04/17.10 and 18.04

Update apt cache and install general dependencies:

```
sudo apt update
sudo apt install libevent-dev libboost-all-dev libminiupnpc10 libzmq5 software-properties-common
```

The wallet requires version 4.8 of the Berkeley DB. The easiest way to get it is to build it with the script contrib/install_db4.sh


```

#### Fedora 27

Install general dependencies:

`sudo dnf install zeromq libevent boost libdb4-cxx miniupnpc`

#### CentOS 7

Add the EPEL repository and install general depencencies:

```
sudo yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
sudo yum install zeromq libevent boost libdb4-cxx miniupnpc
```

### OS X

Build from source and run `./src/hemp0xd`.

### Windows

1) Download windows-x86_64.zip and unpack executables to desired folder.

2) Run `hemp0xd.exe` or control a running daemon with `hemp0x-cli.exe`.

### Need Help?

- See the documentation at the [Hemp0x Wiki](https://hemp0x.wiki/wiki/Hemp0x_Wiki)
for help and more information.
- Ask for help on [Discord](https://discord.gg/DUkcBst), [Telegram](https://t.me/Hemp0xDev) or [Reddit](https://www.reddit.com/r/Hemp0x/).

Building from source
---------------------
The following are developer notes on how to build the Hemp0x core software on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](https://github.com/hemp0x/hemp0x-core/tree/master/doc/dependencies.md)
- [OS X Build Notes](https://github.com/hemp0x/hemp0x-core/tree/master/doc/build-osx.md)
- [Unix Build Notes](https://github.com/hemp0x/hemp0x-core/tree/master/doc/build-unix.md)
- [Windows Build Notes](https://github.com/hemp0x/hemp0x-core/tree/master/doc/build-windows.md)
- [OpenBSD Build Notes](https://github.com/hemp0x/hemp0x-core/tree/master/doc/build-openbsd.md)
- [Gitian Building Guide](https://github.com/hemp0x/hemp0x-core/tree/master/doc/gitian-building.md)

Development
---------------------
Hemp0x repo's [root README](https://github.com/hemp0x/hemp0x-core/blob/master/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](https://github.com/hemp0x/hemp0x-core/blob/master/doc/developer-notes.md)
- [Release Notes](https://github.com/hemp0x/hemp0x-core/blob/master/doc/release-notes.md)
- [Release Process](https://github.com/hemp0x/hemp0x-core/blob/master/doc/release-process.md)
- [Source Code Documentation (External Link)](https://dev.visucore.com/hemp0x/doxygen/) -- 2018-05-11 -- Broken link
- [Translation Process](https://github.com/hemp0x/hemp0x-core/blob/master/doc/translation_process.md)
- [Translation Strings Policy](https://github.com/hemp0x/hemp0x-core/blob/master/doc/translation_strings_policy.md)
- [Travis CI](https://github.com/hemp0x/hemp0x-core/blob/master/doc/travis-ci.md)
- [Unauthenticated REST Interface](https://github.com/hemp0x/hemp0x-core/blob/master/doc/REST-interface.md)
- [Shared Libraries](https://github.com/hemp0x/hemp0x-core/blob/master/doc/shared-libraries.md)
- [BIPS](https://github.com/hemp0x/hemp0x-core/blob/master/doc/bips.md)
- [Dnsseed Policy](https://github.com/hemp0x/hemp0x-core/blob/master/doc/dnsseed-policy.md)
- [Benchmarking](https://github.com/hemp0x/hemp0x-core/blob/master/doc/benchmarking.md)

### Resources
- Discuss on chat [Discord](https://discord.gg/jn6uhur), [Telegram](https://t.me/Hemp0xDev) or [Reddit](https://www.reddit.com/r/Hemp0x/).
- Find out more on the [Hemp0x Wiki](https://hemp0x.wiki/wiki/Hemp0x_Wiki)
- Visit the project home [hemp0x.com](https://hemp0x.com)

### Miscellaneous
- [Assets Attribution](https://github.com/hemp0x/hemp0x-core/blob/master/doc/assets-attribution.md)
- [Files](https://github.com/hemp0x/hemp0x-core/blob/master/doc/files.md)
- [Fuzz-testing](https://github.com/hemp0x/hemp0x-core/blob/master/doc/fuzzing.md)
- [Reduce Traffic](https://github.com/hemp0x/hemp0x-core/blob/master/doc/reduce-traffic.md)
- [Tor Support](https://github.com/hemp0x/hemp0x-core/blob/master/doc/tor.md)
- [Init Scripts (systemd/upstart/openrc)](https://github.com/hemp0x/hemp0x-core/blob/master/doc/init.md)
- [ZMQ](https://github.com/hemp0x/hemp0x-core/blob/master/doc/zmq.md)

License
---------------------
Distributed under the [MIT software license](https://github.com/hemp0x/hemp0x-core/blob/master/COPYING).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
