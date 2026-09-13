#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build/bin"
INSTALLER_SRC="$SCRIPT_DIR/build/install.cpp"
UNINSTALLER_SRC="$SCRIPT_DIR/build/uninstall.cpp"
INSTALL_BIN="$BUILD_DIR/INSTALL"
UNINSTALL_BIN="$BUILD_DIR/UNINSTALL"

compile_cpp() {
    local source_file="$1"
    local output_file="$2"
    local label="$3"

    echo "[*] Compiling $label..."
    if g++ -std=c++17 -Wall -Wextra -I"$ROOT_DIR" "$source_file" -o "$output_file"; then
        return 0
    fi

    echo "[*] Retrying $label with -lstdc++fs..."
    g++ -std=c++17 -Wall -Wextra -I"$ROOT_DIR" "$source_file" -o "$output_file" -lstdc++fs
}

echo "===== ign-lib installer ====="

if ! command -v g++ >/dev/null 2>&1; then
    echo "[!] Error: g++ compiler is not installed or not in PATH."
    exit 1
fi

mkdir -p "$BUILD_DIR"

compile_cpp "$INSTALLER_SRC" "$INSTALL_BIN" "INSTALL"
compile_cpp "$UNINSTALLER_SRC" "$UNINSTALL_BIN" "UNINSTALL"

echo "[*] Running installer..."
"$INSTALL_BIN" --source "$ROOT_DIR" "$@"
