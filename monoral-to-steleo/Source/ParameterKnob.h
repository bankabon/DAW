#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

/** つまみ 1 つ分(つまみ・名前ラベル・パラメータとの接続)。 */
class ParameterKnob : public juce::Component
{
public:
    ParameterKnob (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId)
    {
        const auto* parameter = state.getParameter (parameterId);

        nameLabel.setText (parameter->getName (32).toUpperCase(), juce::dontSendNotification);
        nameLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        nameLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (nameLabel);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        slider.setTextValueSuffix (" " + parameter->getLabel());
        // 部品が親に取り付けられる前に作られるため、数値欄の色はここで直接決める。
        slider.setColour (juce::Slider::textBoxTextColourId, Theme::textPrimary);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterId, slider);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        nameLabel.setBounds (area.removeFromTop (20));
        slider.setBounds (area);
    }

private:
    juce::Label nameLabel;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
