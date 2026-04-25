Build instructions for Hemp0x
=================================

This will install most of the dependencies from ubuntu.
The only one we build, is Berkeley DB 4.8.


Ubuntu 21.10 - Impish Indri - Install dependencies:
---------------------------
`$ sudo apt install
build-essential
libssl-dev
libboost-chrono1.74-dev
libboost-filesystem1.74-dev
libboost-program-options1.74-dev
libboost-system1.74-dev
libboost-thread1.74-dev
libboost-test1.74-dev
bison
libevent-dev
libminiupnpc-dev
zlib1g-dev
libczmq-dev
autoconf
automake
libtool
`

Ubuntu 21.04 - Hirsute Hippo - Install dependencies:
----------------------------
`$ sudo apt install
build-essential
libssl-dev
libboost-chrono1.71-dev
libboost-filesystem1.71-dev
libboost-program-options1.71-dev
libboost-system1.71-dev
libboost-thread1.71-dev
libboost-test1.71-dev
bison
libevent-dev
libminiupnpc-dev
zlib1g-dev
libczmq-dev
autoconf
automake
libtool
`

Ubuntu 18.04 - Bionic Beaver - Install dependencies:
----------------------------
`$ sudo apt install
build-essential
libssl-dev
libboost-chrono-dev
libboost-filesystem-dev
libboost-program-options-dev
libboost-system-dev
libboost-thread-dev
libboost-test-dev
bison
libfreetype6-dev
libevent-dev
libminiupnpc-dev
zlib1g-dev
libczmq-dev
autoconf
automake
libtool
`

Directory structure
------------------
Hemp0x sources in `$HOME/src`

Berkeley DB will be installed to `$HOME/src/db4`


Hemp0x
------------------

Start in $HOME

Make the directory for sources and go into it.

`mkdir src`

`cd src`

__Download Hemp0x source.__

`git clone https://github.com/hemp0x/hemp0x-core`

`cd hemp0x-core`

`git checkout develop` # this checks out the develop branch.

__Download and build Berkeley DB 4.8__

`contrib/install_db4.sh ../`

__The build process:__

`./autogen.sh`

`export BDB_PREFIX=$HOME/src/db4`

`./configure --without-gui BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include" --prefix=/usr/local`

_Adjust to own needs. This will install the binaries to `/usr/local/bin`_


`make -j8`  # 8 for 8 build threads, adjust to fit your setup.

hemp0xd and hemp0x-cli are in `src/`


__Optional:__

`sudo make install`  # if you want to install the binaries to /usr/local/bin (if this prefix was used above).
