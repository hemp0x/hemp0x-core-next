Hemp0x Core Next
================

<p align="center">
  <img src="doc/assets/hemp0x-logo.webp" alt="Hemp0x logo" width="160">
</p>

Hemp0x Core Next is the modernization branch for Hemp0x Core. It builds the
full-node daemon, command-line RPC client, wallet RPC support, offline
transaction utility, peer-to-peer networking, block and transaction validation,
native asset support, and the interfaces used by Hemp0x services.

The first Core Next release is designed to be fully compatible with the current
Hemp0x chain and existing Hemp0x Core operations. It does not introduce
consensus-changing features. Operators should be able to test Core Next beside
existing Hemp0x Core nodes without changing chain data or network rules.

What Changed
------------

Core Next keeps the live Hemp0x rules intact while improving the software
around them:

- daemon, CLI, wallet RPC, pool RPC, explorer, WebCom, and Commander
  compatibility
- removal of the old bundled Qt desktop wallet and Qt dependency paths from
  release builds
- wallet-enabled `hemp0xd` and `hemp0x-cli` builds for operators and services
- Linux and Windows release builds through a guided build helper and the
  tracked depends system
- stripped release binaries by default, with larger dev/debug builds available
  when needed
- refreshed dependency baselines, including OpenSSL 3.5 LTS, LevelDB 1.23, and
  a modern libsecp256k1 import, to move the codebase closer to maintained
  Bitcoin Core-era security foundations
- refreshed bundled Boost, MiniUPnPc, ZeroMQ, libevent, and Windows build
  support
- removal of built-in local CPU/block generation from release binaries while
  keeping pool-facing `getblocktemplate`, `submitblock`, and KAWPOW helper RPCs
- P2P request, relay accounting, addrman load, and misbehavior-score hardening
- invalid-block storage checks and reorg diagnostics
- verified mainnet trust anchors, refreshed checkpoint data, and updated chain
  transaction statistics
- RPC exposure warnings, repeated-auth throttling, auth-cookie hardening, log
  redaction, and a node-status summary RPC
- wallet input shuffling to mitigate CVE-2021-37492-style privacy leakage
- reduced wallet passphrase lifetime and warnings for legacy secret import/export
  RPCs
- safer first-run configuration with generated `hemp.conf` templates
- wallet migration RPCs for exporting, validating, and restoring encrypted
  migration envelopes from legacy wallet data
- the first wallet-storage migration step away from long-term Berkeley DB
  dependence, while preserving existing `wallet.dat` compatibility for this
  release
- KAWPOW epoch context cache hardening and hash guardrail tests
- corrected Hemp0x reward, subsidy, emission, BIP65/BIP66 accessor, and test
  fixture assumptions inherited from earlier code
- messaging and asset regression tests
- invalid-block storage regression tests
- restored and expanded functional test coverage for modernized behavior
- updated Linux and Windows build documentation
- release packaging cleanup for daemon, CLI, and transaction utility builds

Core Next also creates room for future Hemp0x Core work. New features and
network upgrades can be explored over time, but consensus-sensitive changes
must be designed, reviewed, tested, and coordinated separately before they land
in production releases.

Compatibility
-------------

This release is a non-consensus compatibility release. It does not change:

- genesis block data
- network magic or message-start bytes
- default P2P ports or address prefixes
- proof-of-work validation
- difficulty adjustment rules
- subsidy rules
- asset consensus rules
- transaction or block validation semantics

The intent is practical: let operators test the modernized software on the live
network while keeping the old and new binaries compatible during the transition.

Wallet Direction
----------------

Core Next does not ship the old bundled desktop GUI wallet. Users who want a
graphical wallet should use Hemp0x Commander with a compatible `hemp0xd`
backend.

The daemon still includes wallet RPC support. Wallet migration RPCs are included
to help move legacy wallet data toward newer wallet workflows without changing
consensus rules. See the [wallet migration guide](doc/wallet-migration.md) for
the export, validation, and restore commands.

Mining Interface
----------------

Core Next does not include a built-in CPU miner or local release-binary block
generator. That code was removed to keep node binaries focused on validation,
RPC service, and network operation.

Pool and external-miner workflows remain supported through `getblocktemplate`,
`submitblock`, and the KAWPOW helper RPCs. Projects that fork Hemp0x and need
to search for a genesis block should use a standalone genesis/mining tool
instead of embedding miner code in the node binary. Regtest automation in this
repository constructs and submits blocks through the test framework; manual
regtest block production should use external tooling against the same RPC
interfaces.

Builds
------

The recommended build path is the guided helper:

```bash
contrib/build-hemp0x-core.sh
```

The helper can build native Linux binaries or cross-build Windows binaries from
Linux. It checks required tools, can install missing build packages on common
Linux distributions, builds third-party dependencies through `depends/`, strips
release binaries, and can run build checks.

Common release build commands:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
contrib/build-hemp0x-core.sh --target windows --with-tx --run-tests
```

Build guides:

- [Linux build guide](doc/build-linux.md)
- [Windows cross-build guide](doc/build-windows.md)
- [Configuration guide](doc/hemp0x-conf.md)
- [Wallet migration guide](doc/wallet-migration.md)
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
block storage behavior, generated configuration files, and command-line
compatibility.

Contributing
------------

Testing, review, documentation, and focused patches are welcome. Start with
[CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.

Community
---------

- Website: <https://hemp0x.com>
- Discord: <https://discord.gg/Eu4UsYPMGS>
- Reddit: <https://www.reddit.com/r/Hemp0x/>

License
-------

Hemp0x Core is released under the MIT license. See [COPYING](COPYING) for the
full license text and copyright notices.
