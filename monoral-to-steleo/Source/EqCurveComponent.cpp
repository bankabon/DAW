#include "EqCurveComponent.h"
#include "Theme.h"

namespace
{
    constexpr float pointRadius = 6.0f;
    constexpr float pointHitRadius = 11.0f;
    constexpr float leftMargin = 38.0f;
    constexpr float bottomMargin = 20.0f;
    constexpr float topMargin = 8.0f;
    constexpr float rightMargin = 10.0f;
    constexpr float gainStepDb = 0.1f;

    const juce::Colour backgroundColour = Theme::panel;
    const juce::Colour gridColour       = Theme::grid;
    const juce::Colour textColour       = Theme::textMuted;
    const juce::Colour curveColour      = Theme::curve;
    const juce::Colour spectrumColour   = Theme::accent;
}

EqCurveComponent::EqCurveComponent (const SpectrumAnalyzer& inputToShow, const SpectrumAnalyzer& outputToShow)
    : inputAnalyzer (inputToShow), outputAnalyzer (outputToShow)
{
}

void EqCurveComponent::setCurve (const EqCurve& newCurve)
{
    curve = newCurve;

    // 点の数が変わっていたら、操作中の点の番号は無効になる。
    if (dragIndex >= curve.getNumPoints()) dragIndex = -1;
    if (hoverIndex >= curve.getNumPoints()) hoverIndex = -1;

    repaint();
}

void EqCurveComponent::setDisplayState (float newAmount, bool newBypassed)
{
    amount = newAmount;
    bypassed = newBypassed;
}

//==============================================================================
// 座標の変換。横軸は周波数(対数)、縦軸はゲイン(±EqCurve::maxGainDb)。

juce::Rectangle<float> EqCurveComponent::getPlotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (leftMargin).withTrimmedRight (rightMargin)
                                     .withTrimmedTop (topMargin).withTrimmedBottom (bottomMargin);
}

float EqCurveComponent::frequencyToX (float frequencyHz) const
{
    const auto area = getPlotArea();
    const float position = std::log (frequencyHz / EqCurve::minFrequencyHz)
                         / std::log (EqCurve::maxFrequencyHz / EqCurve::minFrequencyHz);
    return area.getX() + area.getWidth() * position;
}

float EqCurveComponent::xToFrequency (float x) const
{
    const auto area = getPlotArea();
    const float position = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / area.getWidth());
    return EqCurve::minFrequencyHz * std::pow (EqCurve::maxFrequencyHz / EqCurve::minFrequencyHz, position);
}

float EqCurveComponent::gainToY (float gainDb) const
{
    const auto area = getPlotArea();
    return area.getCentreY() - gainDb / EqCurve::maxGainDb * (area.getHeight() * 0.5f);
}

float EqCurveComponent::yToGain (float y) const
{
    const auto area = getPlotArea();
    const float gain = (area.getCentreY() - y) / (area.getHeight() * 0.5f) * EqCurve::maxGainDb;
    const float stepped = std::round (gain / gainStepDb) * gainStepDb;
    return juce::jlimit (-EqCurve::maxGainDb, EqCurve::maxGainDb, stepped);
}

juce::Point<float> EqCurveComponent::pointToPosition (const EqPoint& point) const
{
    return { frequencyToX (point.frequencyHz), gainToY (point.gainDb) };
}

EqPoint EqCurveComponent::positionToPoint (juce::Point<float> position) const
{
    return { xToFrequency (position.x), yToGain (position.y) };
}

int EqCurveComponent::findPointAt (juce::Point<float> position) const
{
    int nearest = -1;
    float nearestDistance = pointHitRadius;

    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const float distance = pointToPosition (curve.getPoints()[static_cast<size_t> (i)]).getDistanceFrom (position);

        if (distance <= nearestDistance)
        {
            nearest = i;
            nearestDistance = distance;
        }
    }

    return nearest;
}

juce::String EqCurveComponent::formatFrequency (float frequencyHz)
{
    if (frequencyHz >= 1000.0f)
        return juce::String (frequencyHz / 1000.0f, 2) + " kHz";

    return juce::String (juce::roundToInt (frequencyHz)) + " Hz";
}

//==============================================================================
// 描画

void EqCurveComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (backgroundColour);
    g.fillRoundedRectangle (bounds, 12.0f);

    const auto area = getPlotArea();

    // グラフの外にはみ出さないように、角丸の枠で切り抜く。
    juce::Path clip;
    clip.addRoundedRectangle (bounds, 12.0f);
    g.saveState();
    g.reduceClipRegion (clip);

    drawGrid (g, area);
    drawSpectrum (g, area);
    drawCurve (g, area);
    drawPoints (g);
    g.restoreState();

    g.setColour (Theme::panelBorder);
    g.drawRoundedRectangle (bounds, 12.0f, 1.0f);
}

void EqCurveComponent::drawGrid (juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setFont (juce::FontOptions (11.0f));

    // 横線: ゲイン(3 dB ごと)
    for (float gain = -EqCurve::maxGainDb; gain <= EqCurve::maxGainDb + 0.01f; gain += 3.0f)
    {
        const float y = gainToY (gain);
        const bool isZero = std::abs (gain) < 0.01f;

        g.setColour (isZero ? gridColour.brighter (0.5f) : gridColour);
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());

        g.setColour (textColour);
        g.drawText (juce::String (juce::roundToInt (gain)), 0, juce::roundToInt (y) - 7,
                    static_cast<int> (leftMargin) - 6, 14, juce::Justification::centredRight);
    }

    // 縦線: 周波数
    struct Mark { float frequencyHz; const char* label; };
    const Mark marks[] = { { 20.0f, "20" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" },
                           { 500.0f, "500" }, { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" },
                           { 10000.0f, "10k" }, { 20000.0f, "20k" } };

    for (const auto& mark : marks)
    {
        const float x = frequencyToX (mark.frequencyHz);
        g.setColour (gridColour);
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());

        g.setColour (textColour);
        g.drawText (mark.label, juce::roundToInt (x) - 20, juce::roundToInt (area.getBottom()) + 3,
                    40, 14, juce::Justification::centred);
    }
}

// スペクトラムの輪郭を、グラフの下端から始まる閉じた形の Path にして返す。
juce::Path EqCurveComponent::buildSpectrumPath (const SpectrumAnalyzer& source, juce::Rectangle<float> area) const
{
    const auto& levels = source.getMagnitudesDb();
    const float binsPerHz = static_cast<float> (SpectrumAnalyzer::fftSize / sampleRate);
    constexpr float stepPixels = 2.0f;

    juce::Path path;
    path.startNewSubPath (area.getX(), area.getBottom());

    for (float x = area.getX(); x <= area.getRight(); x += stepPixels)
    {
        // このピクセルが受け持つ周波数範囲のうち、一番大きいビンを表示する。
        const int firstBin = juce::jlimit (0, SpectrumAnalyzer::numBins - 1, static_cast<int> (xToFrequency (x) * binsPerHz));
        const int lastBin  = juce::jlimit (firstBin, SpectrumAnalyzer::numBins - 1, static_cast<int> (xToFrequency (x + stepPixels) * binsPerHz));

        float level = SpectrumAnalyzer::floorDb;
        for (int bin = firstBin; bin <= lastBin; ++bin)
            level = std::max (level, levels[static_cast<size_t> (bin)]);

        // -100 dB 〜 0 dB を、グラフの下端〜上端に対応させる。
        const float position = juce::jlimit (0.0f, 1.0f, (level - SpectrumAnalyzer::floorDb) / -SpectrumAnalyzer::floorDb);
        path.lineTo (x, area.getBottom() - position * area.getHeight());
    }

    path.lineTo (area.getRight(), area.getBottom());
    path.closeSubPath();
    return path;
}

void EqCurveComponent::drawSpectrum (juce::Graphics& g, juce::Rectangle<float> area) const
{
    // 出力(EQ 通過後)は塗りつぶし、入力(EQ 通過前)は白い輪郭線で重ねる。
    // EQ で持ち上げた帯域では、塗りが輪郭線より上に出る。
    g.setGradientFill (juce::ColourGradient (spectrumColour.withAlpha (0.35f), 0.0f, area.getY(),
                                             spectrumColour.withAlpha (0.02f), 0.0f, area.getBottom(), false));
    g.fillPath (buildSpectrumPath (outputAnalyzer, area));

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.strokePath (buildSpectrumPath (inputAnalyzer, area), juce::PathStrokeType (1.2f));

    // 凡例
    g.setFont (juce::FontOptions (11.0f));
    const auto legend = juce::Rectangle<float> (area.getRight() - 150.0f, area.getY() + 6.0f, 144.0f, 16.0f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillRect (legend.getX(), legend.getCentreY() - 0.5f, 14.0f, 1.5f);
    g.drawText ("Input", legend.withTrimmedLeft (20.0f).withWidth (50.0f), juce::Justification::centredLeft);
    g.setColour (spectrumColour.withAlpha (0.8f));
    g.fillRect (legend.getX() + 74.0f, legend.getCentreY() - 4.0f, 14.0f, 8.0f);
    g.drawText ("Output", legend.withTrimmedLeft (94.0f), juce::Justification::centredLeft);
}

juce::Path EqCurveComponent::buildCurvePath (juce::Rectangle<float> area, float gainScale) const
{
    juce::Path path;
    bool isFirstPoint = true;

    for (float x = area.getX(); x <= area.getRight(); x += 2.0f)
    {
        const float y = gainToY (curve.gainDbAt (xToFrequency (x)) * gainScale);

        if (isFirstPoint)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);

        isFirstPoint = false;
    }

    return path;
}

void EqCurveComponent::drawCurve (juce::Graphics& g, juce::Rectangle<float> area) const
{
    // バイパス中は、効いていないことが分かるように薄く描く。
    const float visibility = bypassed ? 0.3f : 1.0f;
    const bool isScaled = std::abs (amount - 1.0f) > 0.005f;

    // EQ Amount が 100% でないときは、点の位置を示す元のカーブを細く薄く描き、
    // 実際に効くカーブ(ゲインを amount 倍したもの)を太く描く。
    if (isScaled)
    {
        g.setColour (curveColour.withAlpha (0.35f * visibility));
        g.strokePath (buildCurvePath (area, 1.0f), juce::PathStrokeType (1.2f));
    }

    const auto path = buildCurvePath (area, amount);

    // カーブと 0 dB の線の間を薄く塗り、ブーストとカットが見分けやすいようにする。
    juce::Path filled (path);
    filled.lineTo (area.getRight(), gainToY (0.0f));
    filled.lineTo (area.getX(), gainToY (0.0f));
    filled.closeSubPath();
    g.setColour (curveColour.withAlpha (0.10f * visibility));
    g.fillPath (filled);

    g.setColour (curveColour.withAlpha (0.25f * visibility));
    g.strokePath (path, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (curveColour.withAlpha (visibility));
    g.strokePath (path, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EqCurveComponent::drawPoints (juce::Graphics& g) const
{
    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const auto& point = curve.getPoints()[static_cast<size_t> (i)];
        const auto centre = pointToPosition (point);
        const bool isActive = (i == hoverIndex || i == dragIndex);
        const float radius = isActive ? pointRadius + 2.0f : pointRadius;

        const float glow = radius + 5.0f;
        g.setColour (curveColour.withAlpha (isActive ? 0.35f : 0.15f));
        g.fillEllipse (centre.x - glow, centre.y - glow, glow * 2.0f, glow * 2.0f);

        g.setColour (isActive ? juce::Colours::white : curveColour);
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (backgroundColour);
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        if (isActive)
        {
            const auto text = formatFrequency (point.frequencyHz) + "   " + juce::String (point.gainDb, 1, false) + " dB";
            const auto textArea = juce::Rectangle<float> (centre.x - 60.0f, centre.y - 30.0f, 120.0f, 18.0f)
                                      .constrainedWithin (getLocalBounds().toFloat());

            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (textArea, 4.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (text, textArea, juce::Justification::centred);
        }
    }
}

//==============================================================================
// マウス操作

void EqCurveComponent::notifyEdited()
{
    if (onCurveEdited != nullptr)
        onCurveEdited (curve);
}

void EqCurveComponent::mouseDown (const juce::MouseEvent& event)
{
    if (onEditGestureStarted != nullptr)
        onEditGestureStarted();

    const int hitIndex = findPointAt (event.position);

    // 右クリックは削除
    if (event.mods.isPopupMenu())
    {
        if (hitIndex >= 0)
        {
            curve.removePoint (hitIndex);
            hoverIndex = -1;
            notifyEdited();
            repaint();
        }
        return;
    }

    if (hitIndex >= 0)
    {
        dragIndex = hitIndex;
    }
    else if (getPlotArea().contains (event.position))
    {
        dragIndex = curve.addPoint (positionToPoint (event.position)); // 追加できなければ -1
        notifyEdited();
    }

    repaint();
}

void EqCurveComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (dragIndex < 0)
        return;

    curve.movePoint (dragIndex, positionToPoint (event.position));
    notifyEdited();
    repaint();
}

void EqCurveComponent::mouseUp (const juce::MouseEvent&)
{
    dragIndex = -1;
    repaint();
}

void EqCurveComponent::mouseMove (const juce::MouseEvent& event)
{
    const int newHover = findPointAt (event.position);

    if (newHover != hoverIndex)
    {
        hoverIndex = newHover;
        repaint();
    }
}

void EqCurveComponent::mouseExit (const juce::MouseEvent&)
{
    hoverIndex = -1;
    repaint();
}

void EqCurveComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    const int hitIndex = findPointAt (event.position);

    if (hitIndex >= 0)
    {
        if (onEditGestureStarted != nullptr)
            onEditGestureStarted();

        curve.removePoint (hitIndex);
        dragIndex = -1;
        hoverIndex = -1;
        notifyEdited();
        repaint();
    }
}
