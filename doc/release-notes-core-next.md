Hemp0x Core Next Release Notes
==============================

This document describes the first Core Next release. It is a non-consensus
modernization release that keeps all live Hemp0x chain rules intact while
improving the software around them.

Compatibility Statement
-----------------------

This release is fully compatible with the current Hemp0x mainnet. It does not
change:

- genesis block data
- network magic or message-start bytes
- default P2P port (42069) or RPC port (8766)
- address prefixes (0x3c, "R" addresses)
- KAWPOW proof-of-work validation
- Dark Gravity Wave difficulty adjustment (180-block lookback)
- subsidy schedule (10 HEMP initial, halving via integer right-shift)
- asset consensus rules
- transaction or block validation semantics

Operators can run Core Next binaries alongside existing Hemp0x Core nodes on
the same chain.

Major Changes
-------------

### Qt GUI Wallet Removed

Core Next does not ship the old bundled Qt desktop wallet. The Qt dependency
paths have been removed from release builds. Users who want a graphical wallet
should use Hemp0x Commander, a separate Tauri-based desktop application that
communicates with `hemp0xd` via RPC.

### Local CPU Miner Removed

The built-in CPU miner and local block generation code has been removed from
release binaries. This keeps node binaries focused on validation, RPC service,
and network operation.

Pool and external-miner workflows remain supported through `getblocktemplate`,
`submitblock`, and KAWPOW helper RPCs. External mining tools should be used
for block construction.

### Production Binaries

Release builds produce:

- `hemp0xd` / `hemp0xd.exe` - full node daemon
- `hemp0x-cli` / `hemp0x-cli.exe` - RPC command-line client
- `hemp0x-tx` / `hemp0x-tx.exe` - offline transaction utility

Binaries are stripped by default. Larger dev/debug builds are available when
needed.

Wallet Migration Notes
----------------------

Core Next includes wallet migration RPCs for moving legacy wallet data into
modern encrypted migration envelopes:

- `exportwalletmigration` - writes a migration envelope to disk
- `validatewalletmigration` - checks an envelope without creating a wallet
- `restorewalletmigration` - restores a new wallet from an encrypted envelope

These commands support public-only envelopes (metadata checks) and encrypted
private envelopes (full restore with BIP39/BIP44 coin type 420).

The legacy `wallet.dat` (Berkeley DB 4.8) format remains compatible for this
release. The migration RPCs provide a path toward newer wallet workflows
without changing consensus rules.

Note: dynamic `loadwallet`/`unloadwallet` RPCs are not included in this
release. Restored wallets are loaded by the restore RPC and can be selected
on restart with `-wallet=<wallet_name>`.

See the [wallet migration guide](wallet-migration.md) for detailed usage.

Mining Interface Notes
----------------------

The following RPCs remain available for pool and external mining:

- `getblocktemplate` - block template requests
- `submitblock` - external block submission
- KAWPOW helper RPCs for proof-of-work verification

The old `generatetoaddress` and related local generation RPCs return error
-32601 (method not found). Regtest automation in this repository uses the
test framework's external block construction with `submitblock`.

Security Hardening
------------------

### RPC and Authentication

- Auth-cookie hardening for RPC authentication
- Repeated-auth throttling to limit brute-force attempts
- Log redaction to prevent sensitive data exposure in debug logs
- RPC exposure warnings for operators

### Network and P2P

- P2P request tracking with bounded memory use
- Relay accounting improvements
- Addrman load bounds checks
- Misbehavior-score guard to prevent integer overflow
- Invalid-block storage checks and reorg diagnostics

### Wallet Security

- Wallet input shuffling to mitigate CVE-2021-37492-style privacy leakage
  (input-ordering fingerprinting)
- Reduced wallet passphrase lifetime defaults
- Warnings for legacy secret import/export RPCs
- Safer first-run configuration with generated `hemp.conf` templates

### KAWPOW

- KAWPOW epoch context cache hardening
- Hash guardrail tests for PoW verification

### Trust Anchors

- Verified mainnet `nMinimumChainWork` and `defaultAssumeValid` values
- Refreshed checkpoint data at block 2,000,000
- Updated chain transaction statistics

Dependency Updates
------------------

The following dependencies have been refreshed as part of security
modernization:

- **OpenSSL 3.5 LTS** - cryptographic library baseline
- **LevelDB 1.23** - chainstate database
- **libsecp256k1 v0.7.1** - elliptic curve operations (modern import)
- **Boost 1.90.0** - C++ utility library
- **MiniUPnPc 2.3.3** - UPnP dependency refresh
- **ZeroMQ 4.3.5** - optional ZMQ notification dependency refresh
- **libevent 2.1.12-stable** - networking/event dependency baseline
- **Berkeley DB 4.8.30** - retained for legacy `wallet.dat` compatibility

These updates move the codebase closer to maintained security foundations.
Dependency updates are described as security modernization unless exact CVEs
are mapped in the tree.

Build Notes
-----------

### Linux

The recommended build path is the guided helper:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
```

See [build-linux.md](build-linux.md) for manual build instructions.

### Windows

Cross-compilation from Linux using the POSIX MinGW-w64 toolchain:

```bash
contrib/build-hemp0x-core.sh --target windows --with-tx --run-tests
```

See [build-windows.md](build-windows.md) for toolchain setup and troubleshooting.

### Configuration

On first launch, `hemp0xd` creates a `hemp.conf` template when the default
configuration file is missing. The generated file enables loopback-only local
RPC with cookie authentication and includes commented examples for common
settings.

See [hemp0x-conf.md](hemp0x-conf.md) for configuration details.

Tests to Run
------------

Before deploying or upgrading, run the following:

```bash
make check                    # unit tests
make -C src check-security    # binary security checks
make -C src check-symbols     # symbol visibility checks
test/functional/test_runner.py --jobs=4  # functional tests
```

Targeted functional tests cover wallet migration, messaging, RPC exposure,
authentication throttling, node status reporting, P2P request limits, invalid
block storage, generated configuration files, and command-line compatibility.

Upgrade Guidance
----------------

1. Back up your `wallet.dat` and data directory before upgrading.
2. Shut down the existing `hemp0xd` cleanly.
3. Replace binaries with the new release.
4. Start `hemp0xd` and verify synchronization.
5. If using wallet migration, follow the export/validate/restore flow in the
   [wallet migration guide](wallet-migration.md).
6. Verify RPC connectivity with `hemp0x-cli getblockchaininfo`.

Operators running Hemp0x Commander or WebCom should verify RPC compatibility
after upgrade. All 165 RPC commands remain available.

Testing Guidance
----------------

- Test on regtest before mainnet deployment.
- Verify wallet migration with a non-production wallet.
- Confirm pool RPCs (`getblocktemplate`, `submitblock`) work with your mining
  infrastructure.
- Run the full test suite on your build platform.

Known Limitations
-----------------

- No dynamic `loadwallet`/`unloadwallet` RPCs. Wallets are loaded at startup
  or by the `restorewalletmigration` RPC.
- No Qt GUI wallet. Use Hemp0x Commander for a graphical interface.
- No built-in CPU miner. Use external mining tools with `getblocktemplate`
  and `submitblock`.
- Berkeley DB 4.8 wallet support is preserved for this release. A future
  wallet storage migration may replace the long-term BDB dependence.

Credits
-------

Thanks to everyone who contributed to this release.
