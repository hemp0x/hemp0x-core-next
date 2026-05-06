Release Process
====================

Before every release candidate:

* Update manpages, see [gen-manpages.sh](https://github.com/hemp0x/hemp0x-core/blob/master/contrib/devtools/README.md#gen-manpagessh).

Before every minor and major release:

* Update [bips.md](bips.md) to account for changes since the last release.
* Update version in `configure.ac` (don't forget to set `CLIENT_VERSION_IS_RELEASE` to `true`)
* Write release notes (see below)
* Update `src/chainparams.cpp` nMinimumChainWork with information from the getblockchaininfo rpc.
* Update `src/chainparams.cpp` defaultAssumeValid  with information from the getblockhash rpc.
  - The selected value must not be orphaned so it may be useful to set the value two blocks back from the tip.
  - Testnet should be set some tens of thousands back from the tip due to reorgs there.
  - This update should be reviewed with a reindex-chainstate with assumevalid=0 to catch any defect
     that causes rejection of blocks in the past history.

Before every major release:

* Update hardcoded [seeds](/contrib/seeds/README.md), see [this pull request](https://github.com/bitcoin/bitcoin/pull/7415) for an example.
* Update `src/chainparams.cpp` chainTxData with statistics about the transaction count and rate.

### First time / New builders


    git clone https://github.com/hemp0x/hemp0x-core.git

### Hemp0x maintainers/release engineers, suggestion for writing release notes

Write release notes. git shortlog helps a lot, for example:

    git shortlog --no-merges v(current version, e.g. 4.6.0)..v(new version, e.g. 4.7.0)

Generate list of authors:

    git log --format='%aN' "$*" | sort -ui | sed -e 's/^/- /'

Tag version (or release candidate) in git

    git tag -s v(new version, e.g. 4.7.0)


### Build binaries

Build release artifacts from a clean, signed tag on a dedicated builder. Release
builds should use the default optimized configuration and must not enable debug
flags unless producing separate developer diagnostics. Do not publish Qt wallet
installers or local mining binaries.

Recommended checks for each release build:

```bash
./autogen.sh
./configure --enable-reduce-exports
make -j"$(nproc)"
make check
make -C src check-security
make -C src check-symbols
```

Package only the expected daemon, CLI, transaction utility, supporting runtime
libraries where needed, license, and README files. Strip release binaries or use
the release packaging scripts that strip artifacts during staging.

### After binaries are built:

- Create `SHA256SUMS.asc` for the builds, and GPG-sign it:

```bash
sha256sum * > SHA256SUMS
```

The list of files should be:
```
hemp0x-${VERSION}.tar.gz
hemp0x-${VERSION}-x86_64-linux-gnu.tar.gz
hemp0x-${VERSION}-win64.zip
```

Community builds may also produce ARM Linux or other platform packages, but
Linux and Windows are the validated release platforms for Core Next. Do not
publish Qt wallet installers or local mining binaries.

- GPG-sign it, delete the unsigned file:
```
gpg --digest-algo sha256 --clearsign SHA256SUMS # outputs SHA256SUMS.asc
rm SHA256SUMS
```
(the digest algorithm is forced to sha256 to avoid confusion of the `Hash:` header that GPG adds with the SHA256 used for the files)
Note: check that SHA256SUMS itself doesn't end up in SHA256SUMS, which is a spurious/nonsensical entry.

- Upload release archives and `SHA256SUMS.asc` from the last step to the GitHub release page.

- Update hemp0x.com version

- Announce the release:

  - hemp0x.com blog post

  - Optionally twitter, reddit /r/Hemp0x, ... but this will usually sort out itself

  - Archive release notes for the new version to `doc/release-notes/` (branch `master` and branch of the release)

  - Create a [new GitHub release](https://github.com/hemp0x/hemp0x-core/releases/new) with a link to the archived release notes.

  - Monitor network health, support channels, and issue reports after publication.
