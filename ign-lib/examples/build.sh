#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BIN_DIR="$SCRIPT_DIR/bin"

if ! command -v g++ >/dev/null 2>&1; then
    echo "Error: g++ compiler is not installed or not in PATH." >&2
    exit 1
fi

mkdir -p "$BIN_DIR"

found=0
for source_file in "$SRC_DIR"/*.cpp; do
    if [ ! -e "$source_file" ]; then
        break
    fi

    found=1
    name="$(basename "$source_file" .cpp)"
    output_file="$BIN_DIR/$name"

    echo "[*] Building $name"
    g++ -std=c++17 -Wall -Wextra -I"$ROOT_DIR" "$source_file" -o "$output_file"
done

if [ "$found" -eq 0 ]; then
    echo "No examples found in $SRC_DIR" >&2
    exit 1
fi

echo "[*] Done. Binaries are in $BIN_DIR"
