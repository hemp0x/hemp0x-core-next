Repository Tools
---------------------

### [Developer tools](/contrib/devtools) ###
Specific tools for developers working on this repository.
Contains the script `github-merge.py` for merging GitHub pull requests securely and signing them using GPG.

### [Verify-Commits](/contrib/verify-commits) ###
Tool to verify that every merge commit was signed by a developer using the above `github-merge.py` script.

### [Linearize](/contrib/linearize) ###
Construct a linear, no-fork, best version of the blockchain.

### [Qos](/contrib/qos) ###

A Linux bash script that will set up traffic control (tc) to limit the outgoing bandwidth for connections to the Hemp0x network. This means one can have an always-on hemp0xd instance running and another local hemp0xd instance which connects to this node and receives blocks from it.

### [Seeds](/contrib/seeds) ###
Utility to generate the pnSeed[] array that is compiled into the client.

Build Tools and Keys
---------------------

### [Debian](/contrib/debian) ###
Contains files used to package hemp0xd, hemp0x-cli, and hemp0x-tx
for Debian-based Linux systems.

### [Gitian-descriptors](/contrib/gitian-descriptors) ###
Legacy deterministic-build descriptors retained for historical reference.

### [Gitian-keys](/contrib/gitian-keys)
PGP keys historically used for signing deterministic release results.

### [macOS SDK](/contrib/macdeploy) ###
Mac SDK extraction helper for cross builds.

### [RPM](/contrib/rpm) ###
RPM spec file for building hemp0x-core on RPM based distributions.

### [Gitian-build](/contrib/gitian-build.sh) ###
Legacy script for historical deterministic builds.

Test and Verify Tools 
---------------------

### [TestGen](/contrib/testgen) ###
Utilities to generate test vectors for the data-driven Hemp0x tests.

### [Verify Binaries](/contrib/verifybinaries) ###
This script attempts to download and verify the signature file SHA256SUMS.asc from hemp0x.org.
