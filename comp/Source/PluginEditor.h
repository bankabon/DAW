#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

#include "LookAndFeel.h"
#include "PluginProcessor.h"

/** ダークテーマの画面。上段に全体つまみ、下段に Stage 1 / Stage 2 を並べる。 */
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (Knob& k, const juce::String& paramId, const juce::String& text, juce::Colour accent);
    void timerCallback() override;
    juce::Rectangle<int> cardBounds (int stage) const;

    PluginProcessor& proc;
    CompLookAndFeel lnf;
    juce::ComboBox presetBox;

    Knob depth, link, inGain, outGain, mix, avolTarget;
    std::array<std::array<Knob, 11>, 2> stageKnobs; // thr ratio knee attack release makeup detect soft hpf deess shmod
    std::array<juce::ToggleButton, 2> bypass, autoGain, autoRelease;
    juce::ToggleButton abButton, avolButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> avolAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 2> autoAttachments, autoRelAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 2> bypassAttachments;
    std::array<float, 2> grShown { 0.0f, 0.0f };

    struct PeakMeter
    {
        float level = -60.0f, hold = -60.0f;
        int holdFrames = 0;

        void update (float peakLinear)
        {
            const float db = juce::Decibels::gainToDecibels (peakLinear, -60.0f);
            level = db > level ? db : std::max (db, level - 1.5f); // 落ちは緩やかに
            if (db >= hold) { hold = db; holdFrames = 45; }
            else if (--holdFrames <= 0) hold = std::max (level, hold - 1.0f);
        }
    };
    std::array<PeakMeter, 2> inMeter, outMeter;

    void drawPeakMeter (juce::Graphics&, juce::Rectangle<float>, const PeakMeter&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
