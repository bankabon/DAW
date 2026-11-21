#pragma once

#include <vector>
#include <juce_data_structures/juce_data_structures.h>

/** EQ カーブ上の 1 点。 */
struct EqPoint
{
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
};

/**
 * 最大 64 点のスプライン EQ カーブ。
 *
 * - 点を滑らかにつないだ曲線(単調 3 次補間)が、そのまま周波数ごとのゲインになる。
 * - 最初の点より低い帯域と最後の点より高い帯域は、端の点のゲインで一定にする。
 * - 点は周波数の低い順に並び、隣と最低 1/48 オクターブは離す(同じ周波数に 2 点置けない)。
 * - カーブは ValueTree(プラグインの保存データ)に読み書きできる。
 */
class EqCurve
{
public:
    static constexpr int maxPoints = 64;
    static constexpr float minFrequencyHz = 20.0f;
    static constexpr float maxFrequencyHz = 20000.0f;
    static constexpr float maxGainDb = 12.0f;
    static constexpr float minSpacingOctaves = 1.0f / 48.0f;

    EqCurve() = default;

    /** 点の一覧から作る。範囲外や近すぎる点は取り除く。 */
    explicit EqCurve (std::vector<EqPoint> newPoints);

    /** 点を追加する。追加できたらその位置の番号、できなければ -1 を返す。 */
    int addPoint (EqPoint point);

    /** 点を動かす。隣の点を越えないよう範囲内に収める。 */
    void movePoint (int index, EqPoint target);

    void removePoint (int index);
    void clear();

    const std::vector<EqPoint>& getPoints() const { return points; }
    int getNumPoints() const { return static_cast<int> (points.size()); }

    /** 指定した周波数でのゲイン(dB)。 */
    float gainDbAt (float frequencyHz) const;

    /** 指定した帯域(対数軸)でのゲインの平均(dB)。Auto Gain の補正量を決めるのに使う。 */
    float meanGainDb (float lowHz, float highHz) const;

    //==============================================================================
    // 保存データとの読み書き
    static juce::Identifier getTreeType();
    static EqCurve readFromState (const juce::ValueTree& pluginState);
    /** カーブを保存データに書く。undoManager を渡すと、元に戻せる変更として記録する。 */
    void writeToState (juce::ValueTree& pluginState, juce::UndoManager* undoManager = nullptr) const;

private:
    void updateSlopes();

    std::vector<EqPoint> points;
    std::vector<float> slopes; // 各点での曲線の傾き(補間に使う)
};
