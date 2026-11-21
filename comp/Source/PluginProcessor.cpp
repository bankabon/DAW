#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
using juce::NormalisableRange;
using juce::ParameterID;

struct StageDefaults
{
    float threshold, ratio, knee, attack, release, detect, softClip, hpf;
};

constexpr StageDefaults stageDefaults[2] = {
    { -20.0f, 1.8f, 12.0f, 30.0f, 250.0f, 0.4f, 0.0f,  80.0f }, // Stage 1 (Gentle)
    { -24.0f, 4.0f,  8.0f,  8.0f, 120.0f, 0.2f, 0.3f, 120.0f }, // Stage 2 (Heavy)
};

NormalisableRange<float> skewed (float lo, float hi, float interval, float centre)
{
    NormalisableRange<float> r (lo, hi, interval);
    r.setSkewForCentre (centre);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> range, float def, const String& suffix)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withLabel (suffix)));
    };

    addFloat ("in_gain",  "Input",       { -24.0f, 24.0f, 0.01f }, 0.0f,   "dB");
    addFloat ("out_gain", "Output",      { -24.0f, 24.0f, 0.01f }, 0.0f,   "dB");
    addFloat ("mix",      "Mix",         { 0.0f, 100.0f, 0.1f },   100.0f, "%");
    addFloat ("depth",    "Depth",       { 0.0f, 100.0f, 0.1f },   100.0f, "%");
    addFloat ("link",     "Stereo Link", { 0.0f, 100.0f, 0.1f },   100.0f, "%");
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "avol", 1 }, "Auto Vol", false));
    addFloat ("avol_target", "Auto Vol Target", { -20.0f, 0.0f, 0.1f }, -5.0f, "dB");
    addFloat ("avol_range",  "Auto Vol Range",  { 0.0f, 18.0f, 0.1f },  12.0f, "dB");
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "ab", 1 }, "A/B Original (Level Matched)", false));

    for (int s = 1; s <= 2; ++s)
    {
        const auto& d = stageDefaults[s - 1];
        const String prefix = "Stage " + String (s) + " ";

        addFloat (stageId (s, "thr"),     prefix + "Threshold", { -60.0f, 0.0f, 0.1f },  d.threshold, "dB");
        addFloat (stageId (s, "ratio"),   prefix + "Ratio",     skewed (1.1f, 20.0f, 0.01f, 4.0f), d.ratio, ":1");
        addFloat (stageId (s, "knee"),    prefix + "Knee",      { 0.0f, 24.0f, 0.1f },   d.knee, "dB");
        addFloat (stageId (s, "attack"),  prefix + "Attack",    skewed (0.1f, 100.0f, 0.01f, 10.0f), d.attack, "ms");
        addFloat (stageId (s, "release"), prefix + "Release",   skewed (10.0f, 2000.0f, 0.1f, 200.0f), d.release, "ms");
        addFloat (stageId (s, "makeup"),  prefix + "Makeup",    { -12.0f, 24.0f, 0.01f }, 0.0f, "dB");
        addFloat (stageId (s, "detect"),  prefix + "Detect",    { 0.0f, 100.0f, 0.1f },  d.detect * 100.0f, "% RMS");
        addFloat (stageId (s, "soft"),    prefix + "Soft Clip", { 0.0f, 100.0f, 0.1f },  d.softClip * 100.0f, "%");
        addFloat (stageId (s, "hpf"),     prefix + "SC HPF",    skewed (20.0f, 400.0f, 1.0f, 100.0f), d.hpf, "Hz");
        addFloat (stageId (s, "deess"),   prefix + "SC HF",     { 0.0f, 12.0f, 0.1f },   0.0f, "dB");
        addFloat (stageId (s, "shmod"),   prefix + "SHMOD",     { 0.0f, 4.0f, 1.0f },    0.0f, "");
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { stageId (s, "bypass"), 1 }, prefix + "Bypass", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { stageId (s, "auto"), 1 }, prefix + "Auto Makeup", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { stageId (s, "autorel"), 1 }, prefix + "Auto Release", false));
    }

    return layout;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

void PluginProcessor::prepareToPlay (double sampleRate, int maxBlockSize)
{
    stage1.prepare (sampleRate);
    stage2.prepare (sampleRate);
    autoVol.prepare (sampleRate);
    dryBuffer.setSize (2, maxBlockSize);
    currentSampleRate = sampleRate;
    abSmoothCoef = 1.0f - std::exp (-1.0f / (0.010f * (float) sampleRate)); // A/B の切替は約10 ms で滑らかに
    prevInGain = juce::Decibels::decibelsToGain (raw ("in_gain"));
    prevOutGain = juce::Decibels::decibelsToGain (raw ("out_gain"));
    prevMix = raw ("mix") * 0.01f;
    abAmount = raw ("ab") > 0.5f ? 1.0f : 0.0f;
    dryPower = wetPower = 0.0f;
    prevMatchGain = 1.0f;
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

comp::Params PluginProcessor::readStage (int s, float depthWeight) const
{
    comp::Params p;
    p.thresholdDb = raw (stageId (s, "thr"));
    // Depth: Ratio を 1(効かない)と設定値の間で補間する
    p.ratio       = 1.0f + (raw (stageId (s, "ratio")) - 1.0f) * depthWeight;
    p.kneeDb      = raw (stageId (s, "knee"));
    p.attackMs    = raw (stageId (s, "attack"));
    p.releaseMs   = raw (stageId (s, "release"));
    p.makeupDb    = raw (stageId (s, "makeup"));
    p.detect      = raw (stageId (s, "detect")) * 0.01f;
    p.softClip    = raw (stageId (s, "soft")) * 0.01f;
    p.hpfHz       = raw (stageId (s, "hpf"));
    p.link        = raw ("link") * 0.01f;
    p.autoMakeup  = raw (stageId (s, "auto")) > 0.5f;
    p.deessDb     = raw (stageId (s, "deess"));
    p.autoRelease = raw (stageId (s, "autorel")) > 0.5f;
    p.shmod       = (int) std::lround (raw (stageId (s, "shmod")));
    return p;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = std::min (buffer.getNumChannels(), 2);
    if (numSamples == 0 || numChannels == 0)
        return;

    // Input Gain の前のレベルを測る
    measure (inPeak, buffer, numChannels);

    // Input Gain(ブロック内でなめらかに変える)
    const float inGain = juce::Decibels::decibelsToGain (raw ("in_gain"));
    buffer.applyGainRamp (0, numSamples, prevInGain, inGain);
    prevInGain = inGain;

    // Mix と A/B 用に原音を保存(確保済みのバッファに書くだけ)
    const bool haveDry = numSamples <= dryBuffer.getNumSamples();
    if (haveDry)
        for (int c = 0; c < numChannels; ++c)
            dryBuffer.copyFrom (c, 0, buffer, c, 0, numSamples);

    // Depth: 0..50% で Stage 1 が立ち上がり、50..100% で Stage 2 が加わる
    const float depth = raw ("depth") * 0.01f;
    const float w1 = juce::jlimit (0.0f, 1.0f, depth * 2.0f);
    const float w2 = juce::jlimit (0.0f, 1.0f, (depth - 0.5f) * 2.0f);

    float* const* data = buffer.getArrayOfWritePointers();

    if (raw (stageId (1, "bypass")) < 0.5f)
    {
        stage1.setParams (readStage (1, w1));
        stage1.process (data, numChannels, numSamples);
        stage1GrDb.store (stage1.getGainReductionDb());
    }
    else
    {
        stage1.reset();
        stage1GrDb.store (0.0f);
    }

    if (raw (stageId (2, "bypass")) < 0.5f)
    {
        stage2.setParams (readStage (2, w2));
        stage2.process (data, numChannels, numSamples);
        stage2GrDb.store (stage2.getGainReductionDb());
    }
    else
    {
        stage2.reset();
        stage2GrDb.store (0.0f);
    }

    // Output Gain
    const float outGain = juce::Decibels::decibelsToGain (raw ("out_gain"));
    buffer.applyGainRamp (0, numSamples, prevOutGain, outGain);
    prevOutGain = outGain;

    // Mix(原音は Output Gain を掛けず、Input Gain 後の音と混ぜる)
    const float mix = raw ("mix") * 0.01f;
    if (haveDry && (mix < 0.999f || prevMix < 0.999f))
    {
        for (int c = 0; c < numChannels; ++c)
        {
            auto* out = buffer.getWritePointer (c);
            const auto* dry = dryBuffer.getReadPointer (c);
            for (int i = 0; i < numSamples; ++i)
            {
                const float m = prevMix + (mix - prevMix) * (float) i / (float) numSamples;
                out[i] = out[i] * m + dry[i] * (1.0f - m);
            }
        }
    }
    prevMix = mix;

    // Auto Vol(最終段。目標ピークレベルに向けてゆっくり音量を揃える)
    autoVol.setParams (raw ("avol") > 0.5f, raw ("avol_target"), raw ("avol_range"));
    autoVol.process (data, numChannels, numSamples);

    // A/B(音量を揃えた原音との比較)
    if (haveDry)
    {
        // 原音とコンプ後(最終)の平均パワーを約1秒で追い、原音側をコンプ後の音量に合わせる
        float drySum = 0.0f, wetSum = 0.0f;
        for (int c = 0; c < numChannels; ++c)
        {
            const auto* dry = dryBuffer.getReadPointer (c);
            const auto* wet = buffer.getReadPointer (c);
            for (int i = 0; i < numSamples; ++i)
            {
                drySum += dry[i] * dry[i];
                wetSum += wet[i] * wet[i];
            }
        }
        const float norm = 1.0f / (float) (numSamples * numChannels);
        if (drySum * norm > 1.0e-9f) // 無音の間は値を保持する
        {
            const float a = std::exp (-(float) numSamples / (float) currentSampleRate);
            dryPower = a * dryPower + (1.0f - a) * drySum * norm;
            wetPower = a * wetPower + (1.0f - a) * wetSum * norm;
        }
        const float matchGain = juce::jlimit (0.063f, 15.8f, std::sqrt ((wetPower + 1.0e-12f) / (dryPower + 1.0e-12f)));

        const float abTarget = raw ("ab") > 0.5f ? 1.0f : 0.0f;
        if (abTarget > 0.5f || abAmount > 1.0e-4f)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                abAmount += (abTarget - abAmount) * abSmoothCoef;
                const float g = prevMatchGain + (matchGain - prevMatchGain) * (float) i / (float) numSamples;
                for (int c = 0; c < numChannels; ++c)
                {
                    auto* out = buffer.getWritePointer (c);
                    out[i] = out[i] * (1.0f - abAmount) + dryBuffer.getReadPointer (c)[i] * g * abAmount;
                }
            }
        }
        else
        {
            abAmount = 0.0f;
        }
        prevMatchGain = matchGain;
    }

    // 最終出力のレベルを測る
    measure (outPeak, buffer, numChannels);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

int PluginProcessor::getNumPrograms() { return (int) presets::all.size(); }

const juce::String PluginProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (presets::all[(size_t) index].name) : juce::String();
}

void PluginProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void PluginProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    const auto& pr = presets::all[(size_t) index];

    setParam ("depth", pr.depthPct);
    setParam ("in_gain", 0.0f);
    setParam ("out_gain", 0.0f);
    setParam ("mix", 100.0f);
    setParam ("link", 100.0f);
    setParam ("ab", 0.0f);
    setParam ("avol", 0.0f);

    const presets::Stage* stages[2] = { &pr.s1, &pr.s2 };
    for (int s = 1; s <= 2; ++s)
    {
        const auto& v = *stages[s - 1];
        setParam (stageId (s, "thr"),     v.thr);
        setParam (stageId (s, "ratio"),   v.ratio);
        setParam (stageId (s, "knee"),    v.knee);
        setParam (stageId (s, "attack"),  v.attack);
        setParam (stageId (s, "release"), v.release);
        setParam (stageId (s, "makeup"),  v.makeup);
        setParam (stageId (s, "detect"),  v.detectPct);
        setParam (stageId (s, "soft"),    v.softPct);
        setParam (stageId (s, "hpf"),     v.hpf);
        setParam (stageId (s, "deess"),   v.deess);
        setParam (stageId (s, "autorel"), v.autoRel);
        setParam (stageId (s, "shmod"),   v.shmod);
        setParam (stageId (s, "bypass"),  0.0f);
        setParam (stageId (s, "auto"),    0.0f);
    }

    currentProgram.store (index);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentProgram.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PluginProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            currentProgram.store (juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("preset", 0)));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
