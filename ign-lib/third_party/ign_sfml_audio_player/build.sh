#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
SFML_DIR="$ROOT_DIR/third_party/SFML"
BUILD_DIR="$SFML_DIR/build-ign"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake is not installed or not in PATH." >&2
    exit 1
fi

if ! command -v g++ >/dev/null 2>&1; then
    echo "Error: g++ is not installed or not in PATH." >&2
    exit 1
fi

if [ ! -f "$SFML_DIR/CMakeLists.txt" ]; then
    echo "Error: SFML source was not found in $SFML_DIR." >&2
    exit 1
fi

cmake -S "$SFML_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DSFML_BUILD_AUDIO=ON \
    -DSFML_BUILD_WINDOW=OFF \
    -DSFML_BUILD_GRAPHICS=OFF \
    -DSFML_BUILD_NETWORK=OFF \
    -DSFML_BUILD_EXAMPLES=OFF \
    -DSFML_BUILD_TEST_SUITE=OFF \
    -DSFML_BUILD_DOC=OFF \
    -DSFML_USE_SYSTEM_DEPS=OFF \
    -DCMAKE_INSTALL_PREFIX="$SFML_DIR"

cmake --build "$BUILD_DIR" --config Release --parallel 2
cmake --install "$BUILD_DIR" --config Release

mkdir -p "$SFML_DIR/bin"

g++ -std=c++17 -O2 \
    -I"$SFML_DIR/include" \
    "$SCRIPT_DIR/ign_sfml_audio_player.cpp" \
    -L"$SFML_DIR/lib" \
    -lsfml-audio-s \
    -lsfml-system-s \
    -lFLAC \
    -lvorbisfile \
    -lvorbisenc \
    -lvorbis \
    -logg \
    -pthread \
    -ldl \
    -lm \
    -o "$SFML_DIR/bin/ign-sfml-audio-player"

echo "Built $SFML_DIR/bin/ign-sfml-audio-player"
