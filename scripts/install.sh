#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root_dir"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

install -Dm755 build/uni4 /usr/local/bin/uni4

if command -v systemctl >/dev/null 2>&1 && [ -d /run/systemd/system ]; then
    install -Dm644 systemd/uni4-kbd-color.service /etc/systemd/system/uni4-kbd-color.service
    install -Dm644 systemd/uni4-kbd-rainbow.service /etc/systemd/system/uni4-kbd-rainbow.service
    install -Dm644 systemd/uni4-tpwatch.service /etc/systemd/system/uni4-tpwatch.service
    systemctl daemon-reload

    cat <<'EOF'

systemd units installed (none enabled). Common choices:
  systemctl enable --now uni4-kbd-rainbow
  systemctl enable --now uni4-kbd-color      # edit RGB values first
  systemctl enable --now uni4-tpwatch
EOF
fi

echo
echo "done. try: sudo uni4 probe"
