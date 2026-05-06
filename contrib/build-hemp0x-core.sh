#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  contrib/build-hemp0x-core.sh
  contrib/build-hemp0x-core.sh [options]

Run without options for an interactive build menu. The script builds from the
current source checkout without installing binaries or touching node data.

Options:
  --interactive       Force the interactive menu.
  --target linux      Build native Linux binaries.
  --target windows    Cross-build 64-bit Windows binaries with MinGW-w64.
  --update            Fetch and fast-forward the current branch before building.
  --with-tx           Build hemp0x-tx in addition to hemp0xd and hemp0x-cli.
  --with-libs         Also build libhemp0xconsensus for developers.
  --run-tests         Run build checks after compiling.
  --skip-depends      Reuse an existing depends build.
  --reuse-build       Reuse current configure/build state when possible.
  --clean             Clean builds are the default; kept for compatibility.
  --debug             Use the developer/debug profile and do not strip binaries.
  --jobs N            Parallel build jobs. Defaults to the detected CPU count.
  --help              Show this help.

Examples:
  contrib/build-hemp0x-core.sh
  contrib/build-hemp0x-core.sh --target linux --with-tx --run-tests
  contrib/build-hemp0x-core.sh --target windows --with-tx --run-tests
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

ask_yes_no() {
    local prompt="$1"
    local default="${2:-yes}"
    local suffix answer

    case "$default" in
        yes) suffix="[Y/n]" ;;
        no) suffix="[y/N]" ;;
        *) suffix="[y/n]" ;;
    esac

    while true; do
        read -r -p "$prompt $suffix " answer
        answer="${answer:-$default}"
        case "$answer" in
            y|Y|yes|YES|Yes) return 0 ;;
            n|N|no|NO|No) return 1 ;;
        esac
        echo "Please answer yes or no."
    done
}

apt_packages() {
    local target="$1"
    local packages=(
        build-essential autoconf automake libtool pkg-config bsdmainutils
        curl python3 gawk ca-certificates
    )

    if [ "$target" = "windows" ]; then
        packages+=(
            gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix
            binutils-mingw-w64-x86-64 mingw-w64
        )
    fi

    printf '%s\n' "${packages[@]}"
}

dnf_packages() {
    local target="$1"
    local packages=(
        gcc gcc-c++ make autoconf automake libtool pkgconf curl python3
        gawk diffutils patch findutils util-linux
    )

    if [ "$target" = "windows" ]; then
        packages+=(mingw64-gcc mingw64-gcc-c++)
    fi

    printf '%s\n' "${packages[@]}"
}

package_hint() {
    local target="$1"

    echo
    echo "Install the missing build tools and rerun this script." >&2
    if have apt-get; then
        echo "Ubuntu/Debian example:" >&2
        echo "  sudo apt update" >&2
        printf '  sudo apt install -y' >&2
        apt_packages "$target" | while read -r package; do printf ' %s' "$package" >&2; done
        echo >&2
        if [ "$target" = "windows" ]; then
            cat >&2 <<'EOF'

Then select the POSIX MinGW variant if alternatives are present:
  sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
  sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
EOF
        fi
    elif have dnf; then
        echo "Fedora/Nobara example:" >&2
        printf '  sudo dnf install -y' >&2
        dnf_packages "$target" | while read -r package; do printf ' %s' "$package" >&2; done
        echo >&2
    else
        echo "Install a C/C++ compiler, make, autoconf, automake, libtool, pkg-config, curl, python3, gawk, patch, hexdump, and the MinGW-w64 POSIX toolchain for Windows builds." >&2
    fi
}

install_packages() {
    local target="$1"

    if have apt-get; then
        sudo apt update
        mapfile -t packages < <(apt_packages "$target")
        sudo apt install -y "${packages[@]}"
        if [ "$target" = "windows" ]; then
            [ -x /usr/bin/x86_64-w64-mingw32-gcc-posix ] && sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix || true
            [ -x /usr/bin/x86_64-w64-mingw32-g++-posix ] && sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix || true
        fi
    elif have dnf; then
        mapfile -t packages < <(dnf_packages "$target")
        sudo dnf install -y "${packages[@]}"
    else
        return 1
    fi
}

collect_missing_tools() {
    local target="$1"
    local missing_name="$2"
    local tools=(make git curl python3 gawk sed grep patch autoconf automake libtoolize pkg-config hexdump)
    local missing_tools=()
    local tool

    if [ "$target" = "windows" ]; then
        tools+=(x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++ x86_64-w64-mingw32-ar x86_64-w64-mingw32-ranlib x86_64-w64-mingw32-strip)
    else
        tools+=(gcc g++ strip)
    fi

    for tool in "${tools[@]}"; do
        have "$tool" || missing_tools+=("$tool")
    done

    if [ "${#missing_tools[@]}" -eq 0 ]; then
        printf -v "$missing_name" '%s' ''
    else
        printf -v "$missing_name" '%s' "${missing_tools[*]}"
    fi
}

require_tools() {
    local target="$1"
    local interactive="$2"
    local missing

    collect_missing_tools "$target" missing
    if [ -z "$missing" ]; then
        return 0
    fi

    echo "Missing tools: $missing" >&2
    package_hint "$target"

    if [ "$interactive" -eq 1 ] && ask_yes_no "Install the missing packages now?" yes; then
        install_packages "$target" || die "automatic package installation is not available on this system"
        collect_missing_tools "$target" missing
        [ -z "$missing" ] || die "tools are still missing after installation: $missing"
        return 0
    fi

    exit 1
}

require_mingw_posix() {
    local interactive="$1"
    local model

    model="$(x86_64-w64-mingw32-g++ -v 2>&1 | sed -n 's/^Thread model: //p' | tail -1)"
    if [ "$model" = "posix" ]; then
        return 0
    fi

    echo "Detected MinGW thread model: ${model:-unknown}" >&2
    echo "Hemp0x Core requires the POSIX MinGW-w64 variant for std::thread and condition_variable support." >&2
    package_hint windows

    if [ "$interactive" -eq 1 ] && ask_yes_no "Install/select the POSIX MinGW toolchain now?" yes; then
        install_packages windows || die "automatic package installation is not available on this system"
        model="$(x86_64-w64-mingw32-g++ -v 2>&1 | sed -n 's/^Thread model: //p' | tail -1)"
        [ "$model" = "posix" ] || die "MinGW thread model is still ${model:-unknown}; select the POSIX variant and rerun"
        return 0
    fi

    exit 1
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

update_source_tree() {
    local branch upstream

    if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        die "cannot update source tree because this is not a git checkout"
    fi

    if [ -n "$(git status --porcelain)" ]; then
        die "refusing to update with local changes present; commit, stash, or clean them first"
    fi

    branch="$(git symbolic-ref --quiet --short HEAD || true)"
    [ -n "$branch" ] || die "cannot update a detached HEAD checkout"

    upstream="$(git rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>/dev/null || true)"
    [ -n "$upstream" ] || die "current branch has no upstream configured"

    git fetch --prune
    git merge --ff-only "$upstream"
}

interactive_menu() {
    local choice

    echo "Hemp0x Core build helper"
    echo
    echo "Select build target:"
    echo "  1) Linux"
    echo "  2) Windows"

    while true; do
        read -r -p "Choice [1]: " choice
        choice="${choice:-1}"
        case "$choice" in
            1) target=linux; break ;;
            2) target=windows; break ;;
            *) echo "Choose 1 or 2." ;;
        esac
    done

    ask_yes_no "Build hemp0x-tx?" yes && with_tx=1 || with_tx=0
    ask_yes_no "Update this checkout before building?" no && update_tree=1 || update_tree=0
    ask_yes_no "Run build checks after compiling?" yes && run_tests=1 || run_tests=0

    echo
    echo "Select build profile:"
    echo "  1) Release binaries"
    echo "  2) Dev/debug binaries"

    while true; do
        read -r -p "Choice [1]: " choice
        choice="${choice:-1}"
        case "$choice" in
            1)
                debug=0
                with_libs=0
                break
                ;;
            2)
                debug=1
                ask_yes_no "Build libhemp0xconsensus for developer integrations?" no && with_libs=1 || with_libs=0
                break
                ;;
            *) echo "Choose 1 or 2." ;;
        esac
    done

    ask_yes_no "Reuse the current build state? Clean builds are safer." no && clean=0 || clean=1
}

target=linux
with_tx=0
with_libs=0
run_tests=0
skip_depends=0
clean=1
debug=0
update_tree=0
jobs="$(nproc 2>/dev/null || echo 2)"
interactive=0

if [ "$#" -eq 0 ] && [ -t 0 ]; then
    interactive=1
fi

while [ "$#" -gt 0 ]; do
    case "$1" in
        --interactive)
            interactive=1
            ;;
        --target)
            shift
            target="${1:-}"
            ;;
        --target=*)
            target="${1#*=}"
            ;;
        --update)
            update_tree=1
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

if [ "$interactive" -eq 1 ]; then
    interactive_menu
fi

[ "$target" = "linux" ] || [ "$target" = "windows" ] || die "--target must be linux or windows"
case "$jobs" in
    ''|*[!0-9]*) die "--jobs must be a positive integer" ;;
esac

ROOT="$(repo_root)"
cd "$ROOT"

require_tools "$target" "$interactive"
[ "$target" = "windows" ] && require_mingw_posix "$interactive"

if [ "$update_tree" -eq 1 ]; then
    update_source_tree
fi

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

echo
echo "Build configuration:"
echo "  target:        $target"
echo "  tx utility:    $([ "$with_tx" -eq 1 ] && echo yes || echo no)"
echo "  update tree:   $([ "$update_tree" -eq 1 ] && echo yes || echo no)"
echo "  tests:         $([ "$run_tests" -eq 1 ] && echo yes || echo no)"
echo "  debug:         $([ "$debug" -eq 1 ] && echo yes || echo no)"
echo "  libraries:     $([ "$with_libs" -eq 1 ] && echo yes || echo no)"
echo "  clean build:   $([ "$clean" -eq 1 ] && echo yes || echo no)"
echo "  jobs:          $jobs"
echo

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
