#!/usr/bin/env bash
# Configures and builds DropIt on macOS / Linux.
#
#   ./scripts/build.sh              # Release build
#   ./scripts/build.sh --debug      # Debug build
#   ./scripts/build.sh --clean      # wipe build/ first
#   ./scripts/build.sh --install    # copy plugin(s) to your user plugin folder
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
CONFIG="Release"
INSTALL=0
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        --debug)   CONFIG="Debug" ;;
        --release) CONFIG="Release" ;;
        --install) INSTALL=1 ;;
        --clean)   CLEAN=1 ;;
        -h|--help) sed -n '2,8p' "$0"; exit 0 ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

[[ $CLEAN -eq 1 ]] && rm -rf "$BUILD_DIR"

if [[ ! -f "$ROOT/external/JUCE/CMakeLists.txt" ]]; then
    echo "external/JUCE not found - CMake will download JUCE automatically (or run scripts/setup.sh first)."
fi

if command -v nproc >/dev/null; then JOBS="$(nproc)"; else JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"; fi

GEN_ARGS=()
command -v ninja >/dev/null && [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]] && GEN_ARGS=(-G Ninja)

cmake -S "$ROOT" -B "$BUILD_DIR" ${GEN_ARGS[@]+"${GEN_ARGS[@]}"} -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS"

ART="$BUILD_DIR/DropIt_artefacts/$CONFIG"
printf "\n\033[32mBuild succeeded:\033[0m\n"
echo "  VST3       : $ART/VST3/DropIt.vst3"
[[ -d "$ART/AU" ]] && echo "  AU         : $ART/AU/DropIt.component"
echo "  Standalone : $ART/Standalone/"

if [[ $INSTALL -eq 1 ]]; then
    if [[ "$(uname -s)" == "Darwin" ]]; then
        VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"
        AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"
        mkdir -p "$VST3_DEST" "$AU_DEST"
        rm -rf "$VST3_DEST/DropIt.vst3" "$AU_DEST/DropIt.component"
        cp -R "$ART/VST3/DropIt.vst3" "$VST3_DEST/"
        [[ -d "$ART/AU/DropIt.component" ]] && cp -R "$ART/AU/DropIt.component" "$AU_DEST/"
        # Refresh the AU cache so Logic / GarageBand see the new build.
        killall -9 AudioComponentRegistrar 2>/dev/null || true
        echo "Installed to $VST3_DEST and $AU_DEST"
    else
        VST3_DEST="$HOME/.vst3"
        mkdir -p "$VST3_DEST"
        rm -rf "$VST3_DEST/DropIt.vst3"
        cp -R "$ART/VST3/DropIt.vst3" "$VST3_DEST/"
        echo "Installed to $VST3_DEST"
    fi
    echo "Rescan plugins in your DAW to pick it up."
fi
