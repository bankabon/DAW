# TwoStageComp(2段がけコンプ)開発メモ

AI(Kiro)と人が、次回このプロジェクトをすぐ再開するためのドキュメント。
要件の背景は `REQUIREMENTS.md`、ここは「今どうなっているか」と「どう触るか」。

## 概要
- Cubase 13 用のボーカル向けコンプ(VST3)。Acustica Audio Coral 2 のコンプ(Chimera)の性格(穏やか、トランジェントを守る)を参考にしたオリジナル実装。
- 内部にコンプ2段(Stage 1 = Gentle、Stage 2 = Heavy)を直列に持つ。各段 Bypass 可。
- Depth つまみで「薄く(Stage 1 のみ)→ 厚く(Stage 2 も追加)」を1つで操作する。
- 配布は自分の Mac のみ(macOS arm64)。

## 環境とビルド
- JUCE は `../monoral-to-steleo/external/JUCE` を共用(CMakeLists.txt で参照。コピーしない)。
- 前提: Command Line Tools、cmake、ninja(`xcode-select --install`、`brew install cmake ninja`)。
- フル版 Xcode は不要。Ninja でビルドする。

```sh
cd /Users/kotatsujiguchi/Documents/private/DAW/comp
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # 初回のみ
cmake --build build                                        # 全ターゲット
./build/CurveCheck                                         # DSP テスト(ALL PASS を確認)
```

生成物(`build/TwoStageComp_artefacts/Release/`):
- `VST3/TwoStageComp.vst3`
- `Standalone/TwoStageComp.app`

### Cubase へのインストール(推奨: スクリプト)
```sh
./build-install.sh
```
ビルド → `CurveCheck`(落ちたら中止)→ `~/Library/Audio/Plug-Ins/VST3/` へコピー → ad-hoc 署名まで行う。ユーザーが「ビルドしたら VST3 へコピーする」ことを許可済みなので、このスクリプトの実行は確認不要。

手動の場合:
```sh
rm -rf ~/Library/Audio/Plug-Ins/VST3/TwoStageComp.vst3
cp -R build/TwoStageComp_artefacts/Release/VST3/TwoStageComp.vst3 ~/Library/Audio/Plug-Ins/VST3/
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/TwoStageComp.vst3
```
Cubase でプラグインを再スキャンする。ユーザーの許可を得てからコピーすること(過去は毎回許可済みだったが、確認は取る)。

## ファイル構成
| ファイル | 役割 |
|------|------|
| `Source/Compressor.h` | 1段分のコンプ。標準ライブラリのみ(JUCE 非依存)。ゲインコンピュータ、ソフトクリップ、サイドチェーン HPF、検波、アタック/リリース |
| `Source/PluginProcessor.h/.cpp` | パラメータ定義、2段の接続、Depth、Mix、メーター値、プリセット適用、状態保存 |
| `Source/PluginEditor.h/.cpp` | 画面。上段(Depth、IN/OUT メーター、Input/Mix/Link/Output)と、Stage 1/2 のカード |
| `Source/LookAndFeel.h` | つまみとバイパスボタンの描画 |
| `Source/Presets.h` | ボーカル用プリセット表(10個) |
| `Tests/CurveCheck.cpp` | JUCE 非依存の DSP テスト(`CurveCheck` ターゲット) |
| `REQUIREMENTS.md` | 要件定義と、Coral 2 の調査結果 |

## Auto Makeup
各段の `auto` をオンにすると、その段の平均ゲインリダクション(時定数約1秒)を求め、同じ量だけ持ち上げる(`Compressor.h`)。手動の Makeup には加算される。
プリセットは `auto` をオフにする(音を変えないため)。テストは `CurveCheck` の「auto makeup」項目。
無音が続くと平均が 0 に戻り、補正も外れる。

## 追加機能の仕様(Compressor.h / PluginProcessor.cpp)
- **SC HF**(`deess`): 検波側だけに約 5 kHz のハイシェルフ(0〜+12 dB)を掛ける。サ行が強いとコンプが深く効く。音そのものの高域を削るのではなく、全帯域のゲインが下がる(分割帯域のディエッサーではない)。
- **Auto Release**(`autorel`): 平均ゲインリダクション(約300 ms)が深いほど Release を長く、浅いほど短くする(Release つまみの 0.35 倍〜2 倍)。
- **SHMOD**(`shmod` 0〜4): アタックの立ち上がり形状。目標に近づくほど速くなる S 字寄りの形で、数値が大きいほど頭を通す。Coral の SHMOD の再現ではなく、同じ狙いのオリジナル。
- **Soft Clip の ADAA**: 1次の Antiderivative Anti-Aliasing。レイテンシは増えない(オーバーサンプリングは使わない)。`softClipAdaa()`。
- **平滑化**: Threshold / Ratio / Makeup は約20 ms、Input / Output / Mix はブロック内の直線ランプで追う。
- **A/B**(`ab`): 原音(Input Gain 後)に切り替える。音量は、原音とコンプ後の平均パワー(約1秒)から自動で揃える。切替は約10 ms のクロスフェード。無音の間は音量補正を保持する。
- **Auto Vol**(`Source/AutoVolume.h`): 最終段(Mix の後、A/B の前)で、ピークの包絡(約300 ms)を目標ピーク(初期 -5 dBFS)に向けてゆっくり揃える。下げるのは約30 ms、上げるのは約600 ms。補正は ±range dB(初期 12)。-55 dBFS 未満では 0 dB に戻す(ノイズを上げない)。ルックアヘッドなしのため、急な大音量の頭は一瞬目標を超えうる。オフにするときは約600 ms かけて 0 dB に戻る。プリセットはオフにする。
- プリセットは `Presets.h`(10個)。男女別に加え、Rap / Whisper / Backing を追加。

## 信号の流れ(PluginProcessor::processBlock)
```
入力ピーク測定 → Input Gain → (原音を保存) → Stage 1 → Stage 2 → Output Gain → Mix → 出力ピーク測定
```
- Mix は「Input Gain 後の原音」と「2段+Output Gain 後」を混ぜる。
- Depth(0〜100 %): `w1 = clamp(depth*2)`、`w2 = clamp((depth-0.5)*2)`。各段の実効 Ratio = `1 + (設定Ratio - 1) * w`。Ratio 1 は効かない。
- レイテンシは 0(ルックアヘッドなし)。

## パラメータID
- 全体: `in_gain`, `out_gain`, `mix`, `depth`, `link`, `ab`(A/B 原音比較), `avol`(Auto Vol オン/オフ), `avol_target`(目標ピーク dBFS、初期 -5), `avol_range`(補正の上限 dB、初期 12、画面には出していない)
- 各段(`s1_` / `s2_` + 名前): `thr`, `ratio`, `knee`, `attack`, `release`, `makeup`, `detect`(%、0=Peak 100=RMS), `soft`(%), `hpf`, `bypass`, `auto`(Auto Makeup), `autorel`(Auto Release), `deess`(SC HF, dB), `shmod`(0〜4)
- ID は `PluginProcessor::stageId(stage, name)` で作る。**ID を変えると保存済みプロジェクトの値が壊れる**。変える場合は version を上げる。
- パラメータ定義は `PluginProcessor::createLayout()`、各段の初期値は `stageDefaults`。

## 新しいパラメータを足す手順
1. `createLayout()` に `addFloat`(各段なら for ループ内)
2. 各段なら `comp::Params`(Compressor.h)に項目を足し、`readStage()` で読む。`Compressor` の処理に反映
3. 各段なら `PluginEditor.cpp` の `stageParamNames` / `stageLabels` と、`PluginEditor.h` の `stageKnobs` の個数を更新し、`resized()` の列数(現在 9)も変更
4. 全体つまみなら `PluginEditor` に `Knob` を追加して `addKnob`、`resized()` に配置
5. プリセットに関わるなら `Presets.h` の `Stage` 構造体と `setCurrentProgram()` を更新
6. `Tests/CurveCheck.cpp` に検証を足し、`./build/CurveCheck` が ALL PASS になることを確認

## プリセットの追加
`Presets.h` の `presets::all` に1行足し、`std::array<Preset, N>` の N を更新するだけ。画面のプルダウンと DAW のプリセット一覧は自動で増える。現在の選択番号は状態に `preset` として保存される。値は「出発点」で、実際の声で調整する前提(根拠はファイル先頭のコメント)。

## UI の注意
- 画面サイズは `PluginEditor` コンストラクタの `setSize`(幅 900)。定数 `headerH / topH / cardH / gap / margin` でレイアウトが決まる。
- 上段の x 座標は決め打ち(Stage バー 230〜380、メーター 392〜466、小つまみ 482〜)。足すときは重ならないよう確認する。
- メーターは 30 Hz のタイマーで更新。ピークは `std::atomic` を exchange で読む(プロセッサ側が最大値を保持)。

## 検証状況(2026-11 時点)
- 確認済み: ビルド成功、`CurveCheck` 全項目 PASS(ゲイン計算、ニー、Ratio 1 で無効、定常 GR、ソフトクリップ上限)。
- **未確認**: Cubase での読み込み、画面の実際の見た目(スクリーンショットが撮れない環境だった)、音の聴感、プリセットの音、pluginval による検証。
- ビルド警告は 0(`-Wsign-conversion` は解消済み)。`CurveCheck` は 18 項目(Auto Makeup、Auto Release、SHMOD、平滑化、SC HF、ADAA、Auto Vol を含む。件数は増減する)。

## 既知の制約・今後の候補
- Soft Clip は ADAA で折り返しを減らしているが、オーバーサンプリングほど完全ではない。
- 複数の Detector タイプ、Mid/Side は未実装(`REQUIREMENTS.md` の「後回し」)。
- プリセット選択後につまみを動かしても、プルダウン表示は選んだ名前のまま。
- 署名は ad-hoc のみ。他の Mac へ配る場合は署名と公証が必要。

## 作業ルール(このプロジェクト)
- JUCE 本体(`monoral-to-steleo/external/JUCE`)は編集しない。
- `build/` は生成物。読み書きしない(`.gitignore` 対象)。
- `~/Library/Audio/Plug-Ins/VST3/` へのコピーは `./build-install.sh` で行う(許可済み)。それ以外の手動コピーは許可を取る。
