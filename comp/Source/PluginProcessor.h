#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

#include "AutoVolume.h"
#include "Compressor.h"

/**
 * 2段がけコンプ。Input → Stage 1 → Stage 2 → Output → Mix。
 * Depth で「薄く(Stage 1 のみ)→ 厚く(Stage 2 も追加)」を1つのつまみで動かす。
 */
class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int maxBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // プリセット(DAW のプリセット一覧にも出る)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // メーター用(エディタが読む)。dB、0 以下。
    std::atomic<float> stage1GrDb { 0.0f };
    std::atomic<float> stage2GrDb { 0.0f };

    // 入出力ピーク(リニア値)。エディタが読むたびに 0 へ戻す(exchange)ので、読み取り間のピークを取りこぼさない。
    std::atomic<float> inPeak[2]  { 0.0f, 0.0f };
    std::atomic<float> outPeak[2] { 0.0f, 0.0f };

    static juce::String stageId (int stage, const char* name) { return "s" + juce::String (stage) + "_" + name; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    comp::Params readStage (int stage, float depthWeight) const;
    float raw (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    void setParam (const juce::String& id, float value);
    static void storeMax (std::atomic<float>& a, float v)
    {
        float cur = a.load();
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    }
    void measure (std::atomic<float> (&dest)[2], const juce::AudioBuffer<float>& b, int numChannels)
    {
        for (int c = 0; c < numChannels; ++c)
            storeMax (dest[c], b.getMagnitude (c, 0, b.getNumSamples()));
    }

    std::atomic<int> currentProgram { 0 };
    double currentSampleRate = 44100.0;
    float prevInGain = 1.0f, prevOutGain = 1.0f, prevMix = 1.0f;
    float dryPower = 0.0f, wetPower = 0.0f, prevMatchGain = 1.0f; // A/B の音量合わせ用
    float abAmount = 0.0f, abSmoothCoef = 0.01f;                  // 0 = コンプ後、1 = 原音(音量補正済み)
    comp::Compressor stage1, stage2;
    comp::AutoVolume autoVol;
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
