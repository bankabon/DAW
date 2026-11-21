# 2段がけコンプ プラグイン 要件定義(確定版 v1)

## 1. 目的
Cubase 13 で使う、ボーカル向けコンプレッサー(VST3)。
内部にコンプを2段(直列)持つ。1段目で薄く、2段目で最後に厚くかける使い方を1つのプラグインで行う。
各段は個別にバイパスできるので、1段だけのコンプとしても使える。配布は自分の Mac のみ。

## 2. 参考製品: Acustica Audio Coral 2 のコンプ(Chimera)
調べて分かったこと(出典: KVR の製品ページ、Acustica 公式ページ、開発元コメント)。

**Coral のコンプの操作系**
| つまみ | 内容 |
|------|------|
| Detector | 9種類。C1〜C6 と Acquamarine / Sand / Ultramarine |
| Attack | 速い〜遅い |
| Release | 速い〜遅い |
| SHMOD | アタック時のエンベロープ形状を変える。0〜4 |
| Threshold | 0〜64 dB(掛かり始めの深さ) |
| Mix | 0〜100 %(パラレル) |
| Makeup | 0〜+24 dB |
| Side-chain | ON/OFF |
| 共通 | ステレオ / Mid-Side 切替、L-R リンク |

**音の性格**
- Ratio つまみはなく、Detector ごとにカーブが決まっている。
- 検波部に多数の整流器(rectifier)を持つ。ピークとレベルの捉え方が滑らか。
- ソフトクリップ方式。大きなダイナミクス変化に緩やかに効き、トランジェントを硬く削らない。通常のコンプはハードクリップ的に頭を潰し、明るく耳に刺さりやすい、という説明。
- 「ボーカルによく合う」という声が複数ある。C4〜C6 は透明で、長い Release でも自然。

**再現の限界**
Acustica は実機の応答をサンプリングする方式(Nebula 系)で、内部は非公開。同じ音の再現はできない。
本プラグインは、上の「操作系と性格」を目標にしたオリジナル実装とする。名前、画面、音源のコピーはしない。

## 3. スコープ
### 作る(v1)
- コンプ2段(Stage 1 = Gentle、Stage 2 = Heavy)の直列接続
- 各段: Threshold / Ratio / Knee / Attack / Release / Makeup / Detect / Soft Clip / HPF / Bypass
- 全体: Input / Output / Mix / Depth / Stereo Link
- 各段のゲインリダクション表示
- 全パラメータの保存とオートメーション

### 後回し(v2 以降)
- SHMOD(アタック形状 0〜4)と、Detector の複数タイプ(C1〜C6 風)
- Auto Release、Auto Makeup
- Mid/Side モード
- オーバーサンプリング(Soft Clip のエイリアシング対策)
- プリセット機能(DAW 側のプリセットは使える)
- AU / AAX、他OS

## 4. 処理の流れ
```
In → Input Gain → [Stage 1] → [Stage 2] → Output Gain → Mix(Dry/Wet) → Out
                     ↑各段Bypass可
```
Mix は原音(Input Gain 後)と、2段通過後の音を混ぜる。

## 5. 機能要件

### 5.1 各段のパラメータ
| パラメータ | 範囲 | Stage 1 初期値 | Stage 2 初期値 |
|------|------|------|------|
| Threshold | -60〜0 dB | -20 dB | -24 dB |
| Ratio | 1.1〜20 :1 | 1.8 | 4.0 |
| Knee | 0〜24 dB | 12 dB | 8 dB |
| Attack | 0.1〜100 ms | 30 ms | 8 ms |
| Release | 10〜2000 ms | 250 ms | 120 ms |
| Makeup | -12〜+24 dB | 0 dB | 0 dB |
| Detect | 0〜100 %(0=Peak、100=RMS) | 40 % | 20 % |
| Soft Clip | 0〜100 % | 0 % | 30 % |
| Sidechain HPF | 20〜400 Hz | 80 Hz | 120 Hz |
| Bypass | ON/OFF | OFF | OFF |

狙い: Stage 1 で 1〜3 dB、Stage 2 で 3〜6 dB のゲインリダクション(ボーカル)。

### 5.2 「薄く → 厚く」の操作
- **Depth(0〜100 %、初期 100 %)**: 2段の効きを1つのつまみで連動させる。
  - 0〜50 %: Stage 1 の効きが 0 から設定値まで増える。Stage 2 は効かない。
  - 50〜100 %: Stage 1 は設定値のまま、Stage 2 の効きが 0 から設定値まで増える。
  - 実装: 各段の Ratio を 1(効かない)と設定値の間で補間する。各段のつまみの設定値が上限になる。
- 各段の Bypass で、1段だけでも使える。

### 5.3 全体
- Input / Output Gain: -24〜+24 dB
- Mix: 0〜100 %(初期 100 %)
- Stereo Link: 0〜100 %(初期 100 %)。100 % で左右に同じゲインリダクション。
- モノラル、ステレオ入出力に対応

## 6. 非機能要件
| 項目 | 要件 |
|------|------|
| 形式 | VST3(Cubase 用)。Standalone も同時にビルドして動作確認に使う |
| 環境 | macOS arm64、Cubase 13 |
| レイテンシ | 0 サンプル(ルックアヘッドなし) |
| CPU | 1 インスタンス 2 % 未満(目標) |
| オートメーション | 全パラメータ対応。値の急変でノイズが出ない |
| 安全性 | NaN / 無限大を出さない。オーディオスレッドでメモリ確保しない |

## 7. 技術方針
- JUCE 8(C++20)、CMake + Ninja。`../monoral-to-steleo/external/JUCE` を共用する(コピーしない)。
- ゲインコンピュータは自作(ソフトニー対応)。ゲインリダクションは dB 領域で計算し、Attack / Release で平滑化する。
- 同じ `Compressor` クラスを2インスタンス並べる。
- パラメータは `AudioProcessorValueTreeState`。

## 8. 検証方法
1. ビルドが通り、VST3 が生成される
2. 静的カーブ(入力レベルと出力レベルの関係)が、設定した Threshold / Ratio / Knee と一致する
3. Cubase 13 に読み込み、ボーカルで聴感確認(これは本人が行う)

## 9. 未確認事項
- Cubase 13 での実機読み込みは、このPCから確認できない。
- フル版 Xcode は未インストール。monoral-to-steleo と同じく、Command Line Tools + Ninja でビルドする。
