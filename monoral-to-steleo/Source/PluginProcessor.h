#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "EqImpulseDesigner.h"
#include "SpectrumAnalyzer.h"
#include "StereoWidener.h"

/**
 * プラグイン本体。
 * 処理の流れ: 入力をモノラルにまとめる → EQ(畳み込み)→ StereoWidener で左右に広げる → (Mono Check なら左右を合算)
 *
 * EQ カーブは保存データ(parameters.state)の中にある。カーブや位相モードが変わったら、
 * メッセージスレッドでインパルス応答を作り直して Convolution に渡す。
 */
class PluginProcessor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::ValueTree::Listener,
                        private juce::AsyncUpdater
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::UndoManager undoManager; // EQ の点の編集やつまみ操作を元に戻すための履歴(parameters より先に作る)
    juce::AudioProcessorValueTreeState parameters;
    SpectrumAnalyzer inputSpectrumAnalyzer;  // 画面のスペクトラム表示用(EQ に入る前の音)
    SpectrumAnalyzer outputSpectrumAnalyzer; // 同上(EQ を通った後の音)

private:
    // 値の変化を受け取り、EQ の作り直しを予約する。
    void parameterChanged (const juce::String& parameterId, float newValue) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override;
    void handleAsyncUpdate() override;

    static bool isEqCurveNode (const juce::ValueTree& tree);
    void rebuildEqFilter();

    WidenerSettings readWidenerSettings() const;
    float readParameter (const char* id) const;
    void mixInputDownToMono (const juce::AudioBuffer<float>& buffer, int startSample, int count);

    StereoWidener widener;
    juce::dsp::Convolution eqConvolution; // EQ(カーブから作ったインパルス応答を畳み込む)
    double currentSampleRate = 44100.0;
    std::vector<float> monoScratch; // 入力をモノラルにまとめた作業用バッファ

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
