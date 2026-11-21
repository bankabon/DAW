#!/bin/zsh
# ビルド → テスト → VST3 を ~/Library/Audio/Plug-Ins/VST3/ にコピー → ad-hoc 署名。
# 使い方: ./build-install.sh
set -euo pipefail

cd "${0:A:h}"   # このスクリプトのあるフォルダ(comp/)

NAME="TwoStageComp"
BUILT="build/${NAME}_artefacts/Release/VST3/${NAME}.vst3"
DEST_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

# 初回だけ configure
if [[ ! -f build/build.ninja ]]; then
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
fi

cmake --build build

# DSP テストが落ちたらインストールしない
./build/CurveCheck | tail -1
./build/CurveCheck > /dev/null

mkdir -p "$DEST_DIR"
rm -rf "$DEST_DIR/${NAME}.vst3"
cp -R "$BUILT" "$DEST_DIR/"
codesign --force --deep -s - "$DEST_DIR/${NAME}.vst3"
codesign -v "$DEST_DIR/${NAME}.vst3"

echo "installed: $DEST_DIR/${NAME}.vst3"
echo "Cubase でプラグインを再スキャン(または再起動)してください。"
