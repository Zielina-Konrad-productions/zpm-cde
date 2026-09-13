#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build/bin"
UNINSTALLER_SRC="$SCRIPT_DIR/build/uninstall.cpp"
UNINSTALL_BIN="$BUILD_DIR/UNINSTALL"

echo "===== ign-lib uninstaller ====="

if ! command -v g++ >/dev/null 2>&1; then
    echo "[!] Error: g++ compiler is not installed or not in PATH."
    exit 1
fi

mkdir -p "$BUILD_DIR"

echo "[*] Compiling UNINSTALL..."
if ! g++ -std=c++17 -Wall -Wextra -I"$ROOT_DIR" "$UNINSTALLER_SRC" -o "$UNINSTALL_BIN"; then
    echo "[*] Retrying UNINSTALL with -lstdc++fs..."
    g++ -std=c++17 -Wall -Wextra -I"$ROOT_DIR" "$UNINSTALLER_SRC" -o "$UNINSTALL_BIN" -lstdc++fs
fi

echo "[*] Running uninstaller..."
"$UNINSTALL_BIN" "$@"
