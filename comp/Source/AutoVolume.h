#pragma once

// Auto Volume: 声の大きさを、目標のピークレベルに向けて自動でゆっくり揃える(フェーダーを動かす感覚)。
// 標準ライブラリのみ使用。レイテンシ 0(ルックアヘッドなし)。
//  - ピークの包絡を約300 msで追い、目標との差を補正ゲインにする。
//  - ゲインは「下げる」ときは速く(約30 ms)、「上げる」ときはゆっくり(約600 ms)。
//  - 補正は ±range dB まで。ノイズを持ち上げないよう、-55 dBFS より小さい間は補正を 0 dB に戻す。
//  - ルックアヘッドがないため、急な大音量の頭は一瞬 0 dBFS を超えうる(後段のリミッターで受ける想定)。

#include <algorithm>
#include <cmath>

namespace comp
{
class AutoVolume
{
public:
    static constexpr size_t maxChannels = 2;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        peakDecay = timeToCoef (300.0f);
        downCoef  = timeToCoef (30.0f);
        upCoef    = timeToCoef (600.0f);
        reset();
    }

    void reset()
    {
        env = 0.0f;
        gainDb = 0.0f;
    }

    void setParams (bool on, float targetDbfs, float rangeDb)
    {
        enabled = on;
        targetDb = targetDbfs;
        range = std::max (rangeDb, 0.0f);
    }

    void process (float* const* channels, int numChannelsIn, int numSamples)
    {
        if (! enabled && std::abs (gainDb) < 0.01f)
        {
            gainDb = 0.0f;
            return;
        }

        const size_t numChannels = std::min ((size_t) std::max (numChannelsIn, 0), maxChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (size_t c = 0; c < numChannels; ++c)
                peak = std::max (peak, std::abs (channels[c][i]));

            env = std::max (peak, env * peakDecay);
            const float envDb = 20.0f * std::log10 (std::max (env, 1.0e-6f));

            float desired = 0.0f;
            if (enabled && envDb > gateDb)
                desired = std::clamp (targetDb - envDb, -range, range);

            const float coef = desired < gainDb ? downCoef : upCoef;
            gainDb = coef * gainDb + (1.0f - coef) * desired;

            const float g = std::pow (10.0f, gainDb / 20.0f);
            for (size_t c = 0; c < numChannels; ++c)
                channels[c][i] *= g;
        }
    }

    float getGainDb() const { return gainDb; }

private:
    float timeToCoef (float ms) const { return std::exp (-1.0f / (0.001f * ms * sr)); }

    static constexpr float gateDb = -55.0f;

    float sr = 44100.0f;
    float peakDecay = 0.0f, downCoef = 0.0f, upCoef = 0.0f;
    float env = 0.0f, gainDb = 0.0f;
    float targetDb = -5.0f, range = 12.0f;
    bool enabled = false;
};
} // namespace comp
