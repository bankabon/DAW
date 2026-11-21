// 静的カーブと動作の確認。JUCE 非依存。 失敗があれば終了コード 1。
#include "AutoVolume.h"
#include "Compressor.h"

#include <cstdio>
#include <vector>

static int failures = 0;

static void expectNear (const char* name, float actual, float expected, float tol)
{
    const bool ok = std::abs (actual - expected) <= tol;
    std::printf ("%s %-44s actual=%8.3f expected=%8.3f\n", ok ? "PASS" : "FAIL", name, actual, expected);
    if (! ok)
        ++failures;
}

// 一定振幅のサイン波を流し、収束後のゲインリダクション(dB)を返す
static float steadyGainReductionDb (const comp::Params& p, float inputDb, float ratioOverride = -1.0f)
{
    comp::Params q = p;
    if (ratioOverride > 0.0f)
        q.ratio = ratioOverride;

    comp::Compressor c;
    c.prepare (48000.0);
    c.setParams (q);

    const float amp = std::pow (10.0f, inputDb / 20.0f);
    std::vector<float> buf (48000);
    float* ch[1] = { buf.data() };

    for (int round = 0; round < 3; ++round) // 3 秒流して収束させる
    {
        for (size_t i = 0; i < buf.size(); ++i)
            buf[i] = amp * std::sin (6.2831853f * 1000.0f * (float) i / 48000.0f);
        c.process (ch, 1, (int) buf.size());
    }
    return c.getGainReductionDb();
}

static void expectTrue (const char* name, bool cond, float a, float b)
{
    std::printf ("%s %-44s a=%8.3f b=%8.3f\n", cond ? "PASS" : "FAIL", name, a, b);
    if (! cond)
        ++failures;
}

static float sineAt (float amp, float hz, size_t i, float sr = 48000.0f)
{
    return amp * std::sin (6.2831853f * hz * (float) i / sr);
}

// 一定のサイン波を seconds 秒流して収束させたあと、無音を silenceMs 流したときの GR(dB)
static float grAfterSilence (const comp::Params& q, float inputDb, float seconds, float silenceMs)
{
    comp::Compressor c;
    c.prepare (48000.0);
    c.setParams (q);
    const float amp = std::pow (10.0f, inputDb / 20.0f);

    std::vector<float> buf ((size_t) (48000.0f * seconds));
    float* ch[1] = { buf.data() };
    for (size_t i = 0; i < buf.size(); ++i)
        buf[i] = sineAt (amp, 1000.0f, i);
    c.process (ch, 1, (int) buf.size());

    const int n = (int) (48.0f * silenceMs);
    std::vector<float> quiet ((size_t) n, 0.0f);
    float* qch[1] = { quiet.data() };
    c.process (qch, 1, n - 48);                 // 無音を流す(最後の 1 ms を残す)
    std::vector<float> tail (48, 0.0f);
    float* tch[1] = { tail.data() };
    c.process (tch, 1, 48);
    return c.getGainReductionDb();
}

// Goertzel: 指定周波数の振幅(最後の n サンプル)
static float goertzelMag (const std::vector<float>& x, float hz, float sr = 48000.0f)
{
    const double w = 2.0 * 3.14159265358979 * hz / sr;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (float v : x)
    {
        const double s0 = v + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return (float) (std::sqrt (s1 * s1 + s2 * s2 - coeff * s1 * s2) / (double) x.size() * 2.0);
}

int main()
{
    // 1) ゲインコンピュータ(ニーなし): -20 dB しきい値、4:1、入力 -8 dB → 超過 12 dB → GR = -9 dB
    expectNear ("gain computer hard knee, over 12 dB, 4:1",
                comp::gainChangeDb (-8.0f, -20.0f, 4.0f, 0.0f), -9.0f, 0.001f);

    // 2) しきい値以下は変化なし
    expectNear ("below threshold -> 0 dB",
                comp::gainChangeDb (-40.0f, -20.0f, 4.0f, 6.0f), 0.0f, 0.001f);

    // 3) ソフトニー: しきい値ちょうどでは、ニー幅 6 dB・4:1 のとき -0.5625 dB
    expectNear ("soft knee at threshold (knee 6, 4:1)",
                comp::gainChangeDb (-20.0f, -20.0f, 4.0f, 6.0f), -0.5625f, 0.001f);

    // 4) ニーの上端で、ニーなしの直線と一致する
    expectNear ("knee upper edge meets straight line",
                comp::gainChangeDb (-17.0f, -20.0f, 4.0f, 6.0f), -0.75f * 3.0f, 0.001f);

    // 5) Ratio = 1 は何も起きない
    expectNear ("ratio 1 -> 0 dB at any level",
                comp::gainChangeDb (-2.0f, -20.0f, 1.0f, 6.0f), 0.0f, 0.001f);

    // 6) 実信号: 入力 -8 dB(サイン波のピーク)、ニーなし、4:1、ピーク検波 → 約 -9 dB に収束
    comp::Params p;
    p.thresholdDb = -20.0f;
    p.ratio = 4.0f;
    p.kneeDb = 0.0f;
    p.attackMs = 1.0f;
    p.releaseMs = 50.0f;
    p.detect = 1.0f; // RMS 検波なら定常サイン波で値が揃う
    p.hpfHz = 20.0f;
    expectNear ("steady-state GR, sine -8 dB peak, 4:1",
                steadyGainReductionDb (p, -8.0f), -9.0f, 0.5f);

    // 7) Depth の考え方: ratio を 1 にすると効かない
    expectNear ("ratio 1 -> no reduction on signal",
                steadyGainReductionDb (p, -8.0f, 1.0f), 0.0f, 0.05f);

    // 7b) Auto Makeup: 収束後、出力レベルが入力レベルに近づく(GR 約 9 dB を補正)
    {
        comp::Params q = p;
        q.autoMakeup = true;
        comp::Compressor c;
        c.prepare (48000.0);
        c.setParams (q);

        const float amp = std::pow (10.0f, -8.0f / 20.0f);
        std::vector<float> buf (48000);
        float* ch[1] = { buf.data() };
        float outPeak = 0.0f;
        for (int round = 0; round < 8; ++round) // 8 秒(時定数 1 秒)
        {
            for (size_t i = 0; i < buf.size(); ++i)
                buf[i] = amp * std::sin (6.2831853f * 1000.0f * (float) i / 48000.0f);
            c.process (ch, 1, (int) buf.size());
        }
        for (size_t i = buf.size() - 4800; i < buf.size(); ++i)
            outPeak = std::max (outPeak, std::abs (buf[i]));

        expectNear ("auto makeup: output peak ~= input peak (dB)",
                    20.0f * std::log10 (outPeak), -8.0f, 0.5f);
    }

    // 8) ソフトクリップは 0 dBFS を超えない / 小さい信号は素通し
    expectNear ("softClip bounded at 1.0 (input 8.0)", comp::softClip (8.0f, 1.0f), 1.0f, 0.001f);
    expectNear ("softClip passes small signal", comp::softClip (0.2f, 1.0f), 0.2f, 0.0001f);
    expectNear ("softClip amount 0 is bypass", comp::softClip (1.7f, 0.0f), 1.7f, 0.0001f);

    // 9) Auto Release: 持続した掛かり方の後は遅く、短い山の後は速く戻る(固定 200 ms と比べる)
    {
        comp::Params f = p;
        f.attackMs = 1.0f;
        f.releaseMs = 200.0f;
        comp::Params a = f;
        a.autoRelease = true;

        const float fixedSustain = std::abs (grAfterSilence (f, -8.0f, 3.0f, 100.0f));
        const float autoSustain  = std::abs (grAfterSilence (a, -8.0f, 3.0f, 100.0f));
        expectTrue ("auto release: sustained -> slower than fixed", autoSustain > fixedSustain + 0.3f, autoSustain, fixedSustain);

        const float fixedShort = std::abs (grAfterSilence (f, -8.0f, 0.02f, 100.0f));
        const float autoShort  = std::abs (grAfterSilence (a, -8.0f, 0.02f, 100.0f));
        expectTrue ("auto release: short burst -> faster than fixed", autoShort < fixedShort - 0.3f, autoShort, fixedShort);
    }

    // 10) SHMOD: 立ち上がり 2 ms の GR は、SHMOD 4 のほうが浅い(頭が通る)
    {
        auto grAt2ms = [&] (int shmod)
        {
            comp::Params q = p;
            q.detect = 0.0f;
            q.attackMs = 20.0f;
            q.shmod = shmod;
            comp::Compressor c;
            c.prepare (48000.0);
            c.setParams (q);
            std::vector<float> buf (96);
            float* ch[1] = { buf.data() };
            for (size_t i = 0; i < buf.size(); ++i)
                buf[i] = sineAt (std::pow (10.0f, -8.0f / 20.0f), 1000.0f, i);
            c.process (ch, 1, (int) buf.size());
            return c.getGainReductionDb();
        };
        const float g0 = grAt2ms (0), g4 = grAt2ms (4);
        expectTrue ("shmod 4: shallower GR at 2 ms than shmod 0", g4 > g0 + 0.05f, g4, g0);
    }

    // 11) Ratio の急変は約 20 ms で滑らかに追う(0.5 ms 後は最終値の 60 % 未満)
    {
        comp::Params q = p;
        q.attackMs = 0.1f;
        q.detect = 1.0f;
        q.ratio = 1.0f;
        comp::Compressor c;
        c.prepare (48000.0);
        c.setParams (q);

        const float amp = std::pow (10.0f, -8.0f / 20.0f);
        std::vector<float> buf (48000);
        float* ch[1] = { buf.data() };
        for (size_t i = 0; i < buf.size(); ++i)
            buf[i] = sineAt (amp, 1000.0f, i);
        c.process (ch, 1, (int) buf.size());

        q.ratio = 20.0f;
        c.setParams (q);
        c.process (ch, 1, 24);
        const float early = std::abs (c.getGainReductionDb());
        for (int round = 0; round < 2; ++round)
        {
            for (size_t i = 0; i < buf.size(); ++i)
                buf[i] = sineAt (amp, 1000.0f, i);
            c.process (ch, 1, (int) buf.size());
        }
        const float finalGr = std::abs (c.getGainReductionDb());
        expectTrue ("ratio change is gradual (0.5 ms vs final)", early < 0.6f * finalGr, early, finalGr);
    }

    // 12) SC HF(サイドチェーン高域ブースト): 6 kHz の信号は、1 kHz より深く圧縮される
    {
        comp::Params q = p;
        q.detect = 1.0f;
        q.hpfHz = 20.0f;
        q.deessDb = 12.0f;
        auto steady = [&] (float hz, float db)
        {
            comp::Compressor c;
            c.prepare (48000.0);
            c.setParams (q);
            const float amp = std::pow (10.0f, db / 20.0f);
            std::vector<float> buf (48000);
            float* ch[1] = { buf.data() };
            for (int round = 0; round < 3; ++round)
            {
                for (size_t i = 0; i < buf.size(); ++i)
                    buf[i] = sineAt (amp, hz, i);
                c.process (ch, 1, (int) buf.size());
            }
            return c.getGainReductionDb();
        };
        const float low = steady (1000.0f, -20.0f), high = steady (6000.0f, -20.0f);
        expectTrue ("SC HF: 6 kHz compresses more than 1 kHz", high < low - 2.0f, high, low);
    }

    // 13) ADAA: 低い周波数では元のソフトクリップとほぼ同じ、高い周波数ではエイリアスが減る
    {
        float maxDiff = 0.0f, prev = 0.0f;
        for (size_t i = 0; i < 4800; ++i)
        {
            const float x = sineAt (2.0f, 100.0f, i);
            maxDiff = std::max (maxDiff, std::abs (comp::softClipAdaa (x, prev, 1.0f) - comp::softClip (x, 1.0f)));
            prev = x;
        }
        expectTrue ("ADAA ~= plain softClip at 100 Hz", maxDiff < 0.02f, maxDiff, 0.02f);

        std::vector<float> plain (48000), adaa (48000);
        prev = 0.0f;
        for (size_t i = 0; i < plain.size(); ++i)
        {
            const float x = sineAt (2.0f, 9000.0f, i);
            plain[i] = comp::softClip (x, 1.0f);
            adaa[i]  = comp::softClipAdaa (x, prev, 1.0f);
            prev = x;
        }
        // 9 kHz の 3 倍(27 kHz)は 21 kHz に折り返す
        const float aliasPlain = goertzelMag (plain, 21000.0f);
        const float aliasAdaa  = goertzelMag (adaa, 21000.0f);
        expectTrue ("ADAA reduces aliasing at 21 kHz (9 kHz tone)", aliasAdaa < 0.7f * aliasPlain, aliasAdaa, aliasPlain);
    }

    // 14) Auto Volume: 目標ピーク -5 dBFS に向けて、小さい声は上げ、大きい声は下げる
    {
        auto levelAfter = [&] (float inDb, float target, float range)
        {
            comp::AutoVolume v;
            v.prepare (48000.0);
            v.setParams (true, target, range);
            const float amp = std::pow (10.0f, inDb / 20.0f);
            std::vector<float> buf (48000);
            float* ch[1] = { buf.data() };
            float peak = 0.0f;
            for (int round = 0; round < 8; ++round)
            {
                for (size_t i = 0; i < buf.size(); ++i)
                    buf[i] = sineAt (amp, 1000.0f, i);
                v.process (ch, 1, (int) buf.size());
            }
            for (size_t i = buf.size() - 4800; i < buf.size(); ++i)
                peak = std::max (peak, std::abs (buf[i]));
            return 20.0f * std::log10 (std::max (peak, 1.0e-9f));
        };

        expectNear ("auto vol: -20 dB input -> -5 dB", levelAfter (-20.0f, -5.0f, 18.0f), -5.0f, 0.7f);
        expectNear ("auto vol: -1 dB input -> -5 dB",  levelAfter (-1.0f,  -5.0f, 18.0f), -5.0f, 0.7f);
        expectNear ("auto vol: range limit (+12 dB max)", levelAfter (-40.0f, -5.0f, 12.0f), -28.0f, 0.7f);
        expectNear ("auto vol: below gate is untouched",  levelAfter (-70.0f, -5.0f, 18.0f), -70.0f, 0.2f);

        comp::AutoVolume off;
        off.prepare (48000.0);
        off.setParams (false, -5.0f, 12.0f);
        std::vector<float> b (480, 0.1f);
        float* och[1] = { b.data() };
        off.process (och, 1, 480);
        expectNear ("auto vol: off is bypass", b[100], 0.1f, 0.0001f);
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL PASS" : "SOME FAILED");
    return failures == 0 ? 0 : 1;
}
