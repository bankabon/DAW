#include "PresetManager.h"
#include "EqCurve.h"
#include "Parameters.h"

namespace
{
    struct ParameterValue
    {
        const char* parameterId;
        float value;
    };

    struct FactoryPreset
    {
        const char* name;
        std::vector<ParameterValue> parameterValues; // 書かなかったパラメータは初期値になる
        std::vector<EqPoint> eqPoints;
    };

    namespace Id = Parameters::Id;

    // マイク別の広がり設定。声を自然に広げる共通の値。
    const std::vector<ParameterValue> voiceSpread {
        { Id::width, 90.0f }, { Id::spreadTimeMs, 12.0f }, { Id::decorrelation, 45.0f }, { Id::motion, 15.0f } };

    const std::vector<FactoryPreset>& factoryPresets()
    {
        // 値はあくまで出発点。録った声を聴いて調整する。
        static const std::vector<FactoryPreset> presets {
            { "Init (Flat)", {}, {} },

            { "Subtle Wide",
              { { Id::width, 70.0f }, { Id::spreadTimeMs, 8.0f }, { Id::decorrelation, 30.0f }, { Id::motion, 10.0f } },
              {} },

            { "Wide",
              { { Id::width, 130.0f }, { Id::spreadTimeMs, 18.0f }, { Id::decorrelation, 60.0f }, { Id::motion, 20.0f } },
              {} },

            { "Dreamy",
              { { Id::width, 160.0f }, { Id::spreadTimeMs, 28.0f }, { Id::decorrelation, 80.0f },
                { Id::motion, 50.0f }, { Id::mix, 85.0f } },
              {} },

            // 近接効果で膨らむ低域と、中高域のきつさを整え、高域を少し補う。
            { "Mic: Shure SM58", voiceSpread,
              { { 70.0f, -8.0f }, { 150.0f, -3.0f }, { 300.0f, -2.0f }, { 1000.0f, 0.0f },
                { 4000.0f, 1.0f }, { 6000.0f, -1.5f }, { 12000.0f, 2.5f } } },

            // 出力が小さく高域がおとなしいので、中高域の明るさを補う。
            { "Mic: Shure SM7B", voiceSpread,
              { { 60.0f, -6.0f }, { 200.0f, -1.0f }, { 3500.0f, 2.5f }, { 8000.0f, 2.0f }, { 14000.0f, 3.0f } } },

            // もともとバランスが良いので補正は小さく。低域の整理と空気感を足す。
            { "Mic: Neumann U87", voiceSpread,
              { { 70.0f, -4.0f }, { 200.0f, -1.0f }, { 1000.0f, 0.0f }, { 5000.0f, 0.5f }, { 12000.0f, 1.5f } } },

            // 高域のきつさを抑え、低域の薄さを補う。
            { "Mic: AT2020", voiceSpread,
              { { 60.0f, -3.0f }, { 120.0f, 1.5f }, { 250.0f, 1.0f }, { 5000.0f, -1.0f },
                { 8000.0f, -2.5f }, { 14000.0f, -1.0f } } },

            // 高域の硬さを和らげ、中域に厚みを出す。
            { "Mic: Rode NT1", voiceSpread,
              { { 80.0f, -3.0f }, { 300.0f, 1.0f }, { 1500.0f, 0.5f }, { 6000.0f, -2.0f }, { 10000.0f, -1.5f } } },

            // --- マイク(追加分)---

            // 低域が薄く中高域に山がある。高域の伸びを補う。
            { "Mic: Shure SM57", voiceSpread,
              { { 80.0f, -5.0f }, { 250.0f, -1.5f }, { 5000.0f, -1.0f }, { 10000.0f, 2.0f }, { 14000.0f, 3.0f } } },

            // 配信向けダイナミックマイク。こもりを抑え、明るさを足す。
            { "Mic: Shure MV7", voiceSpread,
              { { 70.0f, -5.0f }, { 200.0f, -1.5f }, { 3000.0f, 1.5f }, { 10000.0f, 2.0f } } },

            // 高域がおとなしいので、中高域を持ち上げる。
            { "Mic: Rode PodMic", voiceSpread,
              { { 60.0f, -5.0f }, { 250.0f, -1.0f }, { 4000.0f, 2.5f }, { 9000.0f, 2.5f }, { 14000.0f, 3.0f } } },

            // 箱鳴りしやすい中低域を抑える。
            { "Mic: Blue Yeti", voiceSpread,
              { { 80.0f, -4.0f }, { 300.0f, -2.5f }, { 2500.0f, 1.0f }, { 6000.0f, -1.0f }, { 12000.0f, 1.5f } } },

            // 明るめのコンデンサーマイク。高域のきつさを和らげる。
            { "Mic: AT2035", voiceSpread,
              { { 70.0f, -4.0f }, { 150.0f, 0.5f }, { 6000.0f, -1.5f }, { 10000.0f, -1.0f } } },

            // 5kHz 付近の山を少し抑える。
            { "Mic: AKG C214", voiceSpread,
              { { 70.0f, -4.0f }, { 300.0f, -1.0f }, { 5000.0f, -1.5f }, { 12000.0f, -1.0f } } },

            // --- 声の用途別 ---

            { "Voice: Podcast Clear",
              { { Id::width, 60.0f }, { Id::spreadTimeMs, 8.0f }, { Id::decorrelation, 30.0f },
                { Id::motion, 5.0f }, { Id::mix, 80.0f } },
              { { 70.0f, -8.0f }, { 200.0f, -1.5f }, { 3000.0f, 2.0f }, { 10000.0f, 1.5f } } },

            { "Voice: Warm",
              { { Id::width, 80.0f }, { Id::spreadTimeMs, 12.0f }, { Id::decorrelation, 40.0f }, { Id::motion, 10.0f } },
              { { 100.0f, 2.0f }, { 250.0f, 1.0f }, { 4000.0f, -1.0f }, { 9000.0f, -2.0f } } },

            { "Voice: Bright Air",
              { { Id::width, 110.0f }, { Id::spreadTimeMs, 14.0f }, { Id::decorrelation, 55.0f }, { Id::motion, 15.0f } },
              { { 100.0f, -4.0f }, { 3000.0f, 1.5f }, { 8000.0f, 3.0f }, { 14000.0f, 4.0f } } },

            // 250Hz 付近のこもりだけを抑える。
            { "Voice: De-Mud", voiceSpread,
              { { 150.0f, 0.0f }, { 250.0f, -3.5f }, { 450.0f, 0.0f } } },

            // --- 広がり方の違い ---

            { "Wide: Mono-Safe",
              { { Id::width, 80.0f }, { Id::spreadTimeMs, 5.0f }, { Id::decorrelation, 40.0f },
                { Id::motion, 10.0f }, { Id::mix, 70.0f } },
              {} },

            { "Wide: Instrument",
              { { Id::width, 150.0f }, { Id::spreadTimeMs, 20.0f }, { Id::decorrelation, 70.0f }, { Id::motion, 25.0f } },
              {} },

            { "Wide: Moving",
              { { Id::width, 140.0f }, { Id::spreadTimeMs, 22.0f }, { Id::decorrelation, 50.0f }, { Id::motion, 70.0f } },
              {} },

            // --- 効果 ---

            // 低域と高域を大きく削って、電話やラジオのような音にする。広がりは無し。
            { "FX: Radio / Telephone",
              { { Id::width, 0.0f }, { Id::mix, 100.0f } },
              { { 150.0f, -12.0f }, { 300.0f, 0.0f }, { 1000.0f, 4.0f }, { 3000.0f, 4.0f }, { 3500.0f, 0.0f }, { 5000.0f, -12.0f } } },
        };

        return presets;
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& stateToUse)
    : state (stateToUse)
{
}

int PresetManager::getNumFactoryPresets() const
{
    return static_cast<int> (factoryPresets().size());
}

juce::File PresetManager::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("MonoToStereo")
        .getChildFile ("Presets");
}

juce::Array<juce::File> PresetManager::getUserPresetFiles() const
{
    auto files = getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    return files;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;

    for (const auto& preset : factoryPresets())
        names.add (preset.name);

    for (const auto& file : getUserPresetFiles())
        names.add (file.getFileNameWithoutExtension());

    return names;
}

// プリセットを読み込むと設定が丸ごと変わるので、読み込み前の履歴は捨てる。
void PresetManager::clearUndoHistory()
{
    if (state.undoManager != nullptr)
        state.undoManager->clearUndoHistory();
}

void PresetManager::resetAllParametersToDefault()
{
    for (auto* parameter : state.processor.getParameters())
        parameter->setValueNotifyingHost (parameter->getDefaultValue());
}

void PresetManager::setParameterValue (const juce::String& parameterId, float plainValue)
{
    if (auto* parameter = state.getParameter (parameterId))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
}

void PresetManager::clearEq()
{
    // Clear EQ は元に戻せる操作にする。
    if (state.undoManager != nullptr)
        state.undoManager->beginNewTransaction ("Clear EQ");

    EqCurve().writeToState (state.state, state.undoManager);
}

void PresetManager::loadPreset (int index)
{
    const int numFactory = getNumFactoryPresets();

    if (index >= 0 && index < numFactory)
    {
        const auto& preset = factoryPresets()[static_cast<size_t> (index)];

        resetAllParametersToDefault();

        for (const auto& entry : preset.parameterValues)
            setParameterValue (entry.parameterId, entry.value);

        EqCurve (preset.eqPoints).writeToState (state.state);
        clearUndoHistory();
        return;
    }

    const auto files = getUserPresetFiles();
    const int userIndex = index - numFactory;

    if (userIndex >= 0 && userIndex < files.size())
        if (auto xml = juce::XmlDocument::parse (files[userIndex]))
            if (xml->hasTagName (state.state.getType()))
            {
                state.replaceState (juce::ValueTree::fromXml (*xml));
                clearUndoHistory();
            }
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    const auto fileName = juce::File::createLegalFileName (name.trim());

    if (fileName.isEmpty())
        return false;

    const auto directory = getUserPresetDirectory();

    if (! directory.createDirectory())
        return false;

    const auto xml = state.copyState().createXml();
    return xml != nullptr && xml->writeTo (directory.getChildFile (fileName + ".xml"));
}
