#!/bin/bash
# Builds a release of Fallout 2 Community Edition with Stimpack and packs it for
# this platform into dist/:
#
#   macOS    stimpack-<version>-macos.zip      (the .app, ad-hoc signed)
#   Linux    stimpack-<version>-linux-x64.tar.gz
#   Windows  stimpack-<version>-windows-x64.zip (run from Git Bash)
#
# Each archive also holds README, INSTALL, LICENSE, an example fallout2.cfg and
# the English stimpack.msg (a template for translations). No game data is
# included: players need their own copy of Fallout 2.
set -euo pipefail

STIMPACK_DIR="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' <"$STIMPACK_DIR/VERSION")"
BUILD_DIR="${BUILD_DIR:-$STIMPACK_DIR/build/release}"
DIST_ROOT="$STIMPACK_DIR/dist"

case "$(uname -s)" in
    Darwin) PLATFORM=macos ;;
    Linux) PLATFORM=linux-x64 ;;
    MINGW* | MSYS* | CYGWIN*) PLATFORM=windows-x64 ;;
    *) echo "unsupported platform $(uname -s)" >&2; exit 1 ;;
esac

NAME="stimpack-$VERSION-$PLATFORM"
DIST="$DIST_ROOT/$NAME"

"$STIMPACK_DIR/tools/build.sh" Release "$BUILD_DIR"
cmake --build "$BUILD_DIR" --config Release --target ce-dat-tool

rm -rf "$DIST"
mkdir -p "$DIST/translations/english"

find_tool() {
    for candidate in "$BUILD_DIR/$1" "$BUILD_DIR/Release/$1" "$BUILD_DIR/$1.exe" "$BUILD_DIR/Release/$1.exe"; do
        if [[ -f "$candidate" ]]; then
            echo "$candidate"
            return
        fi
    done
    echo "$1 not found in $BUILD_DIR" >&2
    exit 1
}

case "$PLATFORM" in
    macos)
        APP="$BUILD_DIR/Fallout II Community Edition.app"
        ditto "$APP" "$DIST/$(basename "$APP")"
        # Ad-hoc signature: Gatekeeper still asks on first start (see INSTALL).
        codesign --force --deep --sign - "$DIST/$(basename "$APP")"
        ;;
    linux-x64)
        cp "$(find_tool fallout2-ce)" "$DIST/fallout2-ce"
        objcopy --strip-all "$DIST/fallout2-ce" 2>/dev/null || strip "$DIST/fallout2-ce"
        "$(find_tool ce-dat-tool)" create "$STIMPACK_DIR/engine/files/ce.dat" "$DIST/ce.dat"
        ;;
    windows-x64)
        cp "$(find_tool fallout2-ce)" "$DIST/fallout2-ce.exe"
        "$(find_tool ce-dat-tool)" create "$STIMPACK_DIR/engine/files/ce.dat" "$DIST/ce.dat"
        ;;
esac

cp "$STIMPACK_DIR/README.md" "$DIST/README.md"
cp "$STIMPACK_DIR/docs/INSTALL.md" "$DIST/INSTALL.md"
cp "$STIMPACK_DIR/LICENSE.md" "$DIST/LICENSE.md"
cp "$STIMPACK_DIR/engine/files/fallout2.cfg" "$DIST/EXAMPLE_fallout2.cfg"
cp "$STIMPACK_DIR/data/text/english/game/stimpack.msg" "$DIST/translations/english/stimpack.msg"

cd "$DIST_ROOT"
case "$PLATFORM" in
    macos) rm -f "$NAME.zip"; ditto -c -k --keepParent "$NAME" "$NAME.zip"; ARCHIVE="$NAME.zip" ;;
    linux-x64) tar -czf "$NAME.tar.gz" "$NAME"; ARCHIVE="$NAME.tar.gz" ;;
    windows-x64) rm -f "$NAME.zip"; 7z a -tzip "$NAME.zip" "$NAME" >/dev/null; ARCHIVE="$NAME.zip" ;;
esac

if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$ARCHIVE" >"$ARCHIVE.sha256"
else
    sha256sum "$ARCHIVE" >"$ARCHIVE.sha256"
fi
echo "packed: $DIST_ROOT/$ARCHIVE"
