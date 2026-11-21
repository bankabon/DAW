#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** つまみとバイパスボタンの見た目。色はスライダーごとの rotarySliderFillColourId で決まる。 */
class CompLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CompLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffc9d1da));
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,  juce::Colour (0x558b7bff));
        setColour (juce::Label::textColourId,               juce::Colour (0xff8b95a1));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto area   = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float r     = std::min (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c      = area.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float arcR  = r - 2.0f;
        const float thick = std::max (2.5f, r * 0.11f);

        // 溝
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff0d0f12));
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 値の弧(うっすら光らせる)
        if (pos > 0.002f)
        {
            juce::Path value;
            value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
            g.setColour (accent.withAlpha (0.22f));
            g.strokePath (value, juce::PathStrokeType (thick + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // つまみ本体
        const float bodyR = r - thick - 5.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a424c), c.x, c.y - bodyR,
                                                 juce::Colour (0xff1a1e24), c.x, c.y + bodyR, false));
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (juce::Colour (0xff4b5561));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        // 指針
        const float s0 = bodyR * 0.45f, s1 = bodyR * 0.92f;
        const juce::Point<float> d (std::sin (angle), -std::cos (angle));
        g.setColour (accent);
        g.drawLine (c.x + d.x * s0, c.y + d.y * s0, c.x + d.x * s1, c.y + d.y * s1, std::max (2.0f, r * 0.07f));
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (juce::FontOptions (12.0f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);

        // テキスト付きのボタンは ON でアクセント色に光る(Auto Makeup 用)
        if (b.getButtonText().isNotEmpty())
        {
            const bool on = b.getToggleState();
            const auto accent = b.findColour (juce::ToggleButton::tickColourId);
            const auto col = on ? accent : juce::Colour (0xff59636f);
            g.setColour (on ? col.withAlpha (0.2f) : juce::Colours::transparentBlack);
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (hover ? col.brighter (0.3f) : col);
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (b.getButtonText(), r, juce::Justification::centred);
            return;
        }

        const bool bypassed = b.getToggleState();
        const auto col = bypassed ? juce::Colour (0xffe5534b) : juce::Colour (0xff59636f);

        g.setColour (bypassed ? col.withAlpha (0.22f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (hover ? col.brighter (0.3f) : col);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);

        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (bypassed ? "BYPASSED" : "ACTIVE", r, juce::Justification::centred);
    }
};
