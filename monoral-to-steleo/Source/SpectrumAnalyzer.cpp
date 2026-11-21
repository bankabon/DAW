#include "SpectrumAnalyzer.h"

#include <algorithm>

SpectrumAnalyzer::SpectrumAnalyzer()
    : fifoStorage (static_cast<size_t> (fifoSize), 0.0f),
      history (static_cast<size_t> (fftSize), 0.0f),
      fftWorkspace (static_cast<size_t> (fftSize) * 2, 0.0f),
      smoothedDb (static_cast<size_t> (numBins), floorDb)
{
}

void SpectrumAnalyzer::pushSamples (const float* samples, int numSamples) noexcept
{
    const auto writer = fifo.write (numSamples);

    if (writer.blockSize1 > 0)
        std::copy_n (samples, writer.blockSize1, fifoStorage.begin() + writer.startIndex1);

    if (writer.blockSize2 > 0)
        std::copy_n (samples + writer.blockSize1, writer.blockSize2, fifoStorage.begin() + writer.startIndex2);
}

void SpectrumAnalyzer::update()
{
    const int available = fifo.getNumReady();

    if (available > 0)
    {
        std::vector<float> incoming (static_cast<size_t> (available));
        const auto reader = fifo.read (available);

        std::copy_n (fifoStorage.begin() + reader.startIndex1, reader.blockSize1, incoming.begin());
        std::copy_n (fifoStorage.begin() + reader.startIndex2, reader.blockSize2, incoming.begin() + reader.blockSize1);

        // 履歴の末尾に新しいサンプルを足し、古い分を捨てて fftSize 個に保つ。
        history.insert (history.end(), incoming.begin(), incoming.end());
        history.erase (history.begin(), history.end() - fftSize);
    }

    std::copy (history.begin(), history.end(), fftWorkspace.begin());
    std::fill (fftWorkspace.begin() + fftSize, fftWorkspace.end(), 0.0f);

    window.multiplyWithWindowingTable (fftWorkspace.data(), static_cast<size_t> (fftSize));
    fft.performFrequencyOnlyForwardTransform (fftWorkspace.data());

    for (int bin = 0; bin < numBins; ++bin)
    {
        // 振幅を fftSize で割り、窓関数で下がる分(約 2 倍)を補正して dB にする。
        const float magnitude = fftWorkspace[static_cast<size_t> (bin)] * 4.0f / static_cast<float> (fftSize);
        const float levelDb = juce::Decibels::gainToDecibels (magnitude, floorDb);

        auto& shown = smoothedDb[static_cast<size_t> (bin)];
        shown = std::max (levelDb, shown - fallPerUpdateDb);
    }
}
