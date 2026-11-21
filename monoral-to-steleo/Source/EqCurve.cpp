#include "EqCurve.h"

#include <algorithm>
#include <cmath>

namespace
{
    const juce::Identifier curveType  { "EqCurve" };
    const juce::Identifier pointType  { "Point" };
    const juce::Identifier freqProp   { "freq" };
    const juce::Identifier gainProp   { "gain" };

    float toOctaves (float frequencyHz)
    {
        return std::log2 (std::max (frequencyHz, 1.0f));
    }
}

EqCurve::EqCurve (std::vector<EqPoint> newPoints)
{
    std::sort (newPoints.begin(), newPoints.end(),
               [] (const EqPoint& a, const EqPoint& b) { return a.frequencyHz < b.frequencyHz; });

    for (const auto& point : newPoints)
        addPoint (point);
}

int EqCurve::addPoint (EqPoint point)
{
    if (getNumPoints() >= maxPoints)
        return -1;

    point.frequencyHz = juce::jlimit (minFrequencyHz, maxFrequencyHz, point.frequencyHz);
    point.gainDb = juce::jlimit (-maxGainDb, maxGainDb, point.gainDb);

    // 既存の点に近すぎる位置には置けない。
    for (const auto& existing : points)
        if (std::abs (toOctaves (existing.frequencyHz) - toOctaves (point.frequencyHz)) < minSpacingOctaves)
            return -1;

    const auto insertPosition = std::upper_bound (points.begin(), points.end(), point,
                                                  [] (const EqPoint& a, const EqPoint& b)
                                                  { return a.frequencyHz < b.frequencyHz; });
    const int index = static_cast<int> (insertPosition - points.begin());
    points.insert (insertPosition, point);
    updateSlopes();
    return index;
}

void EqCurve::movePoint (int index, EqPoint target)
{
    if (! juce::isPositiveAndBelow (index, getNumPoints()))
        return;

    // 隣の点との間隔を保てる範囲に収める。点の順番は入れ替わらない。
    const float spacingRatio = std::exp2 (minSpacingOctaves);
    float lowest  = minFrequencyHz;
    float highest = maxFrequencyHz;

    if (index > 0)
        lowest = points[static_cast<size_t> (index - 1)].frequencyHz * spacingRatio;

    if (index < getNumPoints() - 1)
        highest = points[static_cast<size_t> (index + 1)].frequencyHz / spacingRatio;

    auto& point = points[static_cast<size_t> (index)];
    point.frequencyHz = juce::jlimit (lowest, std::max (lowest, highest), target.frequencyHz);
    point.gainDb = juce::jlimit (-maxGainDb, maxGainDb, target.gainDb);
    updateSlopes();
}

void EqCurve::removePoint (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPoints()))
        return;

    points.erase (points.begin() + index);
    updateSlopes();
}

void EqCurve::clear()
{
    points.clear();
    slopes.clear();
}

// 単調 3 次補間(Fritsch-Carlson 法)。点と点の間で、点のゲインを超えて膨らまない。
void EqCurve::updateSlopes()
{
    const size_t count = points.size();
    slopes.assign (count, 0.0f);

    if (count < 2)
        return;

    std::vector<float> spans (count - 1);
    std::vector<float> secants (count - 1);

    for (size_t k = 0; k + 1 < count; ++k)
    {
        spans[k] = toOctaves (points[k + 1].frequencyHz) - toOctaves (points[k].frequencyHz);
        secants[k] = (points[k + 1].gainDb - points[k].gainDb) / spans[k];
    }

    slopes.front() = secants.front();
    slopes.back() = secants.back();

    for (size_t k = 1; k + 1 < count; ++k)
    {
        // 上り下りが変わる点では傾きを 0 にして、山や谷を作る。
        if (secants[k - 1] * secants[k] <= 0.0f)
        {
            slopes[k] = 0.0f;
        }
        else
        {
            const float weightA = 2.0f * spans[k] + spans[k - 1];
            const float weightB = spans[k] + 2.0f * spans[k - 1];
            slopes[k] = (weightA + weightB) / (weightA / secants[k - 1] + weightB / secants[k]);
        }
    }
}

float EqCurve::gainDbAt (float frequencyHz) const
{
    if (points.empty())
        return 0.0f;

    if (frequencyHz <= points.front().frequencyHz)
        return points.front().gainDb;

    if (frequencyHz >= points.back().frequencyHz)
        return points.back().gainDb;

    // frequencyHz を挟む 2 点(segment 番目と次の点)を探す。
    const auto upper = std::upper_bound (points.begin(), points.end(), frequencyHz,
                                         [] (float value, const EqPoint& p) { return value < p.frequencyHz; });
    const size_t segment = static_cast<size_t> (upper - points.begin()) - 1;

    const float x0 = toOctaves (points[segment].frequencyHz);
    const float x1 = toOctaves (points[segment + 1].frequencyHz);
    const float span = x1 - x0;
    const float t = (toOctaves (frequencyHz) - x0) / span;

    const float t2 = t * t;
    const float t3 = t2 * t;

    // 3 次エルミート補間
    return (2.0f * t3 - 3.0f * t2 + 1.0f) * points[segment].gainDb
         + (t3 - 2.0f * t2 + t) * span * slopes[segment]
         + (-2.0f * t3 + 3.0f * t2) * points[segment + 1].gainDb
         + (t3 - t2) * span * slopes[segment + 1];
}

float EqCurve::meanGainDb (float lowHz, float highHz) const
{
    constexpr int numSamples = 64;
    const float lowOctaves = toOctaves (lowHz);
    const float highOctaves = toOctaves (highHz);

    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float position = (static_cast<float> (i) + 0.5f) / static_cast<float> (numSamples);
        sum += gainDbAt (std::exp2 (lowOctaves + position * (highOctaves - lowOctaves)));
    }

    return sum / static_cast<float> (numSamples);
}

//==============================================================================
juce::Identifier EqCurve::getTreeType()
{
    return curveType;
}

EqCurve EqCurve::readFromState (const juce::ValueTree& pluginState)
{
    std::vector<EqPoint> loaded;
    const auto node = pluginState.getChildWithName (curveType);

    for (const auto& child : node)
        loaded.push_back ({ static_cast<float> (child.getProperty (freqProp, 1000.0)),
                            static_cast<float> (child.getProperty (gainProp, 0.0)) });

    return EqCurve (std::move (loaded));
}

void EqCurve::writeToState (juce::ValueTree& pluginState, juce::UndoManager* undoManager) const
{
    auto node = pluginState.getChildWithName (curveType);

    if (! node.isValid())
    {
        node = juce::ValueTree (curveType);
        pluginState.appendChild (node, undoManager);
    }

    // 点の数を合わせてから、値だけを書き換える。
    // 全部消して作り直すと、途中の空の状態が画面に伝わって操作中の点が外れてしまう。
    while (node.getNumChildren() > getNumPoints())
        node.removeChild (node.getNumChildren() - 1, undoManager);

    while (node.getNumChildren() < getNumPoints())
        node.appendChild (juce::ValueTree (pointType), undoManager);

    for (int i = 0; i < getNumPoints(); ++i)
    {
        auto child = node.getChild (i);
        child.setProperty (freqProp, points[static_cast<size_t> (i)].frequencyHz, undoManager);
        child.setProperty (gainProp, points[static_cast<size_t> (i)].gainDb, undoManager);
    }
}
