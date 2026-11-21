#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * プラグインのパラメータ名・範囲・初期値をここにまとめる。
 * 値を変えたいときはこのファイルだけを見ればよい。
 */
namespace Parameters
{
    // DAW やプリセットがパラメータを識別する ID。一度公開したら変えないこと。
    namespace Id
    {
        constexpr const char* width         = "width";
        constexpr const char* spreadTimeMs  = "spreadTimeMs";
        constexpr const char* decorrelation = "decorrelation";
        constexpr const char* motion        = "motion";
        constexpr const char* bassKeepHz    = "bassKeepHz";
        constexpr const char* centerLevelDb = "centerLevelDb";
        constexpr const char* mix           = "mix";
        constexpr const char* monoCheck     = "monoCheck";
        constexpr const char* phaseMode     = "phaseMode";
        constexpr const char* eqAmount      = "eqAmount";
        constexpr const char* eqBypass      = "eqBypass";
        constexpr const char* eqAutoGain    = "eqAutoGain";
    }

    // 範囲と初期値
    constexpr float widthMinPercent = 0.0f,     widthMaxPercent = 200.0f,  widthDefaultPercent = 100.0f;
    constexpr float spreadMinMs     = 1.0f,     spreadMaxMs     = 40.0f,   spreadDefaultMs     = 15.0f;
    constexpr float decorrDefaultPercent  = 50.0f;
    constexpr float motionDefaultPercent  = 20.0f;
    constexpr float bassKeepMinHz   = 50.0f,    bassKeepMaxHz   = 300.0f,  bassKeepDefaultHz   = 150.0f;
    constexpr float centerMinDb     = -12.0f,   centerMaxDb     = 6.0f,    centerDefaultDb     = 0.0f;
    constexpr float mixDefaultPercent     = 100.0f;

    /** 全パラメータの定義を作る。AudioProcessorValueTreeState に渡す。 */
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
