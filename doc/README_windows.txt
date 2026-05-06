Hemp0x Core
=============

Intro
-----
Hemp0x is a free open source peer-to-peer electronic cash system that is
completely decentralized, without the need for a central server or trusted
parties.  Users hold the crypto keys to their own money and transact directly
with each other, with the help of a P2P network to check for double-spending.


Setup
-----
Unpack the files into a directory and run hemp0xd.exe, or control a running
daemon with hemp0x-cli.exe. The Windows package contains the server and
command-line tools; the old bundled Qt wallet GUI is not part of Core Next
release builds.

Hemp0x Core is the original Hemp0x client and it builds the backbone of the network.
However, it downloads and stores the entire history of Hemp0x transactions;
depending on the speed of your computer and network connection, the synchronization
process can take anywhere from a few hours to a day or more.

For build and configuration help, see the documentation in the doc directory:
  build-windows.md
  hemp0x-conf.md
  wallet-migration.md
