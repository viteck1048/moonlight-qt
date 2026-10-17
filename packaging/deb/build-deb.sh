#!/bin/bash
# Builds a .deb from a full checkout (with submodules) on Ubuntu 24.04.
# Usage: packaging/deb/build-deb.sh <version> <outdir>    e.g. 6.2.0-keymap.1 dist
set -euo pipefail

VERSION="$1"
OUTDIR="$(realpath -m "$2")"
PKG=moonlight-qt-keymap
SRC="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="$(mktemp -d)"
STAGE="$WORK/$PKG"

cd "$SRC"
qmake6 PREFIX=/usr CONFIG+=release moonlight-qt.pro
make -j"$(nproc)"
make install INSTALL_ROOT="$STAGE"
strip --strip-unneeded "$STAGE/usr/bin/moonlight"

# dpkg-shlibdeps needs a debian/control next to it
mkdir -p "$WORK/debian"
printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' "$PKG" "$PKG" > "$WORK/debian/control"
SHLIBS=$(cd "$WORK" && dpkg-shlibdeps -O "$STAGE/usr/bin/moonlight" | sed 's/^shlibs:Depends=//')

# QML modules are loaded at runtime, so shlibdeps can't see them
QML="qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-templates, qml6-module-qtqml-workerscript"

mkdir -p "$STAGE/DEBIAN" "$STAGE/usr/share/doc/$PKG"
cp LICENSE "$STAGE/usr/share/doc/$PKG/copyright"
cat > "$STAGE/DEBIAN/control" <<CTRL
Package: $PKG
Version: $VERSION
Architecture: $(dpkg --print-architecture)
Maintainer: viteck1048 <viteck1048@gmail.com>
Depends: $SHLIBS, $QML
Conflicts: moonlight-qt
Replaces: moonlight-qt
Section: games
Priority: optional
Homepage: https://github.com/viteck1048/moonlight-qt
Description: Moonlight game streaming client (custom key combos fork)
 Moonlight is an open source client for NVIDIA GameStream and Sunshine.
 This build comes from the viteck1048 fork and adds user-defined key
 combos, host display cycling and Cyrillic text in the streaming
 overlay.
CTRL

mkdir -p "$OUTDIR"
dpkg-deb --root-owner-group --build "$STAGE" "$OUTDIR/${PKG}_${VERSION}_$(dpkg --print-architecture).deb"
rm -rf "$WORK"
