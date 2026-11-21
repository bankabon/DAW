#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "EqCurve.h"

enum class EqPhaseMode
{
    minimum, // 遅延なし。位相は変わる
    linear   // 位相は変わらないが、処理に遅延が出る
};

/**
 * EQ カーブから、畳み込み(juce::dsp::Convolution)に渡すインパルス応答(IR)を作る。
 *
 * 手順: カーブ → 各周波数のゲイン → 逆 FFT → 時間波形(IR)
 * Minimum Phase は、対数ゲインのケプストラムを折り返して最小位相の応答にする。
 */
class EqImpulseDesigner
{
public:
    static constexpr int fftOrder = 13;
    static constexpr int irLength = 1 << fftOrder;

    /** Linear Phase の IR は中央にピークが来るため、その分だけ出力が遅れる。 */
    static constexpr int linearPhaseLatencySamples = irLength / 2;

    /**
     * @param gainScale     カーブのゲインを何倍にするか(EQ Amount。1.0 でそのまま)
     * @param makeupGainDb  全体に足す音量(Auto Gain の補正)
     */
    static juce::AudioBuffer<float> design (const EqCurve& curve, double sampleRate, EqPhaseMode mode,
                                            float gainScale = 1.0f, float makeupGainDb = 0.0f);
};
