#!/usr/bin/env bash
#
# hemp0x-chain-anchor-check.sh — Read-only chain anchor validation helper
#
# HISTORICAL VALIDATION BASELINE (NON-CONSENSUS GUARDRAIL)
# Purpose: Collect compact blockchain info for a selection of checkpoints
# and trust-anchor heights. This script proves that chain parameters and
# trust anchors have not drifted. It is purely read-only and must never
# stop/start nodes, install binaries, write into the datadir, or alter
# wallet/node state in any way.
#
# Usage:
#   ./hemp0x-chain-anchor-check.sh --cli /path/to/hemp0x-cli \
#       --conf /path/to/hemp0x.conf --datadir /path/to/datadir
#
# Safety: refuses to run against the default live datadir unless an
# explicit --allow-default-datadir override is supplied.
#
# Hard rules:
#   - Do not change consensus rules or chain parameters.
#   - Do not stop/start nodes.
#   - Do not write into the datadir.
#   - Do not install binaries or modify system state.

set -euo pipefail

CLI=""
CONF=""
DATADIR=""
ALLOW_DEFAULT_DATADIR=0
HEIGHTS=(0 270144 274176 500000 1000000 1500000 2000000)
EXPECTED_HASHES=(
  "0000009907c43e63467860fcb2a76eed0200e4f918de00bfea3fa35aa22dddd0"
  "0000000582ebb3cb1f1590c43cae7b12b1119178cbd71cbcb6b42084bbe5b085"
  "000000018f0fb60cc898229bf3086df590546354d4a0ebd922c5fa4d2b4c91bf"
  "00000000c1ca9606453b80f39abf80f99edbb67a1f4a97a35438fae825b92db2"
  "0000000489c1038667e941aeafe4177115300da01843adaf36b889a804810bca"
  "00000004cfb28dcb6c737915b008f8c5824c8201670875f7461f4019f8e65ebb"
  "000000002f781ea4d01f5866a8f26747c245ec90111099af851a40144b37e118"
)

usage() {
  cat <<'EOF'
Usage: hemp0x-chain-anchor-check.sh --cli <path> --conf <path> --datadir <path>
          [--allow-default-datadir] [--help]

  --cli           Path to hemp0x-cli binary
  --conf          Path to hemp0x.conf configuration file
  --datadir       Path to the node data directory
  --allow-default-datadir
                  Permit /home/bcr/.hemp0x or $HOME/.hemp0x as datadir
                  (omitted otherwise)
  --help          Show this help message
EOF
  exit 0
}

error_exit() {
  echo "ERROR: $*" >&2
  exit 1
}

check_default_datadir() {
  local dir="$1"
  while [[ "$dir" != "/" && "$dir" == */ ]]; do
    dir="${dir%/}"
  done
  local home_default=""
  if [[ -n "${HOME:-}" ]]; then
    home_default="$HOME/.hemp0x"
  fi

  if [[ "$dir" == "/home/bcr/.hemp0x" ]] || [[ -n "$home_default" && "$dir" == "$home_default" ]]; then
    if [[ "$ALLOW_DEFAULT_DATADIR" -ne 1 ]]; then
      error_exit "Refusing to use default datadir '${dir}'. Supply --allow-default-datadir to override."
    fi
  fi
}

require_value() {
  local opt="$1"
  local value="${2:-}"
  if [[ -z "$value" || "$value" == --* ]]; then
    error_exit "${opt} requires a value"
  fi
}

# Parse arguments
while [[ $# -gt 0 ]]; do
  case "$1" in
    --cli)           require_value "$1" "${2:-}"; CLI="$2"; shift 2 ;;
    --conf)          require_value "$1" "${2:-}"; CONF="$2"; shift 2 ;;
    --datadir)       require_value "$1" "${2:-}"; DATADIR="$2"; shift 2 ;;
    --allow-default-datadir) ALLOW_DEFAULT_DATADIR=1; shift ;;
    --help)          usage ;;
    *)               error_exit "Unknown argument: $1" ;;
  esac
done

if [[ -z "$CLI" ]]; then
  error_exit "--cli is required"
fi
if [[ -z "$CONF" ]]; then
  error_exit "--conf is required"
fi
if [[ -z "$DATADIR" ]]; then
  error_exit "--datadir is required"
fi

check_default_datadir "$DATADIR"

if [[ ! -x "$CLI" ]]; then
  error_exit "CLI binary not executable: $CLI"
fi
if [[ ! -f "$CONF" ]]; then
  error_exit "Config file not found: $CONF"
fi
if [[ ! -d "$DATADIR" ]]; then
  error_exit "Datadir directory not found: $DATADIR"
fi

CLI_CMD=("$CLI" "-conf=$CONF" "-datadir=$DATADIR")

# Verify connectivity
if ! "${CLI_CMD[@]}" getblockchaininfo >/dev/null 2>&1; then
  error_exit "Cannot connect to hemp0xd. Is the node running?"
fi

echo "=== hemp0x-chain-anchor-check ==="
echo "CLI:      $CLI"
echo "Conf:     $CONF"
echo "Datadir:  $DATADIR"
echo ""

# 1. Blockchain info summary
echo "--- getblockchaininfo ---"
"${CLI_CMD[@]}" getblockchaininfo 2>/dev/null | head -n 11
echo ""

# 2. Trust anchor checkpoint check
echo "--- Checkpoints (getblockhash + getblockheader) ---"
PASS=0
FAIL=0
for i in "${!HEIGHTS[@]}"; do
  height="${HEIGHTS[$i]}"
  expected="${EXPECTED_HASHES[$i]}"
  actual="$("${CLI_CMD[@]}" getblockhash "$height" 2>/dev/null)" || actual="ERROR"
  if [[ "$actual" == "$expected" ]]; then
    echo "  [PASS] height=$height hash=$actual"
    PASS=$((PASS + 1))
  else
    echo "  [FAIL] height=$height expected=$expected actual=$actual"
    FAIL=$((FAIL + 1))
  fi
done
echo ""

# 3. Block header summary for selected heights
echo "--- Block header summaries ---"
for height in "${HEIGHTS[@]}"; do
  echo "  Height $height:"
  "${CLI_CMD[@]}" getblockheader "$("${CLI_CMD[@]}" getblockhash "$height" 2>/dev/null)" 2>/dev/null | \
    head -n 6 | sed 's/^/    /'
done
echo ""

echo "=== Summary: $PASS passed, $FAIL failed ==="
if [[ "$FAIL" -gt 0 ]]; then
  exit 1
fi
exit 0
