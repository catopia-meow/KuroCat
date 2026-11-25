#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/sachicat"
VERSION="$(tr -d '\r\n' < "$ROOT/../VERSION")"

echo "==> Building SachiCat for Linux (C++17 / GTK3 + Cairo + XInput2 + evdev)..."
# Release builds run inside Debian 10 (glibc 2.28 / GCC 8, see .github/workflows/build.yml) so the
# binary also starts on 统信 UOS 20 and 银河麒麟 V10, whose glibc is older than Ubuntu 22.04's.
g++ -DSACHICAT_VERSION_STR="\"$VERSION\"" -std=c++17 -O2 -Wall -Wextra \
    "$ROOT/src/main.cpp" \
    -o "$OUT" \
    $(pkg-config --cflags --libs gtk+-3.0 cairo x11 xi) \
    -lpthread

strip "$OUT" || true

# Report the newest glibc symbol the binary needs (must stay <= 2.28 for 统信 UOS 20 / 麒麟 V10).
if command -v objdump >/dev/null 2>&1; then
    NEED="$(objdump -T "$OUT" | grep -o 'GLIBC_[0-9][0-9.]*' | sort -uV | tail -1)"
    echo "==> Requires ${NEED:-unknown}"
    if [ -n "${KUROCAT_MAX_GLIBC:-}" ] && [ -n "$NEED" ] && \
       [ "$(printf '%s\n%s\n' "GLIBC_${KUROCAT_MAX_GLIBC}" "$NEED" | sort -V | tail -1)" != "GLIBC_${KUROCAT_MAX_GLIBC}" ]; then
        echo "[ERROR] $NEED is newer than GLIBC_${KUROCAT_MAX_GLIBC}: old 统信/麒麟 systems could not run it." >&2
        exit 1
    fi
fi
echo "==> Built successfully: $OUT ($(du -h "$OUT" | cut -f1))"
