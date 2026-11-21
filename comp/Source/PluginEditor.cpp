#include "PluginEditor.h"

namespace
{
constexpr size_t numStageKnobs = 11;
constexpr const char* stageParamNames[numStageKnobs] = { "thr", "ratio", "knee", "attack", "release", "makeup", "detect", "soft", "hpf", "deess", "shmod" };
constexpr const char* stageLabels[numStageKnobs]     = { "THRESHOLD", "RATIO", "KNEE", "ATTACK", "RELEASE", "MAKEUP", "DETECT", "SOFT CLIP", "SC HPF", "SC HF", "SHMOD" };

const juce::Colour colDepth  { 0xff8b7bff };
const juce::Colour colMaster { 0xffa9b4c0 };
const std::array<juce::Colour, 2> colStage { juce::Colour (0xff4fd1c5), juce::Colour (0xfff0a04b) };

constexpr int headerH = 56, topH = 170, cardH = 170, gap = 10, margin = 16;
constexpr float grFullScaleDb = 18.0f;
} // namespace

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    abButton.setButtonText ("A/B  ORIGINAL");
    abButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe9edf2));
    addAndMakeVisible (abButton);
    abAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "ab", abButton);

    addKnob (depth,   "depth",    "DEPTH",  colDepth);
    addKnob (inGain,  "in_gain",  "INPUT",  colMaster);
    addKnob (mix,     "mix",      "MIX",    colMaster);
    addKnob (link,    "link",     "LINK",   colMaster);
    addKnob (outGain, "out_gain", "OUTPUT", colMaster);
    addKnob (avolTarget, "avol_target", "", colDepth);

    avolButton.setButtonText ("AUTO VOL");
    avolButton.setColour (juce::ToggleButton::tickColourId, colDepth);
    addAndMakeVisible (avolButton);
    avolAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "avol", avolButton);

    for (size_t s = 0; s < 2; ++s)
    {
        for (size_t i = 0; i < numStageKnobs; ++i)
            addKnob (stageKnobs[s][i], PluginProcessor::stageId ((int) s + 1, stageParamNames[i]), stageLabels[i], colStage[s]);

        addAndMakeVisible (bypass[s]);
        bypassAttachments[s] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, PluginProcessor::stageId ((int) s + 1, "bypass"), bypass[s]);
        bypass[s].onStateChange = [this] { repaint(); };

        autoGain[s].setButtonText ("AUTO MAKEUP");
        autoGain[s].setColour (juce::ToggleButton::tickColourId, colStage[s]);
        addAndMakeVisible (autoGain[s]);
        autoAttachments[s] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, PluginProcessor::stageId ((int) s + 1, "auto"), autoGain[s]);

        autoRelease[s].setButtonText ("AUTO RELEASE");
        autoRelease[s].setColour (juce::ToggleButton::tickColourId, colStage[s]);
        addAndMakeVisible (autoRelease[s]);
        autoRelAttachments[s] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, PluginProcessor::stageId ((int) s + 1, "autorel"), autoRelease[s]);
    }

    setSize (900, headerH + topH + 2 * (cardH + gap) + margin);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& text, juce::Colour accent)
{
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);

    // 値の表示に単位(dB, ms など)を付ける
    if (auto* param = proc.apvts.getParameter (paramId))
    {
        k.slider.textFromValueFunction = [param] (double v)
        {
            const auto unit = param->getLabel();
            return param->getText (param->convertTo0to1 ((float) v), 0) + (unit.isEmpty() ? "" : " " + unit);
        };
        k.slider.valueFromTextFunction = [param] (const juce::String& t)
        {
            return (double) param->convertFrom0to1 (param->getValueForText (t));
        };
        if (paramId.endsWith ("shmod")) // 0〜4 の整数表示
        {
            k.slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::lround (v)); };
            k.slider.valueFromTextFunction = [] (const juce::String& t) { return (double) t.getIntValue(); };
        }
        k.slider.updateText();
    }
}

void PluginEditor::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    const std::array<float, 2> gr { proc.stage1GrDb.load(), proc.stage2GrDb.load() };
    for (size_t i = 0; i < 2; ++i)
        grShown[i] = gr[i] < grShown[i] ? gr[i] : grShown[i] * 0.85f + gr[i] * 0.15f; // 戻りだけ緩やかに

    for (size_t c = 0; c < 2; ++c)
    {
        inMeter[c].update (proc.inPeak[c].exchange (0.0f));
        outMeter[c].update (proc.outPeak[c].exchange (0.0f));
    }

    repaint();
}

juce::Rectangle<int> PluginEditor::cardBounds (int stage) const
{
    return { margin, headerH + topH + stage * (cardH + gap), getWidth() - margin * 2, cardH };
}

void PluginEditor::drawPeakMeter (juce::Graphics& g, juce::Rectangle<float> bar, const PeakMeter& m) const
{
    constexpr float minDb = -48.0f;
    auto pos = [&] (float db) { return juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb); };

    g.setColour (juce::Colour (0xff0d0f12));
    g.fillRoundedRectangle (bar, 2.0f);

    juce::ColourGradient grad (juce::Colour (0xffe5534b), 0.0f, bar.getY(),
                               juce::Colour (0xff3ec97a), 0.0f, bar.getBottom(), false);
    grad.addColour (0.12, juce::Colour (0xffe8c33d)); // 約 -6 dB より上は黄〜赤
    grad.addColour (0.30, juce::Colour (0xff3ec97a));
    g.setGradientFill (grad);

    const float h = bar.getHeight() * pos (m.level);
    g.fillRect (bar.withTop (bar.getBottom() - h));

    // ピークホールド
    const float hy = bar.getBottom() - bar.getHeight() * pos (m.hold);
    g.setColour (m.hold > -0.5f ? juce::Colour (0xffe5534b) : juce::Colour (0xffc9d1da));
    g.fillRect (bar.getX(), hy - 1.0f, bar.getWidth(), 2.0f);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c2128), 0.0f, 0.0f,
                                             juce::Colour (0xff111418), 0.0f, (float) getHeight(), false));
    g.fillAll();

    // ヘッダー
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("TWO STAGE COMP", margin + 4, 10, 300, 26, juce::Justification::left);
    g.setColour (juce::Colour (0xff6b7683));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Gentle  >  Heavy   |   smooth serial compression", margin + 4, 34, 400, 16, juce::Justification::left);

    // 上段パネル
    const auto top = juce::Rectangle<int> (margin, headerH, getWidth() - margin * 2, topH - gap).toFloat();
    g.setColour (juce::Colour (0xff1a1f26));
    g.fillRoundedRectangle (top, 10.0f);
    g.setColour (juce::Colour (0xff2a313a));
    g.drawRoundedRectangle (top, 10.0f, 1.0f);

    // 入出力ピークメーター
    {
        const float mTop = top.getY() + 26.0f, mBottom = top.getBottom() - 30.0f;
        const float barW = 9.0f;
        const float x0 = 392.0f;
        const float xs[4] = { x0, x0 + barW + 3.0f, x0 + 2.0f * barW + 34.0f, x0 + 3.0f * barW + 37.0f };

        drawPeakMeter (g, { xs[0], mTop, barW, mBottom - mTop }, inMeter[0]);
        drawPeakMeter (g, { xs[1], mTop, barW, mBottom - mTop }, inMeter[1]);
        drawPeakMeter (g, { xs[2], mTop, barW, mBottom - mTop }, outMeter[0]);
        drawPeakMeter (g, { xs[3], mTop, barW, mBottom - mTop }, outMeter[1]);

        g.setColour (juce::Colour (0xff8b95a1));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText ("IN",  (int) x0 - 4,  (int) mBottom + 4, 2 * (int) barW + 11, 14, juce::Justification::centred);
        g.drawText ("OUT", (int) xs[2] - 4, (int) mBottom + 4, 2 * (int) barW + 11, 14, juce::Justification::centred);

        // 目盛り(0, -12, -24, -36 dB)
        g.setFont (juce::FontOptions (9.0f));
        g.setColour (juce::Colour (0xff59636f));
        for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
        {
            const float y = mBottom - (mBottom - mTop) * (db + 48.0f) / 48.0f;
            g.drawText (juce::String ((int) db), (int) (xs[1] + barW + 1.0f), (int) y - 6, 22, 12, juce::Justification::centred);
        }
    }

    // Depth に応じた各段の立ち上がり表示
    const float d  = (float) depth.slider.getValue() * 0.01f;
    const float w1 = juce::jlimit (0.0f, 1.0f, d * 2.0f);
    const float w2 = juce::jlimit (0.0f, 1.0f, (d - 0.5f) * 2.0f);
    const float ws[2] = { w1, w2 };
    const float bx = 230.0f, bw = 150.0f;
    for (int s = 0; s < 2; ++s)
    {
        const float by = top.getY() + 52.0f + (float) s * 40.0f;
        g.setColour (juce::Colour (0xff8b95a1));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (s == 0 ? "STAGE 1" : "STAGE 2", (int) bx, (int) by - 16, 100, 14, juce::Justification::left);
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (bx, by, bw, 8.0f, 4.0f);
        g.setColour (colStage[(size_t) s]);
        g.fillRoundedRectangle (bx, by, bw * ws[s], 8.0f, 4.0f);
    }

    // 段カード
    const char* names[2] = { "STAGE 1  ·  GENTLE", "STAGE 2  ·  HEAVY" };
    for (int s = 0; s < 2; ++s)
    {
        const auto card = cardBounds (s).toFloat();
        g.setColour (juce::Colour (0xff1a1f26));
        g.fillRoundedRectangle (card, 10.0f);
        g.setColour (juce::Colour (0xff2a313a));
        g.drawRoundedRectangle (card, 10.0f, 1.0f);

        const auto accent = colStage[(size_t) s];
        g.setColour (accent);
        g.fillRoundedRectangle (card.getX() + 14.0f, card.getY() + 14.0f, 4.0f, 18.0f, 2.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (names[s], (int) card.getX() + 26, (int) card.getY() + 12, 220, 22, juce::Justification::left);

        // ゲインリダクション(横バー)
        const auto bar = juce::Rectangle<float> (card.getRight() - 512.0f, card.getY() + 18.0f, 140.0f, 10.0f);
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (bar, 5.0f);
        const float amount = juce::jlimit (0.0f, 1.0f, -grShown[(size_t) s] / grFullScaleDb);
        g.setColour (accent);
        g.fillRoundedRectangle (bar.withWidth (std::max (0.0f, bar.getWidth() * amount)), 5.0f);
        g.setColour (juce::Colour (0xff8b95a1));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText ("GR", (int) bar.getX() - 28, (int) bar.getY() - 3, 24, 16, juce::Justification::right);
        g.setColour (juce::Colour (0xffc9d1da));
        g.drawText (juce::String (grShown[(size_t) s], 1) + " dB", (int) bar.getRight() + 6, (int) bar.getY() - 3, 56, 16, juce::Justification::left);
    }
}

void PluginEditor::paintOverChildren (juce::Graphics& g)
{
    // バイパス中の段は、ヘッダー以外を暗くする(操作は通常どおり可能)
    for (size_t s = 0; s < 2; ++s)
        if (bypass[s].getToggleState())
        {
            auto body = cardBounds ((int) s);
            body.removeFromTop (40);
            g.setColour (juce::Colour (0xaa111418));
            g.fillRoundedRectangle (body.toFloat().reduced (1.0f), 8.0f);
        }
}

void PluginEditor::resized()
{
    auto place = [] (Knob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (14));
        k.slider.setBounds (r);
    };

    // 上段: Depth(大)+ 小つまみ4つ
    presetBox.setBounds (getWidth() - margin - 260, 14, 260, 28);
    abButton.setBounds (getWidth() - margin - 260 - 148, 14, 140, 28);
    const int ty = headerH + 6;
    place (depth, { margin + 14, ty, 180, topH - gap - 12 });

    Knob* small[5] = { &inGain, &mix, &link, &outGain, &avolTarget };
    for (int i = 0; i < 5; ++i)
        place (*small[i], { 478 + i * 78, ty + 20, 76, topH - gap - 40 });

    // Auto Vol は、目標つまみの上のラベル位置にスイッチを置く
    avolButton.setBounds (478 + 4 * 78 + 2, ty + 20, 72, 16);

    // 各段
    for (int s = 0; s < 2; ++s)
    {
        const auto card = cardBounds (s);
        bypass[(size_t) s].setBounds (card.getRight() - 92, card.getY() + 12, 80, 22);
        autoGain[(size_t) s].setBounds (card.getRight() - 196, card.getY() + 12, 96, 22);
        autoRelease[(size_t) s].setBounds (card.getRight() - 300, card.getY() + 12, 96, 22);

        const int kw = (card.getWidth() - 20) / (int) numStageKnobs;
        for (int i = 0; i < (int) numStageKnobs; ++i)
            place (stageKnobs[(size_t) s][(size_t) i], { card.getX() + 10 + i * kw, card.getY() + 42, kw, card.getHeight() - 48 });
    }
}
