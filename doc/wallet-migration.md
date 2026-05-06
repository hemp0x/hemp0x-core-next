Hemp0x Wallet Migration
=======================

Hemp0x Core Next includes wallet migration RPCs for moving compatible legacy
wallet data into a modern encrypted migration envelope and restoring that
envelope into a new wallet.

These RPCs are non-consensus wallet tools. They do not change chain rules,
network parameters, addresses, proof of work, assets, or transaction validation.

Overview
--------

The migration flow has three commands:

- `exportwalletmigration`: writes a migration envelope to disk
- `validatewalletmigration`: checks an envelope without creating a wallet
- `restorewalletmigration`: restores a new wallet from an encrypted private
  envelope

There are two envelope forms:

- public-only envelopes, for metadata and compatibility checks
- encrypted private envelopes, for restoring a wallet

Only encrypted private envelopes can be restored. Public-only envelopes are
intentionally not restorable because they do not contain the mnemonic/private
material needed to recreate wallet keys.

Private exports are supported for canonical Hemp0x BIP39/BIP44 coin type 420
wallets. Older or non-standard wallets may need to remain on legacy `wallet.dat`
handling until a separate migration path is designed.

Before Exporting
----------------

Back up the existing wallet before testing migration:

```bash
./src/hemp0x-cli backupwallet "/home/user/hemp0x-wallet-backup.dat"
```

If the wallet is encrypted, unlock it briefly before a private export:

```bash
./src/hemp0x-cli walletpassphrase "your-wallet-passphrase" 300
```

Lock it again after the export:

```bash
./src/hemp0x-cli walletlock
```

Public-Only Export
------------------

A public-only envelope is useful for checking metadata. It cannot be restored.

```bash
./src/hemp0x-cli exportwalletmigration \
  "/home/user/hemp0x-wallet-migration-public.json" \
  false \
  false
```

Arguments:

1. output file path
2. `include_private` set to `false`
3. `allow_overwrite` set to `false`

Use `true` for `allow_overwrite` only when replacing an existing export file is
intentional.

Encrypted Private Export
------------------------

An encrypted private envelope can be restored into a new wallet.

```bash
./src/hemp0x-cli exportwalletmigration \
  "/home/user/hemp0x-wallet-migration-private.json" \
  true \
  false \
  "use-a-long-unique-export-passphrase"
```

Arguments:

1. output file path
2. `include_private` set to `true`
3. `allow_overwrite` set to `false`
4. export passphrase

The export passphrase protects the migration envelope. Store it separately from
the exported file. Without this passphrase, the encrypted private envelope
cannot be restored.

Validate an Envelope
--------------------

Validate a public-only envelope:

```bash
./src/hemp0x-cli validatewalletmigration \
  "/home/user/hemp0x-wallet-migration-public.json"
```

Validate an encrypted private envelope:

```bash
./src/hemp0x-cli validatewalletmigration \
  "/home/user/hemp0x-wallet-migration-private.json" \
  "use-a-long-unique-export-passphrase"
```

Validation reports metadata and whether the envelope is restorable. It does not
create or modify a wallet.

Restore a Wallet
----------------

Restore creates a new wallet directory under the node data directory. It does
not overwrite an existing wallet.

```bash
./src/hemp0x-cli restorewalletmigration \
  "/home/user/hemp0x-wallet-migration-private.json" \
  "restored_migration_wallet" \
  "use-a-long-unique-export-passphrase"
```

Arguments:

1. encrypted private envelope path
2. new wallet name
3. export passphrase

The optional fourth argument is `birth_height`, which controls where the rescan
starts:

```bash
./src/hemp0x-cli restorewalletmigration \
  "/home/user/hemp0x-wallet-migration-private.json" \
  "restored_migration_wallet" \
  "use-a-long-unique-export-passphrase" \
  2500000
```

After restore, check the wallet:

```bash
./src/hemp0x-cli listwallets
./src/hemp0x-cli -rpcwallet=restored_migration_wallet getwalletinfo
./src/hemp0x-cli -rpcwallet=restored_migration_wallet getnewaddress
```

To start the daemon with the restored wallet as the selected wallet:

```bash
./src/hemp0x-cli stop
./src/hemp0xd -daemon -wallet=restored_migration_wallet
./src/hemp0x-cli getwalletinfo
```

Current Limitations
-------------------

Core Next does not currently include dynamic `loadwallet` or `unloadwallet`
RPCs. Restored wallets are loaded by the restore RPC for the current process,
and they can be selected on restart with `-wallet=<wallet_name>`.

Adding dynamic wallet load and unload support is possible, but it should be
handled as a separate wallet lifecycle change with its own tests. It touches
wallet registration, rescans, RPC routing, shutdown behavior, and multiwallet
edge cases, so it is better treated as a focused project rather than folded
into the migration RPC documentation pass.

Security Notes
--------------

- Keep a backup of the original `wallet.dat` until the restored wallet has been
  tested and confirmed.
- Use a long, unique export passphrase.
- Store the export file and passphrase separately.
- Delete migration files that are no longer needed.
- Encrypt restored wallets before long-term use:

```bash
./src/hemp0x-cli -rpcwallet=restored_migration_wallet encryptwallet \
  "new-restored-wallet-passphrase"
```

After encrypting a wallet, restart `hemp0xd` before using it again.
