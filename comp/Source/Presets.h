#pragma once

#include <array>

/**
 * ボーカル用プリセット。値は「出発点」で、実際の声で聴いて Threshold と Depth を調整する前提。
 * 考え方:
 *  - 女性: 基音が高め(約 180〜250 Hz)。サイドチェーン HPF は高めにして、息や低域のノイズで掛からないようにする。
 *  - 男性: 基音が低め(約 90〜150 Hz)。HPF は基音より下に置き、声の厚みでコンプが動くようにする。
 *  - Stage 1 は薄く遅め(1〜3 dB)、Stage 2 は速めで深め(3〜6 dB)。
 *  - 追加の3つ(Rap / Whisper / Backing)は男女共通。
 * deess は SC HF(サイドチェーンの高域ブースト dB)、autoRel は Auto Release(0/1)、shmod は 0〜4。
 */
namespace presets
{
struct Stage
{
    float thr, ratio, knee, attack, release, makeup, detectPct, softPct, hpf;
    float deess, autoRel, shmod;
};

struct Preset
{
    const char* name;
    float depthPct;
    Stage s1, s2;
};

inline constexpr std::array<Preset, 10> all {{
    { "Init",                      100.0f, { -20.0f, 1.8f, 12.0f, 30.0f, 250.0f, 0.0f, 40.0f,  0.0f,  80.0f, 0.0f, 0.0f, 0.0f },
                                           { -24.0f, 4.0f,  8.0f,  8.0f, 120.0f, 0.0f, 20.0f, 30.0f, 120.0f, 0.0f, 0.0f, 0.0f } },

    { "Female - Natural",          100.0f, { -22.0f, 1.6f, 12.0f, 25.0f, 220.0f, 0.0f, 50.0f,  0.0f, 120.0f, 0.0f, 0.0f, 0.0f },
                                           { -26.0f, 2.5f, 10.0f, 12.0f, 150.0f, 1.5f, 30.0f, 15.0f, 150.0f, 0.0f, 0.0f, 0.0f } },

    { "Female - Pop Bright",       100.0f, { -24.0f, 2.0f, 10.0f, 15.0f, 180.0f, 0.0f, 40.0f,  0.0f, 120.0f, 0.0f, 0.0f, 0.0f },
                                           { -28.0f, 4.0f,  8.0f,  6.0f, 100.0f, 3.0f, 20.0f, 30.0f, 160.0f, 0.0f, 0.0f, 0.0f } },

    { "Female - Soft Ballad",      100.0f, { -20.0f, 1.5f, 14.0f, 35.0f, 300.0f, 0.0f, 60.0f,  0.0f, 110.0f, 0.0f, 0.0f, 0.0f },
                                           { -24.0f, 2.2f, 12.0f, 15.0f, 200.0f, 1.0f, 50.0f, 10.0f, 130.0f, 0.0f, 0.0f, 0.0f } },

    { "Male - Natural",            100.0f, { -22.0f, 1.7f, 12.0f, 30.0f, 250.0f, 0.0f, 50.0f,  0.0f,  80.0f, 0.0f, 0.0f, 0.0f },
                                           { -26.0f, 2.8f, 10.0f, 12.0f, 160.0f, 1.5f, 30.0f, 15.0f, 100.0f, 0.0f, 0.0f, 0.0f } },

    { "Male - Pop/Rock Punch",     100.0f, { -24.0f, 2.2f, 10.0f, 20.0f, 200.0f, 0.0f, 40.0f,  0.0f,  90.0f, 0.0f, 0.0f, 0.0f },
                                           { -28.0f, 4.5f,  8.0f,  8.0f, 110.0f, 3.0f, 20.0f, 30.0f, 110.0f, 0.0f, 0.0f, 0.0f } },

    { "Male - Warm Ballad",        100.0f, { -20.0f, 1.5f, 14.0f, 40.0f, 320.0f, 0.0f, 60.0f,  0.0f,  70.0f, 0.0f, 0.0f, 0.0f },
                                           { -24.0f, 2.3f, 12.0f, 18.0f, 220.0f, 1.0f, 50.0f, 10.0f,  85.0f, 0.0f, 0.0f, 0.0f } },

    // 密度高め。サ行に反応しやすくして、頭は少し通す(SHMOD)
    { "Vocal - Rap / Dense",       100.0f, { -26.0f, 2.0f,  8.0f, 10.0f, 120.0f, 0.0f, 30.0f,  0.0f, 100.0f, 3.0f, 0.0f, 1.0f },
                                           { -30.0f, 5.0f,  6.0f,  4.0f,  70.0f, 3.0f, 15.0f, 35.0f, 130.0f, 4.0f, 0.0f, 2.0f } },

    // 小さい声。Threshold が低く、サ行や息が目立つので SC HF を強め
    { "Vocal - Whisper / Soft",    100.0f, { -32.0f, 2.0f, 14.0f, 25.0f, 200.0f, 2.0f, 70.0f,  0.0f, 120.0f, 6.0f, 1.0f, 0.0f },
                                           { -36.0f, 2.5f, 12.0f, 15.0f, 150.0f, 2.0f, 60.0f,  5.0f, 150.0f, 8.0f, 1.0f, 0.0f } },

    // 重ねた声。Auto Release でまとまりよく
    { "Vocal - Backing / Choir",   100.0f, { -22.0f, 2.0f, 12.0f, 20.0f, 200.0f, 0.0f, 60.0f,  0.0f, 100.0f, 2.0f, 1.0f, 0.0f },
                                           { -26.0f, 3.5f, 10.0f, 10.0f, 130.0f, 2.0f, 40.0f, 20.0f, 120.0f, 2.0f, 1.0f, 1.0f } },
}};
} // namespace presets
