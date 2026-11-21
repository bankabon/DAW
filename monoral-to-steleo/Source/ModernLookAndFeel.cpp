#include "ModernLookAndFeel.h"
#include "Theme.h"

ModernLookAndFeel::ModernLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       Theme::textPrimary);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  Theme::accentSoft);

    setColour (juce::Slider::thumbColourId,             Theme::textPrimary);
    setColour (juce::Slider::trackColourId,             Theme::accent);
    setColour (juce::Slider::backgroundColourId,        Theme::grid);

    setColour (juce::Label::textColourId,               Theme::textMuted);

    setColour (juce::ComboBox::textColourId,            Theme::textPrimary);
    setColour (juce::ComboBox::arrowColourId,           Theme::textMuted);

    setColour (juce::PopupMenu::backgroundColourId,            Theme::panel);
    setColour (juce::PopupMenu::textColourId,                  Theme::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accentSoft);
    setColour (juce::PopupMenu::highlightedTextColourId,       Theme::textPrimary);

    setColour (juce::TextButton::textColourOffId,       Theme::textPrimary);
    setColour (juce::TextButton::textColourOnId,        Theme::textPrimary);
    setColour (juce::ToggleButton::textColourId,        Theme::textPrimary);
}

void ModernLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
    const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = diameter * 0.5f;
    const float trackThickness = 4.0f;
    const float arcRadius = radius - trackThickness * 0.5f;
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    const bool isActive = slider.isMouseOverOrDragging() && slider.isEnabled();

    // つまみ本体
    const auto body = juce::Rectangle<float> (diameter, diameter).withCentre (centre).reduced (trackThickness + 4.0f);
    g.setGradientFill (juce::ColourGradient (Theme::gridStrong, body.getCentreX(), body.getY(),
                                             Theme::panel, body.getCentreX(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Theme::panelBorder);
    g.drawEllipse (body, 1.0f);

    // 背景の弧
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::grid);
    g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 値の弧(光らせて見せる)
    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, valueAngle, true);

    g.setColour (Theme::accent.withAlpha (isActive ? 0.35f : 0.18f));
    g.strokePath (valueArc, juce::PathStrokeType (trackThickness + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Theme::accent);
    g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 針
    const float pointerInner = body.getWidth() * 0.5f * 0.45f;
    const float pointerOuter = body.getWidth() * 0.5f * 0.85f;
    juce::Path pointer;
    pointer.startNewSubPath (0.0f, -pointerInner);
    pointer.lineTo (0.0f, -pointerOuter);
    g.setColour (Theme::textPrimary);
    g.strokePath (pointer, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::rotation (valueAngle).translated (centre));
}

void ModernLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    const bool isHot = box.isMouseOver() || isButtonDown;

    g.setColour (isHot ? Theme::gridStrong : Theme::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (isHot ? Theme::accentSoft : Theme::panelBorder);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    // 右端の「v」マーク
    juce::Path arrow;
    const float cx = static_cast<float> (width) - 16.0f;
    const float cy = static_cast<float> (height) * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (Theme::textMuted);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ModernLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                              bool isHighlighted, bool isDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (isDown ? Theme::accentSoft : (isHighlighted ? Theme::gridStrong : Theme::panel));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (isHighlighted ? Theme::accentSoft : Theme::panelBorder);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}

void ModernLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool isHighlighted, bool)
{
    // スイッチ型のトグル
    const auto area = button.getLocalBounds().toFloat();
    const float switchWidth = 34.0f;
    const float switchHeight = 18.0f;
    const auto track = juce::Rectangle<float> (switchWidth, switchHeight).withCentre ({ area.getX() + switchWidth * 0.5f + 2.0f, area.getCentreY() });
    const bool isOn = button.getToggleState();

    g.setColour (isOn ? Theme::accent.withAlpha (0.85f) : Theme::grid);
    g.fillRoundedRectangle (track, switchHeight * 0.5f);
    g.setColour (isHighlighted ? Theme::accentSoft : Theme::panelBorder);
    g.drawRoundedRectangle (track, switchHeight * 0.5f, 1.0f);

    const float knobSize = switchHeight - 6.0f;
    const float knobX = isOn ? track.getRight() - knobSize - 3.0f : track.getX() + 3.0f;
    g.setColour (isOn ? Theme::background : Theme::textMuted);
    g.fillEllipse (knobX, track.getCentreY() - knobSize * 0.5f, knobSize, knobSize);

    g.setColour (isOn ? Theme::textPrimary : Theme::textMuted);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (button.getButtonText(),
                juce::Rectangle<float> (track.getRight() + 8.0f, area.getY(), area.getRight() - track.getRight() - 8.0f, area.getHeight()),
                juce::Justification::centredLeft);
}

juce::Font ModernLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (14.0f));
}

juce::Font ModernLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (13.0f));
}
