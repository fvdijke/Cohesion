#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    float faderGain (float v) { const float x = v / 0.8f; return x * x; }    // 0.8 = unity
    constexpr float kSynthGain = 0.5f;
}

juce::AudioProcessor::BusesProperties LaFontProcessor::makeBuses()
{
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (int p = 0; p < kParts; ++p) b = b.withOutput ("Part " + juce::String (p + 1), juce::AudioChannelSet::stereo(), false);
    return b;
}

LaFontProcessor::LaFontProcessor()
    : juce::AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "LaFont", createLayout())
{
    for (int p = 0; p < kParts; ++p)
    {
        rvGain[p] = choGain[p] = 1.0f;
        for (auto& v : lastFx[p]) v = -1.0f;
        auto raw = [&] (const char* w) { return apvts.getRawParameterValue (fpid::part (p, w)); };
        auto& q = pp[p];
        q.ch = raw ("ch"); q.lvl = raw ("lvl"); q.pan = raw ("pan"); q.mute = raw ("mute"); q.solo = raw ("solo"); q.out = raw ("out");
        q.rvOn = raw ("rv_on"); q.rvRoom = raw ("rv_room"); q.rvDamp = raw ("rv_damp"); q.rvWidth = raw ("rv_width"); q.rvLevel = raw ("rv_level");
        q.choOn = raw ("cho_on"); q.choVoices = raw ("cho_voices"); q.choLevel = raw ("cho_level"); q.choRate = raw ("cho_rate"); q.choDepth = raw ("cho_depth");
        engine.setPartProgram (p, p == 9 ? 128 : 0, 0);
    }
    pMaster = apvts.getRawParameterValue (fpid::master);
    // a fresh instance: every part on the first SoundFont of the library (GM layout)
    const auto lib = fontlib::paths();
    if (! lib.isEmpty()) setFontForAllParts (lib[0]);
}

LaFontProcessor::~LaFontProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout LaFontProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto fromPct = [] (const String& s) { return s.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; };
    auto floatPct = [&] (const String& id, const String& name, float lo, float hi, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.001f), def,
               AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (fromPct)));
    };
    auto floatUnit = [&] (const String& id, const String& name, float lo, float hi, float def, const String& unit, int decimals)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.01f), def,
               AudioParameterFloatAttributes().withStringFromValueFunction ([unit, decimals] (float v, int) { return String (v, decimals) + unit; })));
    };

    floatPct (fpid::master, "Master", 0.0f, 1.5f, 1.0f);
    StringArray chans { "OFF" }; for (int c = 1; c <= 16; ++c) chans.add ("CH " + String (c));
    for (int p = 0; p < kParts; ++p)
    {
        const String nm = "Part " + String (p + 1);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { fpid::part (p, "ch"), 1 }, nm + " MIDI", chans, p + 1));
        floatPct (fpid::part (p, "lvl"), nm + " Level", 0.0f, 1.0f, 0.8f);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { fpid::part (p, "pan"), 1 }, nm + " Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { const int x = juce::roundToInt (v * 100.0f); return x == 0 ? String ("C") : (x < 0 ? "L" + String (-x) : "R" + String (x)); })));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { fpid::part (p, "mute"), 1 }, nm + " Mute", false));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { fpid::part (p, "solo"), 1 }, nm + " Solo", false));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { fpid::part (p, "out"), 1 }, nm + " Output", StringArray { "MAIN", "OUT " + String (p + 2) }, 0));
        // FluidSynth reverb / chorus of this part's effects group (defaults = FluidSynth's)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { fpid::part (p, "rv_on"), 1 }, nm + " Reverb On", true));
        floatPct (fpid::part (p, "rv_room"), nm + " Reverb Room", 0.0f, 1.0f, 0.2f);
        floatPct (fpid::part (p, "rv_damp"), nm + " Reverb Damp", 0.0f, 1.0f, 0.0f);
        floatPct (fpid::part (p, "rv_width"), nm + " Reverb Width", 0.0f, 1.0f, 0.5f);
        floatPct (fpid::part (p, "rv_level"), nm + " Reverb Level", 0.0f, 1.0f, 0.9f);
        l.add (std::make_unique<AudioParameterBool> (ParameterID { fpid::part (p, "cho_on"), 1 }, nm + " Chorus On", true));
        l.add (std::make_unique<AudioParameterInt> (ParameterID { fpid::part (p, "cho_voices"), 1 }, nm + " Chorus Voices", 1, 10, 3));
        floatUnit (fpid::part (p, "cho_level"), nm + " Chorus Level", 0.0f, 4.0f, 2.0f, "", 1);
        floatUnit (fpid::part (p, "cho_rate"), nm + " Chorus Rate", 0.1f, 5.0f, 0.3f, " Hz", 1);
        floatUnit (fpid::part (p, "cho_depth"), nm + " Chorus Depth", 0.0f, 30.0f, 8.0f, " ms", 1);
    }
    return l;
}

bool LaFontProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int b = 1; b < layouts.outputBuses.size(); ++b)
    {
        const auto& s = layouts.outputBuses.getReference (b);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo()) return false;
    }
    return layouts.inputBuses.isEmpty();
}

void LaFontProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    blockMax = juce::jmax (32, samplesPerBlock);
    buf.assign ((size_t) (kParts * 6 * blockMax), 0.0f);
    engine.prepare (sr, kSynthGain);
    for (auto& r : lastFx) for (auto& v : r) v = -1.0f;
}

// ---------------------------------------------------------------------------
//  SoundFonts and presets (message thread)
// ---------------------------------------------------------------------------
juce::String LaFontProcessor::partPresetName (int part) const
{
    auto info = partInfo (part);
    return info != nullptr ? info->presetName (partBank (part), partProgram (part)) : juce::String();
}

void LaFontProcessor::setPartFont (int part, const juce::String& path, bool keepProgram)
{
    fonts[part] = path;
    if (! keepProgram)
    {
        // keep the program if the new SoundFont has it, else its first preset of the right kind
        auto info = path.isNotEmpty() ? sf2::get (path) : nullptr;
        int bank = engine.partBank (part), prog = engine.partProgram (part);
        if (info != nullptr && info->ok() && info->find (bank, prog) == nullptr)
        {
            const sf2::Preset* pick = nullptr;
            for (auto& pr : info->presets) if ((part == 9) == (pr.bank == 128)) { pick = &pr; break; }
            if (pick == nullptr) pick = &info->presets.front();
            bank = pick->bank; prog = pick->program;
        }
        engine.setPartProgram (part, bank, prog);
    }
    engine.setPartFont (part, path);
}

void LaFontProcessor::setFontForAllParts (const juce::String& path)
{
    for (int p = 0; p < kParts; ++p) setPartFont (p, path);
}

void LaFontProcessor::selectPreset (int part, int bank, int program) { engine.setPartProgram (part, bank, program); }

void LaFontProcessor::stepPreset (int part, int delta)
{
    auto info = partInfo (part);
    if (info == nullptr || info->presets.empty()) return;
    const int n = (int) info->presets.size();
    int i = 0;
    for (int k = 0; k < n; ++k)
        if (info->presets[(size_t) k].bank == partBank (part) && info->presets[(size_t) k].program == partProgram (part)) { i = k; break; }
    i = (i + delta + n) % n;
    selectPreset (part, info->presets[(size_t) i].bank, info->presets[(size_t) i].program);
}

// ---------------------------------------------------------------------------
//  Audio
// ---------------------------------------------------------------------------
void LaFontProcessor::updateEffects()
{
    for (int p = 0; p < kParts; ++p)
    {
        const auto& q = pp[p];
        const float v[8] = { q.rvRoom->load(), q.rvDamp->load(), q.rvWidth->load(), q.rvLevel->load(),
                             q.choVoices->load(), q.choLevel->load(), q.choRate->load(), q.choDepth->load() };
        if (std::equal (v, v + 4, lastFx[p]) == false)
            engine.setReverb (p, { v[0], v[1], v[2] * 100.0, v[3] });             // FluidSynth width: 0..100
        if (std::equal (v + 4, v + 8, lastFx[p] + 4) == false)
            engine.setChorus (p, { juce::roundToInt (v[4]), v[5], v[6], v[7] });
        std::copy (v, v + 8, lastFx[p]);
    }
}

void LaFontProcessor::handleMidi (const juce::MidiMessage& m)
{
    midiActivity = 1.0f;
    if (m.isSysEx()) { engine.sysex (m.getSysExData(), m.getSysExDataSize()); return; }
    const int ch = m.getChannel();
    if (ch <= 0) return;
    for (int p = 0; p < kParts; ++p)
    {
        if ((int) pp[p].ch->load() != ch) continue;
        if (m.isNoteOn()) activity[p] = 1.0f;
        engine.message (p, m.getRawData(), m.getRawDataSize());
    }
}

void LaFontProcessor::renderEngine (int from, int to)
{
    if (to <= from) return;
    FontEngine::Buffers b;
    for (int p = 0; p < kParts; ++p)
    {
        b.dryL[p] = pb (p, 0) + from; b.dryR[p] = pb (p, 1) + from;
        b.revL[p] = pb (p, 2) + from; b.revR[p] = pb (p, 3) + from;
        b.choL[p] = pb (p, 4) + from; b.choR[p] = pb (p, 5) + from;
    }
    engine.render (b, to - from);
}

void LaFontProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buf.empty()) return;
    updateEffects();
    if (! engine.tryBegin()) return;                     // synth busy (loading / re-creating): skip this block

    if (resetRequest.exchange (false)) engine.resetAll();
    if (panicRequest.exchange (false)) engine.allSoundsOff();

    auto it = midi.cbegin();
    for (int s0 = 0; s0 < n; s0 += blockMax)
    {
        const int len = juce::jmin (blockMax, n - s0);
        int cur = 0;
        while (it != midi.cend() && (*it).samplePosition < s0 + len)
        {
            const int t = juce::jlimit (0, len, (*it).samplePosition - s0);
            if (t > cur) { renderEngine (cur, t); cur = t; }
            handleMidi ((*it).getMessage());
            ++it;
        }
        if (cur < len) renderEngine (cur, len);

        // mixer: dry + this part's reverb + chorus -> the part's output
        auto main = getBusBuffer (buffer, false, 0);
        float* mL = main.getWritePointer (0, s0);
        float* mR = main.getNumChannels() > 1 ? main.getWritePointer (1, s0) : mL;
        const float step = 1.0f / (float) (0.01 * sr);
        bool anySolo = false;
        for (int p = 0; p < kParts; ++p) anySolo |= pp[p].solo->load() > 0.5f;
        for (int p = 0; p < kParts; ++p)
        {
            const auto& q = pp[p];
            const bool mute = q.mute->load() > 0.5f, solo = q.solo->load() > 0.5f;
            const float g = (mute || (anySolo && ! solo)) ? 0.0f : faderGain (q.lvl->load());
            const float pan = q.pan->load();
            const float gl = g * (pan > 0.0f ? 1.0f - pan : 1.0f), gr = g * (pan < 0.0f ? 1.0f + pan : 1.0f);
            float* oL = mL; float* oR = mR;
            if (q.out->load() > 0.5f)
                if (auto* bus = getBus (false, p + 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() == 2)
                {
                    auto aux = getBusBuffer (buffer, false, p + 1);
                    oL = aux.getWritePointer (0, s0); oR = aux.getWritePointer (1, s0);
                }
            const float rvT = q.rvOn->load() > 0.5f ? 1.0f : 0.0f, choT = q.choOn->load() > 0.5f ? 1.0f : 0.0f;
            const float *dL = pb (p, 0), *dR = pb (p, 1), *rL = pb (p, 2), *rR = pb (p, 3), *cL = pb (p, 4), *cR = pb (p, 5);
            for (int i = 0; i < len; ++i)
            {
                rvGain[p] += juce::jlimit (-step, step, rvT - rvGain[p]);
                choGain[p] += juce::jlimit (-step, step, choT - choGain[p]);
                oL[i] += (dL[i] + rL[i] * rvGain[p] + cL[i] * choGain[p]) * gl;
                oR[i] += (dR[i] + rR[i] * rvGain[p] + cR[i] * choGain[p]) * gr;
            }
        }
    }
    engine.end();
    buffer.applyGain (pMaster->load());
}

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------
void LaFontProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree sound ("Sound");
    for (int p = 0; p < kParts; ++p)
    {
        juce::ValueTree t ("Part");
        t.setProperty ("font", fonts[p], nullptr);
        t.setProperty ("bank", engine.partBank (p), nullptr);
        t.setProperty ("program", engine.partProgram (p), nullptr);
        sound.appendChild (t, nullptr);
    }
    state.removeChild (state.getChildWithName ("Sound"), nullptr);
    state.appendChild (sound, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void LaFontProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    const auto sound = state.getChildWithName ("Sound");
    apvts.replaceState (state);
    for (int p = 0; p < kParts && p < sound.getNumChildren(); ++p)
    {
        const auto t = sound.getChild (p);
        engine.setPartProgram (p, t.getProperty ("bank", p == 9 ? 128 : 0), t.getProperty ("program", 0));
        setPartFont (p, t.getProperty ("font").toString(), true);
    }
}

juce::AudioProcessorEditor* LaFontProcessor::createEditor() { return new LaFontEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LaFontProcessor(); }
