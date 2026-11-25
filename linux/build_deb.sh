#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(tr -d '\r\n' < "$ROOT/../VERSION")"
ARCH="$(dpkg --print-architecture)"
PKG_DIR="$(mktemp -d)"
trap 'rm -rf "$PKG_DIR"' EXIT

"$ROOT/build.sh"

mkdir -p \
    "$PKG_DIR/DEBIAN" \
    "$PKG_DIR/usr/bin" \
    "$PKG_DIR/usr/share/applications" \
    "$PKG_DIR/usr/share/icons/hicolor/scalable/apps"

cp "$ROOT/sachicat" "$PKG_DIR/usr/bin/sachicat"
chmod 755 "$PKG_DIR/usr/bin/sachicat"
ln -s sachicat "$PKG_DIR/usr/bin/kurocat"
cp "$ROOT/kurocat.desktop" "$PKG_DIR/usr/share/applications/kurocat.desktop"
cp "$ROOT/kurocat.svg" "$PKG_DIR/usr/share/icons/hicolor/scalable/apps/kurocat.svg"

INSTALLED_KB="$(du -sk "$PKG_DIR/usr" | cut -f1)"

# Kylin's / UOS's package installers are stricter than dpkg: use a real e-mail in Maintainer,
# accept the Ubuntu 24.04+/Debian 13 "t64" GTK package name, and state the glibc floor so an
# incompatible system gets a clear dependency message instead of a silently broken install.
cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: kurocat
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_KB}
Depends: libc6 (>= 2.28), libgtk-3-0 (>= 3.22) | libgtk-3-0t64, libcairo2, libx11-6, libxi6
Maintainer: chanke17 <chanke17@users.noreply.github.com>
Homepage: https://github.com/catopia-meow/SachiCat
Description: SachiCat native desktop keyboard & mouse cat pet
 Lightweight 60fps transparent desktop pet supporting full-state keyboard and
 mouse capture for typing and gaming (XInput2 + evdev), 2.5D head tracking,
 hunting dilated eyes, petting interactions, and Black/Siamese skins.
 Compatible with Ubuntu, Debian, Deepin, UnionTech UOS, and Kylin OS.
EOF

# mktemp -d creates a 0700 directory; dpkg-deb rejects that under a strict umask.
chmod -R u=rwX,go=rX "$PKG_DIR"
chmod 755 "$PKG_DIR/usr/bin/sachicat"

OUT_DEB="$ROOT/SachiCat-Linux-${ARCH}.deb"
dpkg-deb --build --root-owner-group "$PKG_DIR" "$OUT_DEB"
echo "==> Packaged Debian/UOS installer: $OUT_DEB ($(du -h "$OUT_DEB" | cut -f1))"
