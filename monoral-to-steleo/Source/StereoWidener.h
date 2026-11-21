#pragma once

#include <array>
#include <juce_dsp/juce_dsp.h>

/** 広げ処理に渡す設定値。単位はコメントのとおり。 */
struct WidenerSettings
{
    float widthPercent     = 100.0f; // 0〜200。0 でモノラル
    float spreadTimeMs     = 15.0f;  // 左右の遅延時間(Haas 効果)
    float decorrelationPct = 50.0f;  // オールパスによる左右差の量
    float motionPercent    = 20.0f;  // 遅延の揺らぎの量
    float bassKeepHz       = 150.0f; // これより低い音は中央に残す
    float centerLevelDb    = 0.0f;   // 中央成分の音量
    float mixPercent       = 100.0f; // 原音(モノラル)との混ぜ具合
};

/**
 * モノラル音声を左右に広げる。
 *
 * 流れ:
 *   1. 低域と高域に分ける(低域は中央に残す)
 *   2. 高域を左右で違う遅延にする(Haas 効果。わずかに揺らす)
 *   3. 左右で違うオールパスを通して、音色を保ったまま位相差を作る
 *   4. Mid/Side に変換し、Side を Width 倍して左右に戻す
 *   5. 低域を足し、原音と Mix する
 */
class StereoWidener
{
public:
    void prepare (double newSampleRate, int maxBlockSize);
    void reset();

    /** monoInput の numSamples サンプルを処理して左右に出力する。 */
    void process (const float* monoInput, float* outLeft, float* outRight,
                  int numSamples, const WidenerSettings& settings);

private:
    static constexpr int allpassStages = 3;
    static constexpr float maxDelaySeconds = 0.06f; // spreadTime 最大 40ms + 揺らぎ分
    static constexpr float motionDepthRatio = 0.4f; // 遅延時間に対する揺らぎの最大割合
    static constexpr float lfoRateHz = 0.4f;

    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;
    using Allpass = juce::dsp::IIR::Filter<float>;

    void updateSmoothedTargets (const WidenerSettings& settings);

    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;

    juce::dsp::LinkwitzRileyFilter<float> bassSplitter;
    Delay leftDelay  { 1 };
    Delay rightDelay { 1 };
    std::array<Allpass, allpassStages> leftAllpass;
    std::array<Allpass, allpassStages> rightAllpass;

    // 値が急に変わるとノイズが出るので、なめらかに追従させる。
    juce::SmoothedValue<float> smoothedWidth;
    juce::SmoothedValue<float> smoothedSpreadSamples;
    juce::SmoothedValue<float> smoothedDecorrelation;
    juce::SmoothedValue<float> smoothedMotion;
    juce::SmoothedValue<float> smoothedCenterGain;
    juce::SmoothedValue<float> smoothedMix;
};
