#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: contrib/build-hemp0x-core.sh [options]

Build Hemp0x Core from a source checkout without installing binaries or
touching node data directories.

Options:
  --target linux       Build native Linux binaries (default).
  --target windows    Cross-build 64-bit Windows binaries with MinGW-w64.
  --with-tx           Build hemp0x-tx in addition to hemp0xd and hemp0x-cli.
  --with-libs         Also build libhemp0xconsensus for developers.
  --run-tests         Run make check after the build.
  --skip-depends      Reuse an existing depends build.
  --reuse-build       Reuse current configure/build state when possible.
  --clean             Accepted for compatibility; clean builds are the default.
  --debug             Build with debug symbols and do not strip binaries.
  --jobs N            Parallel build jobs (default: nproc or 2).
  --help              Show this help.

Examples:
  contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
  contrib/build-hemp0x-core.sh --target windows --with-tx
EOF
}

die() {
    echo "error: $*" >&2
    exit 1
}

have() {
    command -v "$1" >/dev/null 2>&1
}

repo_root() {
    git rev-parse --show-toplevel 2>/dev/null || pwd
}

package_hint() {
    local target="$1"

    echo
    echo "Install the missing build tools and rerun this script." >&2
    if have apt-get; then
        if [ "$target" = "windows" ]; then
            cat >&2 <<'EOF'
Ubuntu/Debian example:
  sudo apt update
  sudo apt install -y build-essential autoconf automake libtool pkg-config \
    bsdmainutils curl python3 gawk ca-certificates \
    gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix \
    binutils-mingw-w64-x86-64 mingw-w64

Then select the POSIX MinGW variant if alternatives are present:
  sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
  sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
EOF
        else
            cat >&2 <<'EOF'
Ubuntu/Debian example:
  sudo apt update
  sudo apt install -y build-essential autoconf automake libtool pkg-config \
    bsdmainutils curl python3 gawk ca-certificates
EOF
        fi
    elif have dnf; then
        if [ "$target" = "windows" ]; then
            cat >&2 <<'EOF'
Fedora/Nobara example:
  sudo dnf install -y gcc gcc-c++ make autoconf automake libtool pkgconf \
    curl python3 gawk diffutils patch findutils mingw64-gcc mingw64-gcc-c++
EOF
        else
            cat >&2 <<'EOF'
Fedora/Nobara example:
  sudo dnf install -y gcc gcc-c++ make autoconf automake libtool pkgconf \
    curl python3 gawk diffutils patch findutils
EOF
        fi
    else
        echo "Install a C/C++ compiler, make, autoconf, automake, libtool, pkg-config, curl, python3, gawk, patch, and the MinGW-w64 POSIX toolchain for Windows builds." >&2
    fi
}

require_tools() {
    local target="$1"
    local missing=()
    local tools=(make git curl python3 gawk sed grep patch autoconf automake libtoolize pkg-config hexdump)

    for tool in "${tools[@]}"; do
        have "$tool" || missing+=("$tool")
    done

    if [ "$target" = "windows" ]; then
        for tool in x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++ x86_64-w64-mingw32-ar x86_64-w64-mingw32-ranlib x86_64-w64-mingw32-strip; do
            have "$tool" || missing+=("$tool")
        done
    else
        for tool in gcc g++; do
            have "$tool" || missing+=("$tool")
        done
    fi

    if [ "${#missing[@]}" -ne 0 ]; then
        echo "Missing tools: ${missing[*]}" >&2
        package_hint "$target"
        exit 1
    fi
}

require_mingw_posix() {
    local model
    model="$(x86_64-w64-mingw32-g++ -v 2>&1 | sed -n 's/^Thread model: //p' | tail -1)"
    if [ "$model" != "posix" ]; then
        echo "Detected MinGW thread model: ${model:-unknown}" >&2
        echo "Hemp0x Core requires the POSIX MinGW-w64 variant for std::thread and condition_variable support." >&2
        package_hint windows
        exit 1
    fi
}

strip_binary() {
    local target="$1"
    local file="$2"

    [ -f "$file" ] || return 0
    if [ "$target" = "windows" ]; then
        x86_64-w64-mingw32-strip "$file"
    else
        strip "$file"
    fi
}

configured_host() {
    [ -f config.log ] || return 0
    sed -n 's/^host='\''\(.*\)'\''$/\1/p' config.log | tail -1
}

target=linux
with_tx=0
with_libs=0
run_tests=0
skip_depends=0
clean=1
debug=0
jobs="$(nproc 2>/dev/null || echo 2)"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --target)
            shift
            target="${1:-}"
            ;;
        --target=*)
            target="${1#*=}"
            ;;
        --with-tx)
            with_tx=1
            ;;
        --with-libs)
            with_libs=1
            ;;
        --run-tests)
            run_tests=1
            ;;
        --skip-depends)
            skip_depends=1
            ;;
        --reuse-build)
            clean=0
            ;;
        --clean)
            clean=1
            ;;
        --debug)
            debug=1
            ;;
        --jobs)
            shift
            jobs="${1:-}"
            ;;
        --jobs=*)
            jobs="${1#*=}"
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
    shift
done

[ "$target" = "linux" ] || [ "$target" = "windows" ] || die "--target must be linux or windows"
case "$jobs" in
    ''|*[!0-9]*) die "--jobs must be a positive integer" ;;
esac

ROOT="$(repo_root)"
cd "$ROOT"

require_tools "$target"
[ "$target" = "windows" ] && require_mingw_posix

if [ ! -x ./configure ]; then
    ./autogen.sh
fi

if [ "$clean" -eq 1 ]; then
    make distclean || true
fi

host_triplet="x86_64-pc-linux-gnu"
depends_args=()
configure_args=(--enable-reduce-exports)
output_files=(src/hemp0xd src/hemp0x-cli)

if [ "$target" = "windows" ]; then
    host_triplet="x86_64-w64-mingw32"
    depends_args=(HOST="$host_triplet")
    configure_args+=(--host="$host_triplet")
    output_files=(src/hemp0xd.exe src/hemp0x-cli.exe)
fi

previous_host="$(configured_host)"
if [ -n "$previous_host" ] && [ "$previous_host" != "$host_triplet" ]; then
    echo "Previous configure host was $previous_host; cleaning before $host_triplet build."
    make distclean || true
fi

if [ "$with_tx" -eq 1 ]; then
    configure_args+=(--with-tx)
    if [ "$target" = "windows" ]; then
        output_files+=(src/hemp0x-tx.exe)
    else
        output_files+=(src/hemp0x-tx)
    fi
fi

if [ "$with_libs" -eq 1 ]; then
    configure_args+=(--with-libs)
else
    configure_args+=(--without-libs)
fi

if [ "$debug" -eq 1 ]; then
    configure_args+=(--enable-debug)
else
    configure_args+=(--disable-bench)
fi

if [ "$skip_depends" -eq 0 ]; then
    make -C depends "${depends_args[@]}" -j"$jobs"
fi

config_site="$ROOT/depends/$host_triplet/share/config.site"
[ -f "$config_site" ] || die "missing depends config.site: $config_site"

CONFIG_SITE="$config_site" ./configure "${configure_args[@]}"
make -j"$jobs"

if [ "$debug" -eq 0 ]; then
    for file in "${output_files[@]}"; do
        strip_binary "$target" "$file"
    done
fi

if [ "$run_tests" -eq 1 ]; then
    if [ "$target" = "windows" ]; then
        make -C src check-security
    else
        make check
        make -C src check-security
        make -C src check-symbols
    fi
fi

echo
echo "Built Hemp0x Core $target binaries:"
for file in "${output_files[@]}"; do
    if [ -f "$file" ]; then
        ls -lh "$file"
    else
        echo "missing: $file" >&2
        exit 1
    fi
done
