#!/bin/bash
# MonoToStereo をビルドし、VST3 を ~/Library/Audio/Plug-Ins/VST3/ に入れ替える。
#
# 使い方:
#   scripts/build_and_install.sh            # Release ビルド(通常はこちら。軽い)
#   scripts/build_and_install.sh debug      # Debug ビルド(デバッグ・ログ確認用)
#
# Cubase で読み込み中の場合は、先に Cubase を終了してから実行すること。

set -euo pipefail

CONFIG_ARG="${1:-release}"
case "$CONFIG_ARG" in
    release|Release) CONFIG="Release" ;;
    debug|Debug)     CONFIG="Debug" ;;
    *) echo "使い方: $0 [release|debug]" >&2; exit 1 ;;
esac

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')"
INSTALL_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
PLUGIN_NAME="MonoToStereo.vst3"

echo "== 設定: $CONFIG(ビルド先: $BUILD_DIR)"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$CONFIG"

echo "== ビルド"
cmake --build "$BUILD_DIR"

BUILT_PLUGIN="$(find "$BUILD_DIR/MonoToStereo_artefacts" -type d -name "$PLUGIN_NAME" -path "*/VST3/*" | head -n 1)"
if [ -z "$BUILT_PLUGIN" ]; then
    echo "エラー: ビルドした $PLUGIN_NAME が見つかりません" >&2
    exit 1
fi

echo "== インストール: $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
rm -rf "${INSTALL_DIR:?}/$PLUGIN_NAME"
cp -R "$BUILT_PLUGIN" "$INSTALL_DIR/"

echo "== 完了($CONFIG)。Cubase を再起動して読み込み直してください。"
