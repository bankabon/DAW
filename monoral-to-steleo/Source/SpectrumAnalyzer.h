#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>

/**
 * スペクトラム表示用の周波数解析。
 *
 * - 音声スレッドは pushSamples() でサンプルを渡すだけ(ロックもメモリ確保もしない)。
 * - 画面側は update() を一定間隔で呼び、getMagnitudesDb() で結果を読む。
 */
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBins = fftSize / 2 + 1;
    static constexpr float floorDb = -100.0f;

    SpectrumAnalyzer();

    /** 音声スレッドから呼ぶ。バッファが満杯なら新しいサンプルは捨てる。 */
    void pushSamples (const float* samples, int numSamples) noexcept;

    /** 画面側(メッセージスレッド)から呼ぶ。新しい音があればスペクトラムを更新する。 */
    void update();

    /** 各ビンのレベル(dB)。ビン番号 k の周波数は k * サンプルレート / fftSize。 */
    const std::vector<float>& getMagnitudesDb() const { return smoothedDb; }

private:
    static constexpr int fifoSize = 16384;
    static constexpr float fallPerUpdateDb = 1.5f; // 音が消えたときの下がり方

    juce::AbstractFifo fifo { fifoSize };
    std::vector<float> fifoStorage;

    std::vector<float> history;       // 直近 fftSize サンプル
    std::vector<float> fftWorkspace;  // FFT 用(fftSize * 2)
    std::vector<float> smoothedDb;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { static_cast<size_t> (fftSize), juce::dsp::WindowingFunction<float>::hann };
};
