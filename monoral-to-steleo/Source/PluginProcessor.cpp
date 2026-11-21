#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "EqCurve.h"
#include "Parameters.h"

namespace
{
    // これらが変わったら EQ のフィルターを作り直す。
    const char* const eqParameterIds[] = {
        Parameters::Id::phaseMode, Parameters::Id::eqAmount,
        Parameters::Id::eqBypass, Parameters::Id::eqAutoGain };

    // Auto Gain が音量をそろえる帯域。声のほとんどが入る範囲。
    constexpr float autoGainLowHz = 100.0f;
    constexpr float autoGainHighHz = 8000.0f;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, &undoManager, "Parameters", Parameters::createParameterLayout())
{
    for (const auto* id : eqParameterIds)
        parameters.addParameterListener (id, this);

    parameters.state.addListener (this);
}

PluginProcessor::~PluginProcessor()
{
    cancelPendingUpdate();
    parameters.state.removeListener (this);

    for (const auto* id : eqParameterIds)
        parameters.removeParameterListener (id, this);
}

void PluginProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    DBG ("prepareToPlay: sampleRate=" << sampleRate << " maxBlockSize=" << maxBlockSize);

    currentSampleRate = sampleRate;
    monoScratch.assign (static_cast<size_t> (maxBlockSize), 0.0f);
    widener.prepare (sampleRate, maxBlockSize);

    // Convolution は、IR を先に読み込んでから prepare すると最初のブロックから有効になる。
    rebuildEqFilter();
    eqConvolution.prepare ({ sampleRate, static_cast<juce::uint32> (maxBlockSize), 1 });
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // 出力はステレオのみ。入力はモノラル(推奨)またはステレオを受け付ける。
    const auto input  = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    const bool outputIsStereo = (output == juce::AudioChannelSet::stereo());
    const bool inputIsSupported = (input == juce::AudioChannelSet::mono()
                                   || input == juce::AudioChannelSet::stereo());

    return outputIsStereo && inputIsSupported;
}

//==============================================================================
// EQ の作り直し

bool PluginProcessor::isEqCurveNode (const juce::ValueTree& tree)
{
    return tree.hasType (EqCurve::getTreeType());
}

void PluginProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate(); // 音声スレッドから呼ばれることがあるので、作り直しは別スレッドで行う
}

void PluginProcessor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (isEqCurveNode (tree.getParent()))
        triggerAsyncUpdate();
}

void PluginProcessor::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (isEqCurveNode (parent))
        triggerAsyncUpdate();
}

void PluginProcessor::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (isEqCurveNode (parent))
        triggerAsyncUpdate();
}

void PluginProcessor::handleAsyncUpdate()
{
    rebuildEqFilter();
}

void PluginProcessor::rebuildEqFilter()
{
    const bool isLinear = readParameter (Parameters::Id::phaseMode) > 0.5f;
    const auto mode = isLinear ? EqPhaseMode::linear : EqPhaseMode::minimum;

    // バイパス中は平らなカーブにする(処理の遅延は変えないので、切り替えでタイミングがずれない)。
    const bool isBypassed = readParameter (Parameters::Id::eqBypass) > 0.5f;
    const auto curve = isBypassed ? EqCurve() : EqCurve::readFromState (parameters.state);

    const float amount = readParameter (Parameters::Id::eqAmount) * 0.01f;
    const bool useAutoGain = readParameter (Parameters::Id::eqAutoGain) > 0.5f;
    const float makeupGainDb = useAutoGain ? -curve.meanGainDb (autoGainLowHz, autoGainHighHz) * amount : 0.0f;

    eqConvolution.loadImpulseResponse (EqImpulseDesigner::design (curve, currentSampleRate, mode, amount, makeupGainDb),
                                       currentSampleRate,
                                       juce::dsp::Convolution::Stereo::no,
                                       juce::dsp::Convolution::Trim::no,
                                       juce::dsp::Convolution::Normalise::no);

    // Linear Phase は IR の中央がピークなので、その分の遅れを DAW に伝えて補正してもらう。
    setLatencySamples (isLinear ? EqImpulseDesigner::linearPhaseLatencySamples : 0);
}

//==============================================================================
float PluginProcessor::readParameter (const char* id) const
{
    return parameters.getRawParameterValue (id)->load();
}

WidenerSettings PluginProcessor::readWidenerSettings() const
{
    WidenerSettings settings;
    settings.widthPercent     = readParameter (Parameters::Id::width);
    settings.spreadTimeMs     = readParameter (Parameters::Id::spreadTimeMs);
    settings.decorrelationPct = readParameter (Parameters::Id::decorrelation);
    settings.motionPercent    = readParameter (Parameters::Id::motion);
    settings.bassKeepHz       = readParameter (Parameters::Id::bassKeepHz);
    settings.centerLevelDb    = readParameter (Parameters::Id::centerLevelDb);
    settings.mixPercent       = readParameter (Parameters::Id::mix);
    return settings;
}

// 入力が 1ch ならそのままコピー、2ch なら平均して 1ch にする。
void PluginProcessor::mixInputDownToMono (const juce::AudioBuffer<float>& buffer, int startSample, int count)
{
    const int numInputChannels = getTotalNumInputChannels();
    const float* left = buffer.getReadPointer (0, startSample);

    if (numInputChannels >= 2)
    {
        const float* right = buffer.getReadPointer (1, startSample);
        for (int i = 0; i < count; ++i)
            monoScratch[static_cast<size_t> (i)] = 0.5f * (left[i] + right[i]);
    }
    else
    {
        for (int i = 0; i < count; ++i)
            monoScratch[static_cast<size_t> (i)] = left[i];
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto settings = readWidenerSettings();
    const bool monoCheck = readParameter (Parameters::Id::monoCheck) > 0.5f;

    // DAW が想定より大きなブロックを渡しても落ちないよう、作業用バッファの大きさごとに区切る。
    const int chunkSize = static_cast<int> (monoScratch.size());
    const int totalSamples = buffer.getNumSamples();

    for (int start = 0; start < totalSamples; start += chunkSize)
    {
        const int count = juce::jmin (chunkSize, totalSamples - start);

        // 1. モノラルにまとめる
        mixInputDownToMono (buffer, start, count);

        inputSpectrumAnalyzer.pushSamples (monoScratch.data(), count);

        // 2. EQ
        float* monoChannel[1] = { monoScratch.data() };
        juce::dsp::AudioBlock<float> monoBlock (monoChannel, 1, static_cast<size_t> (count));
        eqConvolution.process (juce::dsp::ProcessContextReplacing<float> (monoBlock));

        outputSpectrumAnalyzer.pushSamples (monoScratch.data(), count);

        // 3. 左右に広げる
        widener.process (monoScratch.data(),
                         buffer.getWritePointer (0, start),
                         buffer.getWritePointer (1, start),
                         count, settings);

        // 4. Mono Check: 左右を合算して、モノラル再生時の聞こえ方を確認する。
        if (monoCheck)
        {
            float* left  = buffer.getWritePointer (0, start);
            float* right = buffer.getWritePointer (1, start);
            for (int i = 0; i < count; ++i)
            {
                const float summed = 0.5f * (left[i] + right[i]);
                left[i] = summed;
                right[i] = summed;
            }
        }
    }
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
            undoManager.clearUndoHistory(); // 読み込み前の状態には戻さない
        }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

// DAW がプラグインを生成するときに呼ぶ入口。
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
