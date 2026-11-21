#include "PluginEditor.h"
#include "Parameters.h"
#include "Theme.h"

namespace
{
    constexpr int editorWidth = 960;
    constexpr int editorHeight = 650;
    constexpr int topBarHeight = 56;
    constexpr int eqRowHeight = 32;
    constexpr int knobAreaHeight = 170;
    constexpr int spectrumRefreshHz = 30;

    // 画面に並べるつまみの順番
    const char* const knobParameterIds[] = {
        Parameters::Id::width, Parameters::Id::spreadTimeMs, Parameters::Id::decorrelation,
        Parameters::Id::motion, Parameters::Id::bassKeepHz, Parameters::Id::centerLevelDb,
        Parameters::Id::mix };
}

PluginEditor::PluginEditor (PluginProcessor& owningProcessor)
    : AudioProcessorEditor (&owningProcessor),
      pluginProcessor (owningProcessor),
      presetManager (owningProcessor.parameters),
      eqCurveComponent (owningProcessor.inputSpectrumAnalyzer, owningProcessor.outputSpectrumAnalyzer)
{
    setLookAndFeel (&lookAndFeel);

    // --- 上段 ---
    addAndMakeVisible (presetBox);
    presetBox.onChange = [this]
    {
        const int selected = presetBox.getSelectedId();
        if (selected > 0)
            presetManager.loadPreset (selected - 1);
    };

    addAndMakeVisible (saveButton);
    saveButton.onClick = [this] { askForPresetNameAndSave(); };

    // --- EQ 操作列 ---
    addAndMakeVisible (undoButton);
    undoButton.onClick = [this] { pluginProcessor.undoManager.undo(); };

    addAndMakeVisible (redoButton);
    redoButton.onClick = [this] { pluginProcessor.undoManager.redo(); };

    addAndMakeVisible (clearEqButton);
    clearEqButton.onClick = [this] { presetManager.clearEq(); };

    amountLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    amountLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (amountLabel);

    amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    amountSlider.setTextValueSuffix (" %");
    amountSlider.setColour (juce::Slider::textBoxTextColourId, Theme::textPrimary);
    amountSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (amountSlider);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        pluginProcessor.parameters, Parameters::Id::eqAmount, amountSlider);

    addAndMakeVisible (autoGainButton);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        pluginProcessor.parameters, Parameters::Id::eqAutoGain, autoGainButton);

    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        pluginProcessor.parameters, Parameters::Id::eqBypass, bypassButton);

    phaseLabel.setJustificationType (juce::Justification::centredRight);
    phaseLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    addAndMakeVisible (phaseLabel);

    phaseBox.addItemList ({ "Minimum", "Linear" }, 1);
    addAndMakeVisible (phaseBox);
    phaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        pluginProcessor.parameters, Parameters::Id::phaseMode, phaseBox);

    addAndMakeVisible (monoCheckButton);
    monoCheckAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        pluginProcessor.parameters, Parameters::Id::monoCheck, monoCheckButton);

    // --- 中段: EQ グラフ ---
    addAndMakeVisible (eqCurveComponent);
    eqCurveComponent.onEditGestureStarted = [this]
    {
        // 点の操作 1 回(クリックからドラッグ終了まで)を、元に戻す履歴の 1 件にする。
        pluginProcessor.undoManager.beginNewTransaction ("Edit EQ");
    };
    eqCurveComponent.onCurveEdited = [this] (const EqCurve& editedCurve)
    {
        // 自分が書き込んだ変更は、グラフに戻さない(操作中の点の状態が崩れるため)。
        const juce::ScopedValueSetter<bool> ignoreOwnWrite (isWritingCurve, true);
        editedCurve.writeToState (pluginProcessor.parameters.state, &pluginProcessor.undoManager);
    };

    // --- 下段: つまみ ---
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        knobs[i] = std::make_unique<ParameterKnob> (pluginProcessor.parameters, knobParameterIds[i]);
        addAndMakeVisible (*knobs[i]);
    }

    refreshPresetList();
    refreshCurveFromState();
    pluginProcessor.parameters.state.addListener (this);

    setSize (editorWidth, editorHeight);
    startTimerHz (spectrumRefreshHz);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    pluginProcessor.parameters.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (Theme::backgroundTop, 0.0f, 0.0f,
                                             Theme::background, 0.0f, static_cast<float> (getHeight()), false));
    g.fillAll();

    // タイトル
    g.setColour (Theme::textPrimary);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("MONO", headerArea.getX(), headerArea.getY(), 66, headerArea.getHeight(), juce::Justification::centredLeft);
    g.setColour (Theme::accent);
    g.drawText ("TO", headerArea.getX() + 66, headerArea.getY(), 34, headerArea.getHeight(), juce::Justification::centredLeft);
    g.setColour (Theme::textPrimary);
    g.drawText ("STEREO", headerArea.getX() + 100, headerArea.getY(), 90, headerArea.getHeight(), juce::Justification::centredLeft);

    // つまみ部分のパネル
    const auto panel = knobPanelArea.toFloat().reduced (0.5f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (panel, 12.0f);
    g.setColour (Theme::panelBorder);
    g.drawRoundedRectangle (panel, 12.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // 上段: 左にタイトル、そのあとにプリセット関連、右に Mono Check
    auto topBar = area.removeFromTop (topBarHeight - 16);
    headerArea = topBar.removeFromLeft (200);

    presetBox.setBounds (topBar.removeFromLeft (240).withSizeKeepingCentre (240, 30));
    topBar.removeFromLeft (8);
    saveButton.setBounds (topBar.removeFromLeft (64).withSizeKeepingCentre (64, 30));
    monoCheckButton.setBounds (topBar.removeFromRight (120).withSizeKeepingCentre (120, 30));

    area.removeFromTop (8);

    // EQ 操作列: 左に Undo / Redo / Clear EQ、中央に EQ Amount、右に Auto Gain / EQ Bypass / EQ Phase
    auto eqRow = area.removeFromTop (eqRowHeight);
    undoButton.setBounds (eqRow.removeFromLeft (60).withSizeKeepingCentre (60, 28));
    eqRow.removeFromLeft (6);
    redoButton.setBounds (eqRow.removeFromLeft (60).withSizeKeepingCentre (60, 28));
    eqRow.removeFromLeft (6);
    clearEqButton.setBounds (eqRow.removeFromLeft (84).withSizeKeepingCentre (84, 28));
    eqRow.removeFromLeft (20);

    phaseBox.setBounds (eqRow.removeFromRight (104).withSizeKeepingCentre (104, 28));
    phaseLabel.setBounds (eqRow.removeFromRight (72));
    eqRow.removeFromRight (12);
    bypassButton.setBounds (eqRow.removeFromRight (112));
    autoGainButton.setBounds (eqRow.removeFromRight (104));
    eqRow.removeFromRight (12);

    amountLabel.setBounds (eqRow.removeFromLeft (84));
    amountSlider.setBounds (eqRow);

    area.removeFromTop (10);

    // 下段
    knobPanelArea = area.removeFromBottom (knobAreaHeight);
    auto knobArea = knobPanelArea.reduced (8, 10);
    const int knobWidth = knobArea.getWidth() / numKnobs;
    for (auto& knob : knobs)
        knob->setBounds (knobArea.removeFromLeft (knobWidth));

    area.removeFromBottom (12);

    // 中段
    eqCurveComponent.setBounds (area);
}

//==============================================================================
void PluginEditor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (tree.getParent().hasType (EqCurve::getTreeType()))
        refreshCurveFromState();
}

void PluginEditor::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent.hasType (EqCurve::getTreeType()))
        refreshCurveFromState();
}

void PluginEditor::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent.hasType (EqCurve::getTreeType()))
        refreshCurveFromState();
}

void PluginEditor::refreshCurveFromState()
{
    if (isWritingCurve)
        return;

    eqCurveComponent.setCurve (EqCurve::readFromState (pluginProcessor.parameters.state));
}

void PluginEditor::timerCallback()
{
    pluginProcessor.inputSpectrumAnalyzer.update();
    pluginProcessor.outputSpectrumAnalyzer.update();
    eqCurveComponent.setSampleRate (pluginProcessor.getSampleRate() > 0.0 ? pluginProcessor.getSampleRate() : 44100.0);

    const auto& parameters = pluginProcessor.parameters;
    eqCurveComponent.setDisplayState (parameters.getRawParameterValue (Parameters::Id::eqAmount)->load() * 0.01f,
                                      parameters.getRawParameterValue (Parameters::Id::eqBypass)->load() > 0.5f);
    eqCurveComponent.repaint();

    // 戻せる操作がないときは Undo / Redo を薄くして押せなくする。
    undoButton.setEnabled (pluginProcessor.undoManager.canUndo());
    redoButton.setEnabled (pluginProcessor.undoManager.canRedo());
    undoButton.setAlpha (undoButton.isEnabled() ? 1.0f : 0.4f);
    redoButton.setAlpha (redoButton.isEnabled() ? 1.0f : 0.4f);
}

//==============================================================================
void PluginEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);

    const auto names = presetManager.getPresetNames();
    const int numFactory = presetManager.getNumFactoryPresets();

    // 項目 ID = プリセット番号 + 1(ID 0 は「未選択」を表すため使えない)
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == numFactory)
            presetBox.addSeparator();

        presetBox.addItem (names[i], i + 1);
    }

    presetBox.setTextWhenNothingSelected ("Preset...");
}

void PluginEditor::askForPresetNameAndSave()
{
    auto* window = new juce::AlertWindow ("Save Preset", "Preset name", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", "", "");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PluginEditor> safeThis (this);
    juce::Component::SafePointer<juce::AlertWindow> safeWindow (window);

    window->enterModalState (true,
        juce::ModalCallbackFunction::create ([safeThis, safeWindow] (int result)
        {
            if (result != 1 || safeThis == nullptr || safeWindow == nullptr)
                return;

            const auto name = safeWindow->getTextEditorContents ("name");

            if (safeThis->presetManager.saveUserPreset (name))
            {
                safeThis->refreshPresetList();
                const auto names = safeThis->presetManager.getPresetNames();
                safeThis->presetBox.setSelectedId (names.indexOf (juce::File::createLegalFileName (name.trim())) + 1,
                                                   juce::dontSendNotification);
            }
        }),
        true);
}
