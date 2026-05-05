Hemp0x Core integration/staging tree

https://hemp0x.com

To see how to run Hemp0x, please read the respective files in the doc folder.

What is Hemp0x?

Hemp0x is an experimental proof of work digital currency and asset network designed for peer to peer value transfer and native asset issuance.

In addition to enabling payments, the Hemp0x platform allows anyone to create assets directly on the blockchain without smart contracts or centralized intermediaries. Assets may represent tokens, NFTs, commodities, products, access rights, gift cards, or other forms of transferable value.

Hemp0x uses peer to peer technology to operate with no central authority. Transaction validation, asset issuance, and monetary supply are enforced by consensus rules and carried out collectively by miners and node operators.

Hemp0x follows the original Bitcoin ethos of fair launch, open participation, censorship resistance, and permissionless access. There is no premine, no developer allocation, and no controlling entity.

License

Hemp0x Core is released under the terms of the MIT license. See COPYING for more
information or see https://opensource.org/licenses/MIT.

Development Process

The master branch is regularly built and tested but is not guaranteed to be completely stable.
Tags are created to indicate official stable release versions of Hemp0x Core.

Active development is done on reviewed topic and release branches.

The contribution workflow is described in CONTRIBUTING.md.

Developer IRC is inactive. Please join us on Discord in #development.
https://discord.gg/Eu4UsYPMGS

Testing

Testing and code review is a limiting factor for development. Please be patient and help out by testing other people’s pull requests. This is a security critical project where any mistake may result in loss of funds or asset integrity.

Testnet is up and running and available to use during development.

Automated Testing

Developers are strongly encouraged to write unit tests for new code and to submit new unit tests for existing code. Unit tests can be compiled and run assuming they were not disabled in configure with make check.

Further details on running and extending unit tests can be found in /src/test/README.md.

There are also regression and integration tests written in Python that are run automatically on the build server. These tests can be run if the test dependencies are installed with test/functional/test_runner.py.

Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the code. This is especially important for large or high risk changes. If testing is not straightforward it is recommended to include a test plan in the pull request description.

About Hemp0x

Hemp0x is a decentralized proof-of-work blockchain focused on payments, native
asset issuance, and asset transfer. It preserves the UTXO security model while
adding Hemp0x network parameters, 5-second block targets, KAWPOW proof of work,
and asset functionality.

Hemp0x is free and open source. It launched without a premine, developer
allocation, or protocol-level tax. The project is built on work from the Bitcoin
and Ravencoin open source communities and keeps the MIT license notices required
by that history.

Core release builds provide the daemon, RPC client, and offline transaction
utility. The old bundled Qt wallet GUI is not part of Core Next; external
applications such as Hemp0x Commander and WebCom use the daemon and RPC
interface.
