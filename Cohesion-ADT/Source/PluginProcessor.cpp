#include "PluginProcessor.h"
#include "PluginEditor.h"

// ===========================================================================
//  Factory presets — homage names to well-known double-tracked vocals.
// ===========================================================================
namespace
{
    struct Preset
    {
        const char* name;
        float mix, outGain, delayMs, spread, detune; int voices;
        float lfoRate, lfoDepth, humanize; int shape;
        bool invert; float rotate; bool correct;
        float width, lowMono; bool bassMono, vintage, monoOut;
    };

    // name           mix  out  dly  spr  det  v  rate dep  hum  sh inv rot cor  wid  low  bm   vin  mono
    const Preset kPresets[] = {
        { "Init",            50, 0, 24, 60,  8, 1, 0.8f, 35, 20, 0, false,  0, false, 140, 120, true,  false, false },
        { "Lennon Wide",     55, 0, 22, 90, 12, 1, 0.9f, 50, 30, 0, false,  0, false, 170, 110, true,  true,  false },
        { "Abbey Road '66",  48, 0, 28, 55,  7, 1, 0.6f, 30, 18, 0, false, 20, true,  120, 130, true,  true,  false },
        { "Tomorrow Never",  60, 0, 30, 75, 18, 1, 1.4f, 70, 40, 1, false,  0, false, 160, 120, true,  true,  false },
        { "Ob-La-Di Double", 50, 0, 18, 45,  6, 1, 0.7f, 25, 15, 0, false,  0, false, 110, 140, true,  false, false },
        { "Elliott Smith",   52, 0, 16, 40,  5, 1, 0.5f, 20, 15, 0, false,  0, false,  95, 150, true,  false, false },
        { "Grunge Double",   55, 0, 26, 70, 14, 1, 1.0f, 55, 30, 0, false,  0, false, 150, 120, true,  true,  false },
        { "Ozzy Thick",      55, 0, 24, 80, 16, 1, 1.1f, 60, 35, 0, false,  0, false, 155, 115, true,  false, false },
        { "Wide Chorus",     55, 0, 32,100, 20, 1, 1.6f, 80, 45, 0, false,  0, false, 180, 120, true,  false, false },
        { "Subtle Slap",     38, 0, 14, 30,  4, 1, 0.4f, 15, 10, 0, false,  0, false, 105, 160, true,  false, false },
    };
    constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

CohesionADTAudioProcessor::CohesionADTAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CohesionADTAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Dry/Wet",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Output",
                NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "delayMs", 1 }, "Delay",
                NormalisableRange<float> (5.0f, 40.0f, 0.1f), 24.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "spread", 1 }, "Spread",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "detune", 1 }, "Detune",
                NormalisableRange<float> (0.0f, 30.0f, 0.1f), 8.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "voices", 1 }, "Voices",
                StringArray { "1", "2" }, 1));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfoRate", 1 }, "Mod Rate",
                NormalisableRange<float> (0.1f, 5.0f, 0.01f, 0.5f), 0.8f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfoDepth", 1 }, "Mod Depth",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfoShape", 1 }, "Mod Shape",
                StringArray { "Sine", "Random" }, 0));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "phInvert", 1 }, "Phase Invert", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "phRotate", 1 }, "Phase Rotate",
                NormalisableRange<float> (0.0f, 180.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "phCorrect", 1 }, "Phase Correct", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "phAlign", 1 }, "Phase Align",
                NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width",
                NormalisableRange<float> (0.0f, 200.0f, 0.1f), 140.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lowMono", 1 }, "Low Mono Freq",
                NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.4f), 120.0f));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "bassMono", 1 }, "Bass Mono", true));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vintage", 1 }, "Vintage", false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "monoOut", 1 }, "Mono Out", false));

    return layout;
}

void CohesionADTAudioProcessor::setP (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

int CohesionADTAudioProcessor::getNumPrograms() { return kNumPresets; }

const juce::String CohesionADTAudioProcessor::getProgramName (int i)
{
    return (i >= 0 && i < kNumPresets) ? juce::String (kPresets[i].name) : juce::String();
}

void CohesionADTAudioProcessor::setCurrentProgram (int index)
{
    currentProgram = juce::jlimit (0, kNumPresets - 1, index);
    const auto& p = kPresets[currentProgram];
    setP ("mix", p.mix);           setP ("outGain", p.outGain);
    setP ("delayMs", p.delayMs);   setP ("spread", p.spread);     setP ("detune", p.detune);
    setP ("voices", (float) p.voices);
    setP ("lfoRate", p.lfoRate);   setP ("lfoDepth", p.lfoDepth); setP ("humanize", p.humanize);
    setP ("lfoShape", (float) p.shape);
    setP ("phInvert", p.invert ? 1.0f : 0.0f); setP ("phRotate", p.rotate); setP ("phCorrect", p.correct ? 1.0f : 0.0f);
    setP ("width", p.width);       setP ("lowMono", p.lowMono);   setP ("bassMono", p.bassMono ? 1.0f : 0.0f);
    setP ("vintage", p.vintage ? 1.0f : 0.0f); setP ("monoOut", p.monoOut ? 1.0f : 0.0f);
}

void CohesionADTAudioProcessor::prepareToPlay (double fs, int)
{
    sampleRate = fs;
    delayLine.prepare ((int) (fs * 0.12) + 16);
    delayLine.reset();
    lfo[0].prepare (fs, 0.0f);
    lfo[1].prepare (fs, 0.5f);
    apL.reset(); apR.reset(); apL2.reset(); apR2.reset(); sideHP.reset(); vintL.reset(); vintR.reset();
    scopeHop = juce::jmax (16, (int) (fs / (double) (kScope * 30)));   // ~30 scope frames/sec
    for (auto* s : { &mixSm, &widthSm, &outSm, &delaySm, &spreadSm, &depthSm }) s->reset (fs, 0.02);
}

bool CohesionADTAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CohesionADTAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh   = juce::jmin (buffer.getNumChannels(), 2);
    const int numSmps = buffer.getNumSamples();

    if (apvts.getRawParameterValue ("bypass")->load() > 0.5f)
        return;

    const int   voices   = 1 + (int) apvts.getRawParameterValue ("voices")->load();   // 1 or 2
    const float detune   = apvts.getRawParameterValue ("detune")->load();
    const float humanize = apvts.getRawParameterValue ("humanize")->load();
    const int   shape    = (int) apvts.getRawParameterValue ("lfoShape")->load();
    const bool  invert   = apvts.getRawParameterValue ("phInvert")->load() > 0.5f;
    const bool  correct  = apvts.getRawParameterValue ("phCorrect")->load() > 0.5f;
    const float rotate   = apvts.getRawParameterValue ("phRotate")->load();
    const bool  bassMono = apvts.getRawParameterValue ("bassMono")->load() > 0.5f;
    const bool  vintage  = apvts.getRawParameterValue ("vintage")->load() > 0.5f;
    const bool  monoOut  = apvts.getRawParameterValue ("monoOut")->load() > 0.5f;
    const float lfoRate  = apvts.getRawParameterValue ("lfoRate")->load();
    const float alignMs  = apvts.getRawParameterValue ("phAlign")->load() * 0.01f * 3.0f;   // fine time-align (0..3 ms)

    mixSm.setTargetValue   (apvts.getRawParameterValue ("mix")->load() * 0.015f);   // up to 150% wet
    widthSm.setTargetValue (apvts.getRawParameterValue ("width")->load() * 0.01f);
    outSm.setTargetValue   (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outGain")->load()));
    delaySm.setTargetValue (apvts.getRawParameterValue ("delayMs")->load());
    spreadSm.setTargetValue (apvts.getRawParameterValue ("spread")->load() * 0.01f);
    depthSm.setTargetValue (apvts.getRawParameterValue ("lfoDepth")->load());

    lfo[0].setShape (shape); lfo[1].setShape (shape);
    lfo[0].setRate (lfoRate);
    lfo[1].setRate (lfoRate * (1.0f + humanize * 0.0025f));   // humanize: slightly different rate
    apL.setCoef (rotate / 180.0f * 0.96f);  apR.setCoef (rotate / 180.0f * 0.96f);
    apL2.setCoef (rotate / 180.0f * 0.96f); apR2.setCoef (rotate / 180.0f * 0.96f);
    sideHP.setFreq (apvts.getRawParameterValue ("lowMono")->load(), sampleRate);
    vintL.setFreq (9000.0, sampleRate);
    vintR.setFreq (9000.0, sampleRate);

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const float msToSamp = (float) sampleRate / 1000.0f;
    float blockPeak = 0.0f;

    for (int n = 0; n < numSmps; ++n)
    {
        const float dryL = L[n];
        const float dryR = R ? R[n] : L[n];
        const float mono = 0.5f * (dryL + dryR);

        delayLine.push (mono);

        const float base   = delaySm.getNextValue();
        const float spread = spreadSm.getNextValue();
        const float depthMs = depthSm.getNextValue() * 0.025f + detune * 0.04f;   // pitch wobble (ms)
        const float lv0 = lfo[0].next();
        const float lv1 = lfo[1].next();

        // mono double image from the voices (SPREAD = timing difference -> thickness)
        float wetM = 0.0f;
        for (int v = 0; v < voices; ++v)
        {
            const float lfov  = (v == 0 ? lv0 : lv1);
            const float tspr  = (voices == 1) ? 0.0f : (v == 0 ? -1.0f : 1.0f) * spread * 0.22f;
            const float baseV = base * (1.0f + tspr) - alignMs;
            const float dMs   = juce::jlimit (1.0f, 110.0f, baseV + lfov * depthMs);
            wetM += delayLine.read (dMs * msToSamp);
        }
        wetM /= (float) voices;

        // phase: ROTATE all-pass (always active), CORRECT adds a stage, INVERT flips polarity
        if (rotate > 0.5f || correct)
        {
            wetM = apL.process (wetM);
            if (correct) wetM = apL2.process (wetM);
        }
        if (invert)  wetM = -wetM;
        if (vintage) wetM = std::tanh (vintL.lp (wetM) * 1.15f);

        // WIDTH = separation of ORIGINAL vs the ADT double.
        // 0% = double centred (thickening); 100% = original hard-left, double hard-right.
        const float sep = juce::jlimit (0.0f, 1.0f, widthSm.getNextValue());
        const float m   = mixSm.getNextValue();
        float outL = dryL                  + wetM * m * (1.0f - sep);
        float outR = dryR * (1.0f - sep)   + wetM * m;

        if (bassMono)   // keep the low end centred regardless of separation
        {
            const float mid  = 0.5f * (outL + outR);
            const float side = sideHP.hp (0.5f * (outL - outR));
            outL = mid + side; outR = mid - side;
        }
        if (monoOut) { const float mo = 0.5f * (outL + outR); outL = outR = mo; }

        const float og = outSm.getNextValue();
        L[n] = std::tanh (outL * og);          // gentle soft-clip -> no overload crackle
        if (R) R[n] = std::tanh (outR * og);
        blockPeak = juce::jmax (blockPeak, juce::jmax (std::abs (L[n]), R ? std::abs (R[n]) : 0.0f));

        // scope (input peak envelope)
        scopePeak = juce::jmax (scopePeak, std::abs (mono));
        if (++scopeCount >= scopeHop)
        {
            const int p = scopePos.load();
            scope[p] = scopePeak;
            scopePos.store ((p + 1) % kScope);
            scopePeak = 0.0f; scopeCount = 0;
        }
    }

    outPeak.store (juce::jmax (outPeak.load() * 0.8f, blockPeak));
}

void CohesionADTAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CohesionADTAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CohesionADTAudioProcessor::createEditor()
{
    return new CohesionADTAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CohesionADTAudioProcessor();
}
