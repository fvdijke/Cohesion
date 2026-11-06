#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const char* bandTypeChoices[] = { "Bell", "Low Shelf", "High Shelf" };
    const char* mbModeChoices[]   = { "Comp", "Exp", "Gate" };
    const float defaultFreq[CohesionEQCompAudioProcessor::kNumBands] = { 80.0f, 200.0f, 500.0f, 1500.0f, 4000.0f, 10000.0f };
    const float defaultXover[4] = { 150.0f, 700.0f, 3000.0f, 8000.0f };

    juce::String bandId (int b, const char* field) { return "b"  + juce::String (b) + "_" + field; }
    juce::String mbId   (int b, const char* field) { return "mb" + juce::String (b) + "_" + field; }
}

CohesionEQCompAudioProcessor::CohesionEQCompAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CohesionEQCompAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "eqOn", 1 }, "EQ On", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "chainOrder", 1 }, "Comp before EQ", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 }, "Input",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "hpOn", 1 }, "HPF On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hpFreq", 1 }, "HPF Freq",
                                                       NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.3f), 30.0f));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "lpOn", 1 }, "LPF On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lpFreq", 1 }, "LPF Freq",
                                                       NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));

    for (int b = 0; b < kNumBands; ++b)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { bandId (b, "on"), 1 },
                    "Band " + String (b + 1) + " On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "freq"), 1 },
                    "Band " + String (b + 1) + " Freq",
                    NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f), defaultFreq[b]));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "gain"), 1 },
                    "Band " + String (b + 1) + " Gain",
                    NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "q"), 1 },
                    "Band " + String (b + 1) + " Q",
                    NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.35f), 1.0f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { bandId (b, "type"), 1 },
                    "Band " + String (b + 1) + " Type",
                    StringArray (bandTypeChoices, 3), 0));
    }

    // ---- output limiter ----
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "limOn", 1 }, "Limiter On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "limCeil", 1 }, "Limiter Ceiling",
                                                       NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "limRel", 1 }, "Limiter Release",
                                                       NormalisableRange<float> (1.0f, 500.0f, 1.0f, 0.4f), 60.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "limPush", 1 }, "Limiter Push",
                                                       NormalisableRange<float> (0.0f, 18.0f, 0.1f), 0.0f));

    // ---- multiband ----
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "mbOn", 1 }, "Multiband On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mbBands", 1 }, "Bands",
                StringArray { "2", "3", "4", "5" }, 2));
    const char* xName[4] = { "X1", "X2", "X3", "X4" };
    const float xLo[4] = { 20.0f, 200.0f, 1000.0f, 4000.0f };
    const float xHi[4] = { 500.0f, 2000.0f, 6000.0f, 18000.0f };
    for (int i = 0; i < 4; ++i)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { juce::String ("x") + juce::String (i + 1), 1 },
                    xName[i], NormalisableRange<float> (xLo[i], xHi[i], 1.0f, 0.3f), defaultXover[i]));

    for (int b = 0; b < ceqc::Multiband::kMaxBands; ++b)
    {
        const String n = "MB " + String (b + 1) + " ";
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { mbId (b, "mode"), 1 }, n + "Mode",
                    StringArray (mbModeChoices, 3), 0));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { mbId (b, "thr"), 1 }, n + "Threshold",
                    NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { mbId (b, "ratio"), 1 }, n + "Ratio",
                    NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.4f), 2.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { mbId (b, "atk"), 1 }, n + "Attack",
                    NormalisableRange<float> (0.1f, 100.0f, 0.1f, 0.4f), 20.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { mbId (b, "rel"), 1 }, n + "Release",
                    NormalisableRange<float> (5.0f, 1000.0f, 1.0f, 0.4f), 150.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { mbId (b, "gain"), 1 }, n + "Gain",
                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { mbId (b, "width"), 1 }, n + "Width",
                    NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));
        layout.add (std::make_unique<AudioParameterBool>   (ParameterID { mbId (b, "solo"), 1 }, n + "Solo", false));
        layout.add (std::make_unique<AudioParameterBool>   (ParameterID { mbId (b, "mute"), 1 }, n + "Mute", false));
        layout.add (std::make_unique<AudioParameterBool>   (ParameterID { mbId (b, "byp"), 1 }, n + "Bypass", false));
    }

    return layout;
}

void CohesionEQCompAudioProcessor::prepareToPlay (double fs, int)
{
    sampleRate = fs;
    for (int ch = 0; ch < 2; ++ch)
    {
        hp[ch].setSampleRate (fs); hp[ch].reset();
        lp[ch].setSampleRate (fs); lp[ch].reset();
        for (int b = 0; b < kNumBands; ++b) { band[b][ch].setSampleRate (fs); band[b][ch].reset(); }
    }
    multiband.prepare (fs);
    limiter.prepare (fs, 2);
    inGainSm.reset  (fs, 0.02);
    outGainSm.reset (fs, 0.02);
}

bool CohesionEQCompAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CohesionEQCompAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh   = juce::jmin (buffer.getNumChannels(), 2);
    const int numSmps = buffer.getNumSamples();

    if (apvts.getRawParameterValue ("bypass")->load() > 0.5f)
        return;

    inGainSm.setTargetValue  (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("inGain")->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outGain")->load()));

    const bool  eqOn   = apvts.getRawParameterValue ("eqOn")->load() > 0.5f;
    const bool  hpOn   = apvts.getRawParameterValue ("hpOn")->load() > 0.5f;
    const float hpFreq = apvts.getRawParameterValue ("hpFreq")->load();
    const bool  lpOn   = apvts.getRawParameterValue ("lpOn")->load() > 0.5f;
    const float lpFreq = apvts.getRawParameterValue ("lpFreq")->load();
    const bool  compFirst = apvts.getRawParameterValue ("chainOrder")->load() > 0.5f;

    bool  bandOn[kNumBands];
    for (int b = 0; b < kNumBands; ++b)
    {
        bandOn[b] = apvts.getRawParameterValue (bandId (b, "on"))->load() > 0.5f;
        if (! bandOn[b]) continue;

        const float freq = apvts.getRawParameterValue (bandId (b, "freq"))->load();
        const float gain = apvts.getRawParameterValue (bandId (b, "gain"))->load();
        const float q    = apvts.getRawParameterValue (bandId (b, "q"))->load();
        const int   type = (int) apvts.getRawParameterValue (bandId (b, "type"))->load();
        const auto  svfType = (type == 1 ? ceqc::SVF::LowShelf : type == 2 ? ceqc::SVF::HighShelf : ceqc::SVF::Bell);

        for (int ch = 0; ch < numCh; ++ch)
            band[b][ch].set (svfType, freq, q, gain);
    }

    if (hpOn) for (int ch = 0; ch < numCh; ++ch) hp[ch].set (ceqc::SVF::Highpass, hpFreq, 0.70710678f, 0.0f);
    if (lpOn) for (int ch = 0; ch < numCh; ++ch) lp[ch].set (ceqc::SVF::Lowpass,  lpFreq, 0.70710678f, 0.0f);

    // ---- multiband setup ----
    const bool mbOn = apvts.getRawParameterValue ("mbOn")->load() > 0.5f;
    if (mbOn)
    {
        const int nb = 2 + (int) apvts.getRawParameterValue ("mbBands")->load();
        multiband.setBands (nb);
        const float xf[4] = { apvts.getRawParameterValue ("x1")->load(),
                              apvts.getRawParameterValue ("x2")->load(),
                              apvts.getRawParameterValue ("x3")->load(),
                              apvts.getRawParameterValue ("x4")->load() };
        multiband.setFreqs (xf);
        for (int b = 0; b < nb; ++b)
            multiband.setBand (b,
                (int) apvts.getRawParameterValue (mbId (b, "mode"))->load(),
                apvts.getRawParameterValue (mbId (b, "thr"))->load(),
                apvts.getRawParameterValue (mbId (b, "ratio"))->load(),
                apvts.getRawParameterValue (mbId (b, "atk"))->load(),
                apvts.getRawParameterValue (mbId (b, "rel"))->load(),
                apvts.getRawParameterValue (mbId (b, "gain"))->load(),
                apvts.getRawParameterValue (mbId (b, "width"))->load(),
                apvts.getRawParameterValue (mbId (b, "solo"))->load() > 0.5f,
                apvts.getRawParameterValue (mbId (b, "mute"))->load() > 0.5f,
                apvts.getRawParameterValue (mbId (b, "byp"))->load()  > 0.5f);
    }

    float frame[2] { 0.0f, 0.0f };
    auto runEq = [&] (int ch, float x)
    {
        if (hpOn) x = hp[ch].process (x);
        for (int b = 0; b < kNumBands; ++b) if (bandOn[b]) x = band[b][ch].process (x);
        if (lpOn) x = lp[ch].process (x);
        return x;
    };
    for (int n = 0; n < numSmps; ++n)
    {
        const float ig = inGainSm.getNextValue();
        const float og = outGainSm.getNextValue();

        float preSum = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = buffer.getWritePointer (ch)[n] * ig;
            preSum += x;                              // input, post input-gain (the "before")
            frame[ch] = x;
        }

        if (! compFirst)   // EQ -> COMP
        {
            if (eqOn) for (int ch = 0; ch < numCh; ++ch) frame[ch] = runEq (ch, frame[ch]);
            if (mbOn) multiband.process (frame, numCh);
        }
        else               // COMP -> EQ
        {
            if (mbOn) multiband.process (frame, numCh);
            if (eqOn) for (int ch = 0; ch < numCh; ++ch) frame[ch] = runEq (ch, frame[ch]);
        }

        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[n] = frame[ch] * og;

        const float monoPre = (numCh > 1) ? 0.5f * preSum : preSum;
        pushAnalysisSamplePre (monoPre);
        const float mono = (numCh > 1) ? 0.5f * (frame[0] + frame[1]) * og : frame[0] * og;
        pushAnalysisSample (mono);
    }

    // ---- output limiter (after output gain, guarantees the ceiling) ----
    limiter.setEnabled (apvts.getRawParameterValue ("limOn")->load() > 0.5f);
    limiter.setCeiling (apvts.getRawParameterValue ("limCeil")->load());
    limiter.setRelease (apvts.getRawParameterValue ("limRel")->load());
    limiter.setPush    (apvts.getRawParameterValue ("limPush")->load());
    float* chans[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    limiter.process (chans, numCh, numSmps);

    const int nbCur = mbOn ? (2 + (int) apvts.getRawParameterValue ("mbBands")->load()) : 0;
    for (int b = 0; b < ceqc::Multiband::kMaxBands; ++b)
        bandGaindB[b].store (b < nbCur ? multiband.fetchGR (b) : 0.0f);
}

void CohesionEQCompAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CohesionEQCompAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CohesionEQCompAudioProcessor::createEditor()
{
    return new CohesionEQCompAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CohesionEQCompAudioProcessor();
}
