# Hemp0x Core Next Follow-Up Items

This file tracks non-consensus follow-up work discovered during release
testing and normal project maintenance. Items listed here are not release
blockers unless they are explicitly marked as such. They are kept in one
place so future maintenance releases can group related fixes, test them
together, and avoid losing small but useful improvements.

Consensus-changing ideas should not be tracked here as ordinary TODO items.
They require separate design, review, testing, and network coordination.

## RPC UX

- `getassetdata` should return a clear JSON-RPC error when the requested
  asset does not exist.
  Current behavior: empty output with success status.
  Desired behavior: a structured error such as `Asset not found`.
  Scope: RPC behavior only; no consensus or wallet-storage changes.

- Consider a dedicated read-only `getbuildinfo` RPC or additional additive
  capability fields after downstream users have tested the new
  `getnetworkinfo.build` and `getnetworkinfo.build_commit` fields.
  Possible future fields: build channel, wallet migration support, messaging
  support, restricted assets/qualifiers support, rewards support, snapshot
  support, and enabled indexes.
  Scope: non-consensus RPC metadata only. Do not alter BIP14 subversion or peer
  protocol behavior.

## Wallet RPC

- Evaluate whether `loadwallet` and `unloadwallet` should be added for
  service workflows, or document clearly that this release uses daemon
  restart for wallet reload testing.
  Scope: wallet/RPC lifecycle behavior only; no consensus changes.

## Sync UX

- Consider a compact node-status RPC or clearer sync-status output for users,
  combining block height, header height, peer count, warnings, verification
  progress, and wallet availability.
  Scope: operational visibility only; no consensus changes.

## Pre-Release Hardening Candidates

- Add a CI/release validation pass before rebuilding artifacts:
  `make check`, selected functional tests, binary security checks, and warning
  scans should run as part of the release path. If GitHub Actions is kept for
  private pre-release work, it should not silently build release artifacts with
  weaker warning coverage than local validation.
  Scope: build and release process only; no runtime behavior changes.

- Evaluate `ReadMigrationEnvelopeRestoreData` restore flow to remove the double
  file read/decrypt while preserving current validation behavior.
  Scope: wallet migration RPC implementation only; no envelope format changes.

- Add or expand tests for wallet secret-export guarding so `getmywords`,
  `getmasterkeyinfo`, and `dumpwallet` behavior is covered both with and
  without `-allowwalletsecretexport`.
  Scope: test coverage only unless a regression is found.

- Evaluate high-volume asset/list RPC caps or pagination defaults without
  breaking explorer, WebCom, Commander, pool, or service workflows.
  Scope: RPC resource hardening only; no asset validation changes.

- Evaluate a channel-keyed message lookup path for Commander message RPCs.
  Current `viewchannelmessages` and `getmessagetxid` preserve the existing
  message database format and therefore load the wallet message set before
  filtering. A faster design would require a non-consensus message index or
  database helper keyed by channel and stable message identity. Do not change
  the message database layout silently in a maintenance release; design the
  migration and fallback behavior first.
  Scope: RPC/index performance only; no transaction, validation, consensus,
  wallet database, or network protocol changes.

- Add optional pagination and operator-configurable caps for Core address-index
  RPCs such as `getaddressutxos`, `getaddressdeltas`, `getaddresstxids`,
  `getaddressmempool`, and high-volume `getaddressbalance` asset queries.
  WebCom currently serves normal address reads from PostgreSQL with its own
  limits/cursors and does not directly call these Core RPCs, but direct Core
  consumers may depend on the current unlimited default. Preserve the old
  default until downstream users have a documented migration path.
  Scope: RPC resource hardening only; no index database or consensus changes.

- Review `UnitValueFromAmount` missing braces and missing-asset fallback behavior
  for a non-consensus, user-visible RPC display fix.
  Scope: RPC display behavior only; no asset accounting or validation changes.

- Review reward distribution payout math for floating-point usage. Only change
  before release if it is proven RPC-only and covered by tests.
  Scope: RPC transaction construction only unless proven otherwise.

- Review mining RPC safe-mode behavior. Only add `ObserveSafeMode()` to mining
  RPCs if the behavior is operationally correct for pools and cannot interfere
  with chain recovery, diagnostics, or existing service workflows.
  Scope: RPC availability policy only; no block validation changes.

## Deferred Protocol And Compatibility Design

- Asset transfer input/output conservation changes require chain-history review
  and height-gated activation.

- Qualifier add/remove cache or database repair requires a state-repair or
  reindex plan before any live change.

- `OP_HEMP_ASSET` semantics must remain unchanged unless activated through a
  formal protocol upgrade.

- `g_failed_blocks` bounding or pruning needs deeper validation review before
  release. It is a DoS-hardening idea, but invalid-block bookkeeping can affect
  header/block processing behavior and should not be changed without tests for
  invalid-chain descendants, reconsiderblock, reorgs, and node restart behavior.

- Burn addresses, prefixes, ports, genesis, PoW, subsidy, difficulty, asset
  activation, and other chainparams must not be changed in a maintenance
  hardening pass.

- BIP32 `"Bitcoin seed"` derivation must remain unchanged for wallet
  compatibility unless a separate migration design is prepared.

- BIP39 wallet.dat storage format redesign should be a separate wallet migration
  project, not part of this release candidate.
