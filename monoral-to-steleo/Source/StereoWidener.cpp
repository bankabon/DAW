#include "StereoWidener.h"

namespace
{
    constexpr float smoothingSeconds = 0.05f;

    // 左右で別の周波数を使うと、左右の位相の回り方が変わる(デコリレーション)。
    constexpr std::array<float, 3> leftAllpassFrequenciesHz  { 300.0f, 1200.0f, 4000.0f };
    constexpr std::array<float, 3> rightAllpassFrequenciesHz { 500.0f, 2000.0f, 6500.0f };

    template <typename FilterArray, typename FrequencyArray>
    void setAllpassFrequencies (FilterArray& filters, const FrequencyArray& frequencies, double sampleRate)
    {
        for (size_t i = 0; i < filters.size(); ++i)
            filters[i].coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderAllPass (sampleRate, frequencies[i]);
    }

    template <typename FilterArray>
    float runAllpassChain (FilterArray& filters, float sample)
    {
        for (auto& filter : filters)
            sample = filter.processSample (sample);
        return sample;
    }
}

void StereoWidener::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const juce::dsp::ProcessSpec monoSpec { newSampleRate, static_cast<juce::uint32> (maxBlockSize), 1 };

    bassSplitter.prepare (monoSpec);

    const int maxDelaySamples = static_cast<int> (std::ceil (maxDelaySeconds * newSampleRate)) + 4;
    leftDelay.setMaximumDelayInSamples (maxDelaySamples);
    rightDelay.setMaximumDelayInSamples (maxDelaySamples);
    leftDelay.prepare (monoSpec);
    rightDelay.prepare (monoSpec);

    setAllpassFrequencies (leftAllpass,  leftAllpassFrequenciesHz,  newSampleRate);
    setAllpassFrequencies (rightAllpass, rightAllpassFrequenciesHz, newSampleRate);

    for (auto* smoothed : { &smoothedWidth, &smoothedSpreadSamples, &smoothedDecorrelation,
                            &smoothedMotion, &smoothedCenterGain, &smoothedMix })
        smoothed->reset (newSampleRate, smoothingSeconds);

    reset();
}

void StereoWidener::reset()
{
    bassSplitter.reset();
    leftDelay.reset();
    rightDelay.reset();

    for (auto& filter : leftAllpass)  filter.reset();
    for (auto& filter : rightAllpass) filter.reset();

    lfoPhase = 0.0f;
}

void StereoWidener::updateSmoothedTargets (const WidenerSettings& settings)
{
    const auto msToSamples = [this] (float ms) { return ms * 0.001f * static_cast<float> (sampleRate); };

    smoothedWidth.setTargetValue         (settings.widthPercent * 0.01f);
    smoothedSpreadSamples.setTargetValue (msToSamples (settings.spreadTimeMs));
    smoothedDecorrelation.setTargetValue (settings.decorrelationPct * 0.01f);
    smoothedMotion.setTargetValue        (settings.motionPercent * 0.01f);
    smoothedCenterGain.setTargetValue    (juce::Decibels::decibelsToGain (settings.centerLevelDb));
    smoothedMix.setTargetValue           (settings.mixPercent * 0.01f);
}

void StereoWidener::process (const float* monoInput, float* outLeft, float* outRight,
                             int numSamples, const WidenerSettings& settings)
{
    bassSplitter.setCutoffFrequency (settings.bassKeepHz);
    updateSmoothedTargets (settings);

    const float lfoIncrement = juce::MathConstants<float>::twoPi * lfoRateHz / static_cast<float> (sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float input = monoInput[i];

        // 1. 低域と高域に分ける
        float lowBand = 0.0f;
        float highBand = 0.0f;
        bassSplitter.processSample (0, input, lowBand, highBand);

        // 2. 左右で違う遅延にする。LFO で左右の遅延を逆向きに揺らす。
        const float width       = smoothedWidth.getNextValue();
        const float spread      = smoothedSpreadSamples.getNextValue();
        const float decorrelate = smoothedDecorrelation.getNextValue();
        const float motion      = smoothedMotion.getNextValue();
        const float centerGain  = smoothedCenterGain.getNextValue();
        const float mix         = smoothedMix.getNextValue();

        const float lfo = std::sin (lfoPhase);
        lfoPhase += lfoIncrement;
        if (lfoPhase >= juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;

        const float modulation = motionDepthRatio * motion * lfo;
        const float leftDelaySamples  = 0.5f * spread * (1.0f + modulation);
        const float rightDelaySamples = spread * (1.0f - modulation);

        leftDelay.pushSample (0, highBand);
        rightDelay.pushSample (0, highBand);
        const float delayedLeft  = leftDelay.popSample (0, leftDelaySamples, true);
        const float delayedRight = rightDelay.popSample (0, rightDelaySamples, true);

        // 3. オールパスで位相差を作る。decorrelate が 0 なら通さない。
        const float wetLeft  = juce::jmap (decorrelate, delayedLeft,  runAllpassChain (leftAllpass,  delayedLeft));
        const float wetRight = juce::jmap (decorrelate, delayedRight, runAllpassChain (rightAllpass, delayedRight));

        // 4. Mid/Side に変換し、Side を Width 倍して左右に戻す
        const float mid  = 0.5f * (wetLeft + wetRight) * centerGain;
        const float side = 0.5f * (wetLeft - wetRight) * width;

        // 5. 低域は中央に足し、原音と Mix する
        const float wideLeft  = mid + side + lowBand;
        const float wideRight = mid - side + lowBand;

        outLeft[i]  = input * (1.0f - mix) + wideLeft  * mix;
        outRight[i] = input * (1.0f - mix) + wideRight * mix;
    }
}
