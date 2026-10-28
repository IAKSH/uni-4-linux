#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
uni="${UNI:-$(command -v uni || echo "$root_dir/build/uni")}"
label="${1:?usage: colorsnap.sh <label> [outfile]}"
out="${2:-/tmp/uni-colorsnap-${label}.txt}"

{
    echo "== ${label} 0x0700-0x09ff =="
    "$uni" ec dump 0x0700 0x300
    echo "== ${label} 0x1800-0x182f =="
    "$uni" ec dump 0x1800 0x30
} >"$out"

echo "captured -> $out"
