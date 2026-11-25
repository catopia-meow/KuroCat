#!/usr/bin/env bash
# 卸载通过 install.sh 免密码安装的 SachiCat（只删除当前用户目录下的文件）。
# 如果你是用 .deb 安装的，请改用：sudo apt remove kurocat
set -uo pipefail

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

pkill -x sachicat >/dev/null 2>&1 || true
pkill -x kurocat >/dev/null 2>&1 || true
rm -f "$HOME/.local/bin/sachicat" "$HOME/.local/bin/kurocat" \
      "$DATA_HOME/applications/kurocat.desktop" \
      "$DATA_HOME/icons/hicolor/scalable/apps/kurocat.svg" \
      "$CONFIG_HOME/autostart/kurocat.desktop"
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$DATA_HOME/applications" >/dev/null 2>&1 || true

echo "SachiCat 已卸载。（设置文件保留在 $CONFIG_HOME/kurocat，如不需要可手动删除）"
