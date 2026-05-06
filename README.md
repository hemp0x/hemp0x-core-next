Hemp0x Core Next
================

Hemp0x Core Next is the modernized Hemp0x full-node software. It provides the
daemon, command-line RPC client, wallet RPC support, offline transaction
utility, peer-to-peer networking, block and transaction validation, native asset
support, and the services used by Hemp0x infrastructure.

This branch is designed to stay compatible with the live Hemp0x network while
raising the quality bar for builds, security hardening, maintainability, and
release packaging. Node operators should be able to test Core Next beside
existing Hemp0x Core nodes without changing consensus rules or chain data.

Project Goals
-------------

Hemp0x is a proof-of-work UTXO blockchain focused on payments, native assets,
and long-term open participation. Core Next keeps those network rules intact
while improving the software around them:

- daemon and RPC compatibility for existing operators and services
- wallet-enabled `hemp0xd` and `hemp0x-cli` builds without the old bundled GUI
- Linux and Windows release builds through the tracked depends system
- smaller stripped release binaries by default
- stronger P2P, RPC, dependency, and wallet migration test coverage
- updated dependency baselines, including OpenSSL 3.5 LTS, LevelDB 1.23, and a
  modern libsecp256k1 import
- hardened request accounting, relay limits, RPC exposure warnings, and
  operational status reporting
- wallet migration RPCs for exporting and restoring encrypted migration
  envelopes from legacy wallet data
- messaging and asset feature validation through functional tests
- cleaner release documentation and build tooling for operators and developers

Core Next intentionally does not ship the old desktop GUI wallet. Users who
want a graphical wallet should use Hemp0x Commander with a compatible
`hemp0xd` backend.

Live Network Compatibility
--------------------------

Core Next is a non-consensus modernization line. It does not change genesis
data, network magic, ports, address prefixes, proof-of-work validation,
difficulty rules, subsidy rules, asset rules, transaction validation, or block
acceptance semantics.

Consensus-sensitive work remains out of scope unless it is separately designed,
reviewed, tested, and coordinated with the network.

Builds
------

The recommended build path is the guided helper:

```bash
contrib/build-hemp0x-core.sh
```

The helper can build native Linux binaries or cross-build Windows binaries from
Linux. It checks required tools, can install missing build packages on common
distributions, builds third-party dependencies through `depends/`, strips
release binaries, and can run build checks.

Common release build commands:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
contrib/build-hemp0x-core.sh --target windows --with-tx --run-tests
```

Detailed build notes:

- [Linux build guide](doc/build-linux.md)
- [Windows cross-build guide](doc/build-windows.md)
- [macOS build guide](doc/build-osx.md)
- [FreeBSD build guide](doc/build-freebsd.md)
- [OpenBSD build guide](doc/build-openbsd.md)
- [Raspberry Pi build guide](doc/build-raspberrypi.md)

Release Binaries
----------------

Release builds produce:

```text
hemp0xd      / hemp0xd.exe
hemp0x-cli   / hemp0x-cli.exe
hemp0x-tx    / hemp0x-tx.exe
```

`hemp0xd` is the full node and daemon. `hemp0x-cli` is the RPC command-line
client. `hemp0x-tx` is an optional offline transaction utility.

Developer/debug builds are available from the helper menu or with `--debug`.
They are intentionally larger and are not intended for release packaging.

Testing
-------

Core changes should be tested with the relevant unit, security, symbol, and
functional tests before release:

```bash
make check
make -C src check-security
make -C src check-symbols
test/functional/test_runner.py --jobs=4
```

Targeted functional tests cover wallet migration, messaging, RPC exposure,
authentication throttling, node status reporting, P2P request limits, invalid
block storage behavior, and command-line compatibility.

Contributing
------------

Testing and review are valuable contributions. Hemp0x Core is security-critical
software, and careful reproduction steps, build logs, operating-system details,
and test results help move the project forward.

Before opening a pull request:

- build with the depends system when possible
- keep consensus behavior unchanged unless the change is explicitly scoped as a
  network upgrade proposal
- preserve daemon, CLI, wallet RPC, mining-pool RPC, explorer, WebCom, and
  Commander compatibility
- include tests or a clear test plan for behavioral changes
- keep commits small enough to review

Community
---------

- Website: <https://hemp0x.com>
- Wiki: <https://hemp0x.wiki/wiki/Hemp0x_Wiki>
- Discord: <https://discord.gg/Eu4UsYPMGS>
- Telegram: <https://t.me/Hemp0xDev>
- Reddit: <https://www.reddit.com/r/Hemp0x/>

License
-------

Hemp0x Core is released under the MIT license. See [COPYING](COPYING) for the
full license text and copyright notices.
