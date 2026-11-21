#pragma once

// 1段分のコンプレッサー。標準ライブラリのみ使用(JUCE 非依存)。
// ゲインリダクションは dB 領域で計算する。ルックアヘッドなし(レイテンシ 0)。

#include <algorithm>
#include <array>
#include <cmath>

namespace comp
{
struct Params
{
    float thresholdDb = -20.0f;
    float ratio       = 2.0f;
    float kneeDb      = 6.0f;
    float attackMs    = 20.0f;
    float releaseMs   = 200.0f;
    float makeupDb    = 0.0f;
    float detect      = 0.0f;   // 0 = Peak, 1 = RMS
    float softClip    = 0.0f;   // 0..1
    float hpfHz       = 80.0f;  // サイドチェーン HPF
    float link        = 1.0f;   // 0..1 ステレオリンク
    bool  autoMakeup  = false;  // true: 平均ゲインリダクションを自動で補正する
    float deessDb     = 0.0f;   // サイドチェーンの高域(約5 kHz 以上)を持ち上げる量 0..12 dB。サ行に反応しやすくする
    bool  autoRelease = false;  // true: 掛かり方が持続しているほど Release を長く、短い山ほど短くする
    int   shmod       = 0;      // 0..4 アタックの立ち上がり形状。大きいほど最初はゆっくり、後から速く(頭を通す)
};

// 入力レベル(dB)に対するゲイン変化(dB, 0 以下)。ソフトニー付き。
inline float gainChangeDb (float levelDb, float thresholdDb, float ratio, float kneeDb)
{
    const float slope = 1.0f / ratio - 1.0f; // 0 以下
    const float over  = levelDb - thresholdDb;

    if (kneeDb <= 0.0f)
        return over > 0.0f ? slope * over : 0.0f;

    if (2.0f * over < -kneeDb)
        return 0.0f;

    if (2.0f * std::abs (over) <= kneeDb)
    {
        const float x = over + kneeDb * 0.5f;
        return slope * x * x / (2.0f * kneeDb);
    }

    return slope * over;
}

// 0 dBFS を上限に、頭だけを滑らかに丸める。amount=0 で素通し。
inline float softClip (float x, float amount)
{
    if (amount <= 0.0f)
        return x;

    const float t = 1.0f - 0.6f * amount; // 丸め始める位置
    const float a = std::abs (x);
    if (a <= t)
        return x;

    const float y = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
    return x < 0.0f ? -y : y;
}

// softClip の不定積分(偶関数)。ADAA 用。
inline double softClipAntiderivative (double x, double t)
{
    const double a = std::abs (x);
    if (a <= t)
        return 0.5 * a * a;

    const double k = 1.0 - t;
    const double u = (a - t) / k;
    const double logCosh = u + std::log1p (std::exp (-2.0 * u)) - 0.69314718055994531;
    return 0.5 * t * t + t * (a - t) + k * k * logCosh;
}

// 1次の ADAA(アンチエイリアス)付きソフトクリップ。レイテンシは増やさない。
// prev は直前のサンプル(同じチャンネル)。
inline float softClipAdaa (float x, float prev, float amount)
{
    if (amount <= 0.0f)
        return x;

    const double t = 1.0 - 0.6 * (double) amount;
    const double d = (double) x - (double) prev;
    if (std::abs (d) < 1.0e-5)
        return softClip (0.5f * (x + prev), amount);

    return (float) ((softClipAntiderivative (x, t) - softClipAntiderivative (prev, t)) / d);
}

class Compressor
{
public:
    static constexpr size_t maxChannels = 2;

    void prepare (double sampleRate)
    {
        sr = static_cast<float> (sampleRate);
        smoothCoef = 1.0f - timeToCoef (20.0f);
        reset();
    }

    void reset()
    {
        for (size_t c = 0; c < maxChannels; ++c)
        {
            meanSquare[c] = 0.0f;
            grDb[c] = 0.0f;
            slowGrDb[c] = 0.0f;
            prevClipIn[c] = 0.0f;
            hpf[c] = {};
            shelf[c] = {};
        }
        avgGrDb = 0.0f;
        meterGrDb = 0.0f;
        snapNext = true; // 次の setParams で平滑化の値を目標値に合わせる
    }

    void setParams (const Params& p)
    {
        params = p;
        attackCoef  = timeToCoef (std::max (p.attackMs,  0.01f));
        releaseCoef = timeToCoef (std::max (p.releaseMs, 1.0f));
        relFastCoef = timeToCoef (std::max (p.releaseMs * 0.35f, 1.0f));
        relSlowCoef = timeToCoef (std::max (p.releaseMs * 2.0f,  1.0f));
        rmsCoef     = timeToCoef (10.0f);
        autoCoef    = timeToCoef (1000.0f); // Auto Makeup は約1秒でゆっくり追う
        slowCoef    = timeToCoef (300.0f);  // Auto Release の「持続」判定
        updateHpf (p.hpfHz);
        updateShelf (p.deessDb);

        // しきい値、Ratio、Makeup は約20 ms で滑らかに追う(Depth やオートメーションの急変でステップ状にならないように)
        if (snapNext)
        {
            thresholdS = p.thresholdDb;
            ratioS     = p.ratio;
            makeupS    = p.makeupDb;
            snapNext   = false;
        }
    }

    // ブロック処理。channels はチャンネルごとの先頭ポインタ。
    void process (float* const* channels, int numChannelsIn, int numSamples)
    {
        const size_t numChannels = std::min ((size_t) std::max (numChannelsIn, 0), maxChannels);
        float maxGr = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            thresholdS += (params.thresholdDb - thresholdS) * smoothCoef;
            ratioS     += (params.ratio       - ratioS)     * smoothCoef;
            makeupS    += (params.makeupDb    - makeupS)    * smoothCoef;

            std::array<float, maxChannels> levelDb {};
            float loudest = -200.0f;

            for (size_t c = 0; c < numChannels; ++c)
            {
                const float x = channels[c][i];
                float sc = hpf[c].process (x, hpfCoeffs);
                if (useShelf)
                    sc = shelf[c].process (sc, shelfCoeffs);

                meanSquare[c] = rmsCoef * meanSquare[c] + (1.0f - rmsCoef) * sc * sc;
                const float peak = std::abs (sc);
                const float rms  = std::sqrt (meanSquare[c]) * 1.41421356f; // サイン波でピークと揃える
                const float det  = peak + params.detect * (rms - peak);

                levelDb[c] = 20.0f * std::log10 (std::max (det, 1.0e-6f));
                loudest = std::max (loudest, levelDb[c]);
            }

            // Auto Makeup: 平均ゲインリダクションを追い、その分を持ち上げる(瞬間値では追わない)
            float autoDb = 0.0f;
            if (params.autoMakeup)
            {
                float sum = 0.0f;
                for (size_t c = 0; c < numChannels; ++c)
                    sum += grDb[c];
                avgGrDb = autoCoef * avgGrDb + (1.0f - autoCoef) * (sum / (float) numChannels);
                autoDb = -avgGrDb;
            }

            for (size_t c = 0; c < numChannels; ++c)
            {
                const float linked = levelDb[c] + params.link * (loudest - levelDb[c]);
                const float target = gainChangeDb (linked, thresholdS, ratioS, params.kneeDb);

                float coef;
                if (target < grDb[c])
                {
                    // 掛かる方向: アタック。SHMOD > 0 では、目標に近づくほど速くする(頭を通す)
                    coef = attackCoef;
                    if (params.shmod > 0)
                    {
                        const float progress = target < -1.0e-6f ? std::clamp (grDb[c] / target, 0.0f, 1.0f) : 1.0f;
                        const float s = (float) std::min (params.shmod, 4) * 0.2125f; // 4 で 0.85
                        coef = 1.0f - (1.0f - attackCoef) * ((1.0f - s) + s * progress);
                    }
                }
                else
                {
                    // 戻る方向: リリース。Auto Release では、掛かり方が持続しているほど長くする
                    coef = releaseCoef;
                    if (params.autoRelease)
                    {
                        const float sustain = std::clamp (-slowGrDb[c] / 6.0f, 0.0f, 1.0f);
                        coef = relFastCoef + (relSlowCoef - relFastCoef) * sustain;
                    }
                }
                grDb[c] = coef * grDb[c] + (1.0f - coef) * target;
                slowGrDb[c] = slowCoef * slowGrDb[c] + (1.0f - slowCoef) * grDb[c];

                const float gain = std::pow (10.0f, (grDb[c] + autoDb + makeupS) / 20.0f);
                const float y = channels[c][i] * gain;
                channels[c][i] = softClipAdaa (y, prevClipIn[c], params.softClip);
                prevClipIn[c] = y;

                maxGr = std::min (maxGr, grDb[c]);
            }
        }

        meterGrDb = maxGr;
    }

    // 直近ブロックの最大ゲインリダクション(dB, 0 以下)。メーター用。
    float getGainReductionDb() const { return meterGrDb; }

private:
    struct Coeffs
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    };

    struct Biquad
    {
        float z1 = 0.0f, z2 = 0.0f;
        float process (float x, const Coeffs& k)
        {
            const float y = k.b0 * x + z1;
            z1 = k.b1 * x - k.a1 * y + z2;
            z2 = k.b2 * x - k.a2 * y;
            return y;
        }
    };

    float timeToCoef (float ms) const { return std::exp (-1.0f / (0.001f * ms * sr)); }

    void updateHpf (float freq)
    {
        // RBJ cookbook 2次ハイパス(Q = 0.7071)
        const float w0    = 6.28318531f * std::clamp (freq, 10.0f, sr * 0.45f) / sr;
        const float cosw  = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * 0.70710678f);
        const float a0    = 1.0f + alpha;

        hpfCoeffs.b0 = (1.0f + cosw) * 0.5f / a0;
        hpfCoeffs.b1 = -(1.0f + cosw) / a0;
        hpfCoeffs.b2 = hpfCoeffs.b0;
        hpfCoeffs.a1 = -2.0f * cosw / a0;
        hpfCoeffs.a2 = (1.0f - alpha) / a0;
    }

    void updateShelf (float gainDb)
    {
        // RBJ cookbook ハイシェルフ(約 5 kHz、S = 1)。サイドチェーン専用
        useShelf = gainDb > 0.01f;
        if (! useShelf)
        {
            shelfCoeffs = {};
            return;
        }

        const float A     = std::pow (10.0f, gainDb / 40.0f);
        const float w0    = 6.28318531f * std::min (5000.0f, sr * 0.45f) / sr;
        const float cosw  = std::cos (w0);
        const float alpha = std::sin (w0) * 0.5f * 1.41421356f;
        const float sqA2a = 2.0f * std::sqrt (A) * alpha;

        const float b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw + sqA2a);
        const float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw);
        const float b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw - sqA2a);
        const float a0 = (A + 1.0f) - (A - 1.0f) * cosw + sqA2a;
        const float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw);
        const float a2 = (A + 1.0f) - (A - 1.0f) * cosw - sqA2a;

        shelfCoeffs = { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    Params params;
    float sr = 44100.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f, relFastCoef = 0.0f, relSlowCoef = 0.0f;
    float rmsCoef = 0.0f, autoCoef = 0.0f, slowCoef = 0.0f, smoothCoef = 0.0f;
    float thresholdS = -20.0f, ratioS = 2.0f, makeupS = 0.0f;
    bool snapNext = true;
    bool useShelf = false;
    Coeffs hpfCoeffs, shelfCoeffs;
    std::array<Biquad, maxChannels> hpf {}, shelf {};
    std::array<float, maxChannels> meanSquare {}, grDb {}, slowGrDb {}, prevClipIn {};
    float avgGrDb = 0.0f;
    float meterGrDb = 0.0f;
};
} // namespace comp
