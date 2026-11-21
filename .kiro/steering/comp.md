---
inclusion: auto
name: comp-plugin
description: DAW/comp の TwoStageComp(Cubase 用 2段がけコンプ VST3, JUCE)を編集・ビルド・検証するときの案内
---

# comp(TwoStageComp)

`DAW/comp` は Cubase 13 用のボーカル向け2段コンプ VST3(JUCE / C++ / CMake + Ninja)。
作業前に次を読むこと。

- 現状・構成・ビルド手順・パラメータID・追加手順: #[[file:comp/README.md]]
- 要件と Coral 2 の調査結果: #[[file:comp/REQUIREMENTS.md]]

要点:
- 変更後は `cmake --build build` と `./build/CurveCheck`(ALL PASS)で検証する。
- パラメータ ID は変えない(保存済みプロジェクトが壊れる)。
- JUCE 本体は `monoral-to-steleo/external/JUCE` を共用。編集しない。
- 変更後は `./build-install.sh` で、ビルド、テスト、VST3 へのコピー、署名まで行う(ユーザー許可済み)。
- Cubase 実機と聴感は未確認。このPCでは画面のスクリーンショットも撮れない。
