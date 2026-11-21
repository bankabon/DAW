#include "EqImpulseDesigner.h"

#include <complex>
#include <vector>
#include <juce_dsp/juce_dsp.h>

namespace
{
    using Complex = std::complex<float>;
    constexpr int length = EqImpulseDesigner::irLength;
    constexpr int halfLength = length / 2;

    // 0 〜 ナイキスト周波数の各ビンのゲイン(dB)を、カーブから求める。
    std::vector<float> gainDbPerBin (const EqCurve& curve, double sampleRate, float gainScale, float makeupGainDb)
    {
        std::vector<float> gains (static_cast<size_t> (halfLength + 1));

        for (int bin = 0; bin <= halfLength; ++bin)
        {
            const float frequency = static_cast<float> (bin * sampleRate / length);
            gains[static_cast<size_t> (bin)] = curve.gainDbAt (frequency) * gainScale + makeupGainDb;
        }

        return gains;
    }

    // 0〜ナイキストの値を、負の周波数側にも折り返した全 length 個のスペクトルにする。
    std::vector<Complex> mirrorToFullSpectrum (const std::vector<float>& halfSpectrum)
    {
        std::vector<Complex> full (static_cast<size_t> (length));

        for (int bin = 0; bin <= halfLength; ++bin)
        {
            full[static_cast<size_t> (bin)] = halfSpectrum[static_cast<size_t> (bin)];

            if (bin > 0 && bin < halfLength)
                full[static_cast<size_t> (length - bin)] = halfSpectrum[static_cast<size_t> (bin)];
        }

        return full;
    }

    // 位相が直線の応答。IR は 0 を中心に左右対称になるので、中央(halfLength)へずらす。
    std::vector<float> designLinearPhase (const juce::dsp::FFT& fft, const std::vector<float>& gainsDb)
    {
        std::vector<float> magnitude (gainsDb.size());
        for (size_t i = 0; i < gainsDb.size(); ++i)
            magnitude[i] = juce::Decibels::decibelsToGain (gainsDb[i]);

        const auto spectrum = mirrorToFullSpectrum (magnitude);
        std::vector<Complex> time (static_cast<size_t> (length));
        fft.perform (spectrum.data(), time.data(), true);

        std::vector<float> impulse (static_cast<size_t> (length));
        for (int n = 0; n < length; ++n)
            impulse[static_cast<size_t> ((n + halfLength) % length)] = time[static_cast<size_t> (n)].real();

        // 両端を滑らかに 0 に近づけて、切り出しによる乱れを抑える。
        for (int n = 0; n < length; ++n)
            impulse[static_cast<size_t> (n)] *= 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * static_cast<float> (n) / static_cast<float> (length)));

        return impulse;
    }

    // 最小位相の応答(ケプストラム法)。
    std::vector<float> designMinimumPhase (const juce::dsp::FFT& fft, const std::vector<float>& gainsDb)
    {
        constexpr float dbToNeper = 0.11512925f; // ln(10) / 20。dB を自然対数のゲインに直す

        std::vector<float> logMagnitude (gainsDb.size());
        for (size_t i = 0; i < gainsDb.size(); ++i)
            logMagnitude[i] = gainsDb[i] * dbToNeper;

        // 対数ゲイン → ケプストラム
        const auto logSpectrum = mirrorToFullSpectrum (logMagnitude);
        std::vector<Complex> cepstrum (static_cast<size_t> (length));
        fft.perform (logSpectrum.data(), cepstrum.data(), true);

        // 折り返し: 正の時間側だけを残して 2 倍にする(因果的な応答にする)
        std::vector<Complex> folded (static_cast<size_t> (length), Complex (0.0f, 0.0f));
        folded[0] = cepstrum[0].real();
        for (int n = 1; n < halfLength; ++n)
            folded[static_cast<size_t> (n)] = 2.0f * cepstrum[static_cast<size_t> (n)].real();
        folded[static_cast<size_t> (halfLength)] = cepstrum[static_cast<size_t> (halfLength)].real();

        // ケプストラム → 複素スペクトル → 指数を取って元のゲインと位相に戻す
        std::vector<Complex> minimumSpectrum (static_cast<size_t> (length));
        fft.perform (folded.data(), minimumSpectrum.data(), false);
        for (auto& value : minimumSpectrum)
            value = std::exp (value);

        std::vector<Complex> time (static_cast<size_t> (length));
        fft.perform (minimumSpectrum.data(), time.data(), true);

        // 後半を滑らかに減衰させ、切り出しによる乱れを抑える。
        std::vector<float> impulse (static_cast<size_t> (length));
        for (int n = 0; n < length; ++n)
        {
            float fade = 1.0f;
            if (n >= halfLength)
                fade = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * static_cast<float> (n - halfLength) / static_cast<float> (halfLength)));

            impulse[static_cast<size_t> (n)] = time[static_cast<size_t> (n)].real() * fade;
        }

        return impulse;
    }
}

juce::AudioBuffer<float> EqImpulseDesigner::design (const EqCurve& curve, double sampleRate, EqPhaseMode mode,
                                                    float gainScale, float makeupGainDb)
{
    const juce::dsp::FFT fft (fftOrder);
    const auto gainsDb = gainDbPerBin (curve, sampleRate, gainScale, makeupGainDb);

    const auto impulse = (mode == EqPhaseMode::linear) ? designLinearPhase (fft, gainsDb)
                                                       : designMinimumPhase (fft, gainsDb);

    juce::AudioBuffer<float> buffer (1, length);
    buffer.copyFrom (0, 0, impulse.data(), length);
    return buffer;
}
