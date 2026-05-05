macOS Build Instructions and Notes
====================================
The commands in this guide should be executed in a Terminal application.
The built-in one is located in 
```
/Applications/Utilities/Terminal.app
```

Preparation
-----------
Install the macOS command line tools:

`xcode-select --install`

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

Dependencies
----------------------

    brew install automake berkeley-db4 libtool boost miniupnpc openssl@3 pkg-config python libevent

If you run into issues, check [Homebrew's troubleshooting page](https://docs.brew.sh/Troubleshooting).
See [dependencies.md](dependencies.md) for a complete overview.

## Berkeley DB
It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use [this](/contrib/install_db4.sh) script to install it
like so:

```shell
./contrib/install_db4.sh .
```

from the root of the repository.

**Note**: You only need Berkeley DB if the wallet is enabled (see [*Disable-wallet mode*](/doc/build-osx.md#disable-wallet-mode)).

## Build Hemp0x Core

1. Clone the Hemp0x Core source code:
    ```shell
    git clone https://github.com/hemp0x/hemp0x-core
    cd hemp0x-core
    ```

2.  Build hemp0x-core:

    Configure and build the hemp0x binaries.
    ```shell
    ./autogen.sh
    ./configure
    make
    ```

3.  It is recommended to build and run the unit tests:
    ```shell
    make check
    ```

## `disable-wallet` mode
When the intention is to run only a P2P node without a wallet, Hemp0x Core may be
compiled in `disable-wallet` mode with:
```shell
./configure --disable-wallet
```

In this case there is no dependency on Berkeley DB 4.8.

External pool software can request block templates in disable-wallet mode using the
`getblocktemplate` RPC call.

## Running
Hemp0x Core is now available at `./src/hemp0xd`

Before running, you may create an empty configuration file:
```shell
mkdir -p "/Users/${USER}/Library/Application Support/Hemp0x"

touch "/Users/${USER}/Library/Application Support/Hemp0x/hemp0x.conf"

chmod 600 "/Users/${USER}/Library/Application Support/Hemp0x/hemp0x.conf"
```

The first time you run hemp0xd, it will start downloading the blockchain. This process could
take many hours, or even days on slower than average systems.

You can monitor the download process by looking at the debug.log file:
```shell
tail -f $HOME/Library/Application\ Support/Hemp0x/debug.log
```

Other commands:
-------

    ./src/hemp0xd -daemon # Starts the hemp0x daemon.
    ./src/hemp0x-cli --help # Outputs a list of command-line options.
    ./src/hemp0x-cli help # Outputs a list of RPC commands when the daemon is running.

Notes
-----

* Tested on macOS 10.15 and later on 64-bit Intel and Apple Silicon processors.

* autoreconf (boost issue)
