#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * プリセットの一覧・読み込み・保存。
 *
 * - 工場出荷プリセット: コードの中に持つ(PresetManager.cpp)。広がり設定と EQ カーブを含む。
 * - ユーザープリセット: 保存データ(XML)をファイルとして保存する。
 *     ~/Library/Application Support/MonoToStereo/Presets フォルダの .xml ファイル
 *
 * マイク別プリセットの EQ は、公開されている一般的な特性をもとにした初期値(目安)。
 */
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    /** 工場出荷プリセットの数。名前一覧の先頭からこの数だけが工場出荷。 */
    int getNumFactoryPresets() const;

    /** 工場出荷 → ユーザーの順の名前一覧。 */
    juce::StringArray getPresetNames() const;

    void loadPreset (int index);

    /** 現在の状態を名前を付けて保存する。成功したら true。 */
    bool saveUserPreset (const juce::String& name);

    /** EQ カーブだけを空にする(他の設定は変えない)。 */
    void clearEq();

private:
    static juce::File getUserPresetDirectory();
    juce::Array<juce::File> getUserPresetFiles() const;
    void clearUndoHistory();
    void resetAllParametersToDefault();
    void setParameterValue (const juce::String& parameterId, float plainValue);

    juce::AudioProcessorValueTreeState& state;
};
