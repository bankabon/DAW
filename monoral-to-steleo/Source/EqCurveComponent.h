#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include "EqCurve.h"
#include "SpectrumAnalyzer.h"

/**
 * EQ カーブのグラフ。点を直接操作して調整する。
 *
 *   空いている場所をクリック : 点を追加(最大 64 点)
 *   点をドラッグ             : 周波数とゲインを変更
 *   点をダブルクリック / 右クリック : 点を削除
 *
 * カーブを編集したら onCurveEdited を呼ぶ。保存データへの反映は呼び出し側が行う。
 */
class EqCurveComponent : public juce::Component
{
public:
    /** inputToShow は EQ に入る前の音、outputToShow は EQ を通った後の音。 */
    EqCurveComponent (const SpectrumAnalyzer& inputToShow, const SpectrumAnalyzer& outputToShow);

    std::function<void (const EqCurve&)> onCurveEdited;

    /** 点の追加・移動・削除の操作を始める直前に呼ぶ(元に戻す履歴の区切りに使う)。 */
    std::function<void()> onEditGestureStarted;

    void setCurve (const EqCurve& newCurve);
    void setSampleRate (double newSampleRate) { sampleRate = newSampleRate; }

    /** 実際に効いているカーブの表示用。amount は 1.0 で等倍。バイパス中は薄く表示する。 */
    void setDisplayState (float newAmount, bool newBypassed);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> getPlotArea() const;
    float frequencyToX (float frequencyHz) const;
    float xToFrequency (float x) const;
    float gainToY (float gainDb) const;
    float yToGain (float y) const;
    juce::Point<float> pointToPosition (const EqPoint& point) const;
    EqPoint positionToPoint (juce::Point<float> position) const;
    int findPointAt (juce::Point<float> position) const;

    void drawGrid (juce::Graphics&, juce::Rectangle<float> area) const;
    void drawSpectrum (juce::Graphics&, juce::Rectangle<float> area) const;
    juce::Path buildSpectrumPath (const SpectrumAnalyzer&, juce::Rectangle<float> area) const;
    void drawCurve (juce::Graphics&, juce::Rectangle<float> area) const;
    juce::Path buildCurvePath (juce::Rectangle<float> area, float gainScale) const;
    void drawPoints (juce::Graphics&) const;
    void notifyEdited();

    static juce::String formatFrequency (float frequencyHz);

    const SpectrumAnalyzer& inputAnalyzer;
    const SpectrumAnalyzer& outputAnalyzer;
    double sampleRate = 44100.0;
    EqCurve curve;
    float amount = 1.0f;
    bool bypassed = false;
    int hoverIndex = -1;
    int dragIndex = -1;
};
