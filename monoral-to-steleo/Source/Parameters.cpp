#include "Parameters.h"

namespace Parameters
{
    namespace
    {
        // パラメータ定義を作る小さな補助関数。version は ID 変更時の互換管理用(今は 1 固定)。
        std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id,
                                                             const juce::String& name,
                                                             float min, float max,
                                                             float defaultValue,
                                                             const juce::String& unit)
        {
            return std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, 1 },
                name,
                juce::NormalisableRange<float> (min, max),
                defaultValue,
                juce::AudioParameterFloatAttributes()
                    .withLabel (unit)
                    .withStringFromValueFunction ([] (float value, int) { return juce::String (value, 1); }));
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (makeFloat (Id::width,         "Width",         widthMinPercent, widthMaxPercent, widthDefaultPercent, "%"));
        layout.add (makeFloat (Id::spreadTimeMs,  "Spread Time",   spreadMinMs,     spreadMaxMs,     spreadDefaultMs,     "ms"));
        layout.add (makeFloat (Id::decorrelation, "Decorrelation", 0.0f,            100.0f,          decorrDefaultPercent, "%"));
        layout.add (makeFloat (Id::motion,        "Motion",        0.0f,            100.0f,          motionDefaultPercent, "%"));
        layout.add (makeFloat (Id::bassKeepHz,    "Bass Keep",     bassKeepMinHz,   bassKeepMaxHz,   bassKeepDefaultHz,   "Hz"));
        layout.add (makeFloat (Id::centerLevelDb, "Center Level",  centerMinDb,     centerMaxDb,     centerDefaultDb,     "dB"));
        layout.add (makeFloat (Id::mix,           "Mix",           0.0f,            100.0f,          mixDefaultPercent,   "%"));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { Id::monoCheck, 1 }, "Mono Check", false));

        // EQ Amount: カーブのゲインを何倍にするか(点の位置は変えない)
        layout.add (makeFloat (Id::eqAmount, "EQ Amount", 0.0f, 200.0f, 100.0f, "%"));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { Id::eqBypass, 1 }, "EQ Bypass", false));

        // Auto Gain: EQ を切り替えても音量が変わらないよう、声の帯域の平均ゲインを打ち消す
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { Id::eqAutoGain, 1 }, "Auto Gain", false));

        // EQ の位相モード。0 = Minimum(遅延なし)、1 = Linear(位相が変わらないが遅延あり)
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { Id::phaseMode, 1 }, "EQ Phase",
            juce::StringArray { "Minimum", "Linear" }, 0));

        return layout;
    }
}
