#pragma once

#include <juce_graphics/juce_graphics.h>

/** 画面全体で使う色。見た目を変えたいときはここを直す。 */
namespace Theme
{
    const juce::Colour background    { 0xff0d1117 };
    const juce::Colour backgroundTop { 0xff161d27 }; // 背景グラデーションの上側
    const juce::Colour panel         { 0xff151b23 };
    const juce::Colour panelBorder   { 0xff252e3a };
    const juce::Colour grid          { 0xff222b36 };
    const juce::Colour gridStrong    { 0xff34414f };

    const juce::Colour textPrimary   { 0xffe8eef5 };
    const juce::Colour textMuted     { 0xff7d8b99 };

    const juce::Colour accent        { 0xff4cc9f0 }; // つまみ・スペクトラム
    const juce::Colour accentSoft    { 0xff2b6f86 };
    const juce::Colour curve         { 0xffffa94d }; // EQ カーブと点
}
