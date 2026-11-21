#pragma once

#include <array>
#include <memory>
#include "EqCurveComponent.h"
#include "ModernLookAndFeel.h"
#include "ParameterKnob.h"
#include "PluginProcessor.h"
#include "PresetManager.h"

/**
 * プラグインの画面。
 *
 *   上段 : プリセット選択・保存、Mono Check
 *   EQ 操作列 : Undo / Redo / Clear EQ、EQ Amount、Auto Gain、EQ Bypass、EQ Phase
 *   中段 : EQ カーブのグラフ(点を直接操作)
 *   下段 : 広がり処理のつまみ
 */
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::ValueTree::Listener,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor& owningProcessor);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // 保存データの EQ カーブが変わったら、グラフに反映する。
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override;
    void timerCallback() override; // スペクトラム表示の更新

    void refreshCurveFromState();
    void refreshPresetList();
    void askForPresetNameAndSave();

    PluginProcessor& pluginProcessor;
    bool isWritingCurve = false; // グラフ操作による書き込み中は true
    PresetManager presetManager;

    ModernLookAndFeel lookAndFeel;
    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> knobPanelArea;

    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Save" };
    juce::ToggleButton monoCheckButton { "Mono Check" };

    // EQ 操作列
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton clearEqButton { "Clear EQ" };
    juce::Label amountLabel { {}, "EQ AMOUNT" };
    juce::Slider amountSlider;
    juce::ToggleButton autoGainButton { "Auto Gain" };
    juce::ToggleButton bypassButton { "EQ Bypass" };
    juce::Label phaseLabel { {}, "EQ PHASE" };
    juce::ComboBox phaseBox;

    EqCurveComponent eqCurveComponent;

    static constexpr int numKnobs = 7;
    std::array<std::unique_ptr<ParameterKnob>, numKnobs> knobs;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> phaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoCheckAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
