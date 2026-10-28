#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
uni4="${UNI4:-$(command -v uni4 || echo "$root_dir/build/uni4")}"
out="${1:?usage: snapshot.sh <outfile>}"

: >"$out"

for base in 0100 0200 0300 0500 0600 0900 0A00 0B00 0C00 0D00 0E00 \
            1100 1200 1300 1400 1500 1600 1700 1900 1A00 1B00 1C00 1D00 1E00; do
    echo "== 0x${base} ==" >>"$out"
    "$uni4" ec dump "0x${base}" 0x100 >>"$out" 2>/dev/null || echo "(unreadable)" >>"$out"
done

echo "snapshot written to $out"
