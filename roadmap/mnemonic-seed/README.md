# Mnemonic Seed

For all cryptocurrencies and crypto-assets, the greatest difficulty is securing your private keys.  

There have been evolutions over the last ten years and it is about as close to the final solution as it can get provided that crypto owner holds the keys.

A marvelous solution is to create a random seed from which all other keys can be generated.  For an overview of how this works read Seeds of Freedom:  
(https://medium.com/@tronblack/hemp0x-seeds-of-freedom-a3a3ff0fa1)

In an effort to bring ease-of-use and interoperability between Core, Commander, and WebCom, Hemp0x wallet tooling should support seed-backed recovery workflows. A mnemonic seed is a 128 bit random number that is run through HMAC-512 hashing algorithm to produce a master key. The main advantage of starting with a 12-word seed is the ease of backing up the wallet.

Core Next still preserves `wallet.dat` compatibility. Future seed or vault tooling should use BIP44-compatible derivation.

A command-line option such as `-noseed` may be useful if a future seed-backed default wallet is added and operators need to create a legacy-format wallet explicitly.

When using a 12-word seed, address derivation should be compatible with the BIP32/BIP39/BIP44 standards. The canonical Hemp0x BIP44 coin type is 420, so the first non-change address path is `m/44'/420'/0'/0/0`.

The advantage to this method of key generation is that a 12-word seed can be written down, or stamped into stainless steel and safely stored offline without the risk of a thumb-drive or hard drive backup failure.

Also, the same 12-word seed can be entered into an online or offline wallet and transactions signed using the derived keys, allowing asset support while keeping keys under the owner's control.

### Technical
If seed support is added to Core wallet storage, the seed should be stored in encrypted form when wallet encryption is enabled and the master key should be derived on demand.

For those the never import a private key into the wallet.dat, safely storing the 12-words is sufficient backup.

For the case when a private key is imported in the core client's wallet.dat, a warning should be presented that the 12-word mnemonic backup is now insufficient, and the wallet.dat should be backed up.  The reason is that the imported key is added to a list of keys and cannot be derived from the 12-word seed, so any funds sent to the address(es) for the imported key(s) would be lost in the case of wallet.dat being lost or corrupted.

When creating a seed-backed wallet for the first time, the default should generate 128 bits of entropy and store either the words or the 132 bits, which includes a 4 bit checksum. If a legacy option is set via command-line or `hemp0x.conf`, the original wallet key generation path should be used.

The path derivation should be dependent on the way the master key is generated: 
* Original Master Key:  BIP32 m/0'/0' (external) or m/0'/1' (internal)
* Seed-based master key (BIP39): BIP32/BIP44 `m/44'/420'/0'/0`

This change does not require a hard fork (upgrade), but it does require maintaining 100% compatibility with the old derivation path when the original master key is in the wallet.dat or it will appear to users that funds are lost.  Only new users, or those that start with a new wallet.dat will be switched over to the 12-word seed.

Optional: In order to back up the master key and chaincode, it requires 48 words.

### Compatibility
For wallets that use BIP39/BIP32/BIP44 and the correct coin type of 420 for Hemp0x, the 12 words should be compatible with external Hemp0x-aware tooling.

Because the amount of HEMP in the asset UTXO is 0, and because the Hemp0x transaction will be invalid if the asset outputs don't match the asset inputs, this prevents external wallets from being able to lose assets even though the external wallets are completely unaware of assets.
