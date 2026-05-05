Dependencies
============

These are the dependencies currently used by Hemp0x Core. You can find instructions for installing them in the `build-*.md` file for your platform.

| Dependency | Version used | Minimum required | CVEs | Shared |
| --- | --- | --- | --- | --- |
| Berkeley DB | [4.8.30](http://www.oracle.com/technetwork/database/database-technologies/berkeleydb/downloads/index.html) | 4.8.x | No |  |
| Boost | [1.90.0](http://www.boost.org/users/download/) | [1.58.0](https://www.boost.org/) | No |  |
| ccache | [3.3.4](https://ccache.samba.org/download.html) |  | No |  |
| Clang | [11.0.1](http://llvm.org/releases/download.html) |  (C++11 support) |  |  |
| Expat | System or native package |  | Yes | Yes |
| GCC |  | [7+](https://gcc.gnu.org/) |  |  |
| libevent | [2.1.12-stable](https://github.com/libevent/libevent/releases) | 2.0.22 | No |  |
| LevelDB | [1.23](https://github.com/google/leveldb/releases) | Vendored | No |  |
| macOS packaging helpers | Pinned in `depends/packages/native_*.mk` | macOS release builds |  |  |
| MiniUPnPc | [2.3.3](http://miniupnp.free.fr/files) |  | No |  |
| OpenSSL | [3.5.6 LTS](https://www.openssl.org/source) | 3.5.x | Yes |  |
| Python (tests) |  | [3.6](https://www.python.org/downloads) |  |  |
| ZeroMQ | [4.3.5](https://github.com/zeromq/libzmq/releases) |  | No |  |
| zlib | System |  |  | No |
