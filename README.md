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
Tags are created regularly to indicate new official stable release versions of Hemp0x Core.

Active development is done in the develop branch.

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

Hemp0x is a decentralized peer to peer blockchain focused on the issuance, transfer, and verification of assets.

At its core Hemp0x exists to solve a fundamental problem: determining ownership and enabling trustless transfer of that ownership without reliance on centralized systems.

Hemp0x is designed for real world asset use cases including agriculture, farming, hemp and cannabis products, supply chain tracking, seed to sale verification, commodities, retail and ecommerce assets, peer to peer exchange, and digital collectibles.

By keeping asset creation and transfer native to the protocol, Hemp0x avoids reliance on smart contracts, custodians, bridges, or external databases.

Thank you to the Bitcoin developers.

The Hemp0x project is built on the work of the Bitcoin and Ravencoin open source communities. Hundreds of developers and thousands of contributors have spent years building secure, censorship resistant, peer to peer systems.

Hemp0x exists because of this foundation and seeks to extend it while preserving the principles that made Bitcoin successful.

Abstract

Hemp0x aims to implement a blockchain optimized for the transfer of assets from one holder to another.

Based on the extensive development and testing of Bitcoin and Ravencoin, Hemp0x introduces faster block times while preserving the proven UTXO security model. Hemp0x is free and open source and is issued and mined transparently with no premine, no developer allocation, and no protocol level taxes.

The project prioritizes user control, privacy, censorship resistance, and jurisdictional neutrality while allowing optional application level features where needed.

A blockchain is a ledger that records ownership of value and enables that value to be transferred. One of the most fundamental applications of blockchain technology is determining who owns what in a verifiable and tamper resistant way. This is why the first and most successful blockchain use case to date has been Bitcoin.

The success of token systems such as Ethereum ERC20 demonstrates demand for tokenized assets. Tokens offer advantages over traditional systems including faster settlement, reduced reliance on trusted intermediaries, increased transparency, and global accessibility.

Bitcoin can also serve as rails for token systems through overlay projects such as Omnilayer, RSK, or Counterparty. However, neither Bitcoin nor Ethereum was originally designed with native asset issuance as a primary focus.

Hemp0x is designed as a use case specific blockchain focused on one function: the secure and censorship resistant transfer of assets.

Bitcoin is and should remain focused on being sound decentralized money. It is unlikely that Bitcoin development will prioritize features that are specifically beneficial to asset issuance. One goal of the Hemp0x project is to explore whether a dedicated asset focused blockchain can provide advantages for real world ownership, commerce, and supply chain use cases.

In a global economy borders and jurisdictions become less relevant as assets move across networks instantly. As people can already transfer wealth globally using Bitcoin, they will increasingly demand the same efficiency for products, commodities, and other asset holdings.

For such a system to function fairly, the underlying rails must be censorship resistant and jurisdiction agnostic. This is not ideological but practical. Protocol level controls introduce jurisdictional conflicts and centralized points of failure. By remaining neutral, permissionless, and decentralized, Hemp0x provides open infrastructure that anyone can use without approval or trust.
