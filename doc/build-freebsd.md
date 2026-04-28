Build instructions for Hemp0x 
=================================
FreeBSD 13.0
---------------------------------
This will install most of the dependencies from FreeBSD pkg.

The only one we build, is Berkeley DB 4.8.


Install dependencies:
----------------------------
`# pkg install autoconf automake boost-libs git gmake libevent libtool pkgconf openssl
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

`cd Hemp0x`

`git checkout develop` # this checks out the develop branch.

__Download and build Berkeley DB 4.8__

`contrib/install_db4.sh ../`

__The build process:__

`./autogen.sh`

This is for `sh` or `bash`. 

`export BDB_PREFIX=$HOME/src/db4`

`./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include" CFLAGS="-fPIC" CXXFLAGS="-fPIC -I/usr/local/include" --prefix=/usr/local MAKE=gmake`

_Adjust to own needs. `--prefix=/usr/local` will install the binaries to `/usr/local/bin`_


`gmake -j8`  # 8 for 8 build threads, adjust to fit your setup.

hemp0xd and hemp0x-cli are in `src/`


__Optional:__

`make install`  # if you want to install the binaries to /usr/local/bin.




