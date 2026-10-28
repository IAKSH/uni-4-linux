#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
uni4="${UNI4:-$(command -v uni4 || echo "$root_dir/build/uni4")}"
label="${1:?usage: colorsnap.sh <label> [outfile]}"
out="${2:-/tmp/uni4-colorsnap-${label}.txt}"

{
    echo "== ${label} 0x0700-0x09ff =="
    "$uni4" ec dump 0x0700 0x300
    echo "== ${label} 0x1800-0x182f =="
    "$uni4" ec dump 0x1800 0x30
} >"$out"

echo "captured -> $out"
