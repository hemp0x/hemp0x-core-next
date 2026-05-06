Contributing to Hemp0x Core
===========================

Hemp0x Core is security-critical software. Small, well-tested contributions are
easier to review and safer to merge than large mixed changes. Testing, code
review, documentation fixes, build reports, and focused patches are all useful.

Contribution Flow
-----------------

Use the normal GitHub pull request workflow:

1. Fork the repository.
2. Create a topic branch for one focused change.
3. Build and test the change.
4. Open a pull request with a clear summary and test notes.

Pull requests should target the active development branch unless a maintainer
asks for a different base.

What To Include
---------------

A useful pull request includes:

- a short description of the problem or improvement
- the reason for the chosen approach
- the operating system and compiler used for testing
- commands run and whether they passed
- screenshots or logs when they help explain behavior
- migration notes for operators when behavior changes

Keep unrelated formatting, cleanup, and behavior changes in separate pull
requests. Mechanical rewrites make review harder unless they are isolated.

Consensus Safety
----------------

Most changes should not alter consensus behavior. Be especially careful with:

- block and transaction validation
- proof-of-work validation
- difficulty adjustment
- timestamps and median-time-past rules
- subsidy and fee rules
- asset consensus rules
- genesis data, network magic, ports, and address prefixes

Any consensus-sensitive proposal must be clearly labeled, designed separately,
reviewed carefully, and coordinated with the network before it can be considered
for a production release.

Compatibility
-------------

Hemp0x Core is used by node operators, wallets, explorers, indexers, pool
software, WebCom services, and Hemp0x Commander. Pull requests should preserve
existing daemon, CLI, wallet RPC, mining-pool RPC, asset RPC, REST, and ZMQ
behavior unless a breaking change is explicitly proposed and documented.

Builds
------

Use the tracked depends system when possible:

```bash
contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
```

For Windows release validation from Linux:

```bash
contrib/build-hemp0x-core.sh --target windows --with-tx --run-tests
```

See:

- [Linux build guide](doc/build-linux.md)
- [Windows cross-build guide](doc/build-windows.md)

Testing
-------

Run the tests that match the risk of the change. Common checks are:

```bash
make check
make -C src check-security
make -C src check-symbols
test/functional/test_runner.py --jobs=4
```

For focused changes, running the relevant functional test is acceptable, but
the pull request should say exactly what was run. Changes that touch shared
behavior, networking, wallet code, RPC, assets, or build tooling should include
broader validation.

Commit Style
------------

Use clear commit subjects with a component prefix when helpful:

```text
build: improve Linux package detection
net: bound request tracking memory use
rpc: add node status summary
wallet: validate migration envelope metadata
docs: clarify Windows cross-build setup
```

Prefer small commits that can be reviewed independently. Explain why the change
is needed when the reason is not obvious from the diff.

Review
------

Reviewers may use common review shorthand:

- `ACK`: reviewed and tested, looks good
- `utACK`: reviewed, not tested
- `Concept ACK`: agree with the idea, implementation still needs review
- `NACK`: should not merge; include a technical reason
- `nit`: small non-blocking issue

Please be patient with review. A slow review usually means the change needs
more time, more testing, or more focused explanation.

Licensing
---------

By contributing to this repository, you agree to license your contribution
under the MIT license unless the file already states a different compatible
license. Do not remove existing copyright or license notices. If you import
work from another project, preserve the required notices and identify the
source.
