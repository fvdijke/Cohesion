#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    float faderGain (float v) { const float x = v / 0.8f; return x * x; }    // 0.8 = unity
}

juce::AudioProcessor::BusesProperties LaModuleProcessor::makeBuses()
{
    {
        auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
        for (int p = 0; p < mt::kParts; ++p)
            b = b.withOutput (p < 8 ? "Part " + juce::String (p + 1) : juce::String ("Rhythm"), juce::AudioChannelSet::stereo(), false);
        return b;
    }
}

LaModuleProcessor::LaModuleProcessor()
    : juce::AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "LaModule", createLayout())
{
    for (int p = 0; p < mt::kParts; ++p) { lastVolume[p] = -1; lastPan[p] = -1; lastExpr[p] = -1; }
    for (int p = 0; p < mt::kParts; ++p)
    {
        auto raw = [&] (const char* w) { return apvts.getRawParameterValue (lpid::part (p, w)); };
        pCh[p] = raw ("ch"); pLvl[p] = raw ("lvl"); pPan[p] = raw ("pan"); pRev[p] = raw ("rev");
        pEch[p] = raw ("ech"); pMute[p] = raw ("mute"); pSolo[p] = raw ("solo"); pOut[p] = raw ("out");
    }
    auto rawId = [&] (const char* id) { return apvts.getRawParameterValue (id); };
    pEngine = rawId (lpid::engine); pMaster = rawId (lpid::master);
    pEcSync = rawId (lpid::ecSync); pEcDiv = rawId (lpid::ecDiv);
    pEcTime = rawId (lpid::ecTime); pEcFb = rawId (lpid::ecFb);
    pEcLevel = rawId (lpid::ecLevel);
    pRvOn = rawId (lpid::rvOn); pEcOn = rawId (lpid::ecOn);
    for (auto* id : { lpid::rvMode, lpid::rvTime, lpid::rvLevel }) apvts.addParameterListener (id, this);

    const auto sets = romlib::storedSets();
    if (! sets.empty()) setRom (sets.front()->label);
    active = &laEngine;
    startTimerHz (20);
}

LaModuleProcessor::~LaModuleProcessor()
{
    stopTimer();
    for (auto* id : { lpid::rvMode, lpid::rvTime, lpid::rvLevel }) apvts.removeParameterListener (id, this);
    munt.close();
}

juce::AudioProcessorValueTreeState::ParameterLayout LaModuleProcessor::createLayout()
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

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { lpid::engine, 1 }, "Engine", StringArray { "MUNT", "COHESION LA" }, 0));
    floatPct (lpid::master, "Master", 0.0f, 1.5f, 1.0f);

    StringArray chans { "OFF" }; for (int c = 1; c <= 16; ++c) chans.add ("CH " + String (c));
    for (int p = 0; p < mt::kParts; ++p)
    {
        const String nm = p < 8 ? "Part " + String (p + 1) : String ("Rhythm");
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { lpid::part (p, "ch"), 1 }, nm + " MIDI", chans, p < 8 ? p + 2 : 10));
        floatPct (lpid::part (p, "lvl"), nm + " Level", 0.0f, 1.0f, 0.8f);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { lpid::part (p, "pan"), 1 }, nm + " Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { const int x = juce::roundToInt (v * 100.0f); return x == 0 ? String ("C") : (x < 0 ? "L" + String (-x) : "R" + String (x)); })));
        floatPct (lpid::part (p, "rev"), nm + " Reverb", 0.0f, 1.0f, 1.0f);
        floatPct (lpid::part (p, "ech"), nm + " Echo", 0.0f, 1.0f, 0.0f);
        l.add (std::make_unique<AudioParameterBool> (ParameterID { lpid::part (p, "mute"), 1 }, nm + " Mute", false));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { lpid::part (p, "solo"), 1 }, nm + " Solo", false));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { lpid::part (p, "out"), 1 }, nm + " Output",
               StringArray { "MAIN", "OUT " + String (p + 2) }, 0));
    }
    l.add (std::make_unique<AudioParameterBool> (ParameterID { lpid::rvOn, 1 }, "Reverb On", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { lpid::ecOn, 1 }, "Echo On", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { lpid::rvMode, 1 }, "Reverb Mode", StringArray { "ROOM", "HALL", "PLATE", "TAP DELAY" }, 0));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { lpid::rvTime, 1 }, "Reverb Time", 0, 7, 5));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { lpid::rvLevel, 1 }, "Reverb Level", 0, 7, 3));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { lpid::ecSync, 1 }, "Echo Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { lpid::ecDiv, 1 }, "Echo Division",
           StringArray { "1/2", "1/4", "1/4 D", "1/8", "1/8 D", "1/8 T", "1/16" }, 4));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { lpid::ecTime, 1 }, "Echo Time", NormalisableRange<float> (20.0f, 1500.0f, 1.0f, 0.5f), 375.0f,
           AudioParameterFloatAttributes().withLabel ("ms")));
    floatPct (lpid::ecFb, "Echo Feedback", 0.0f, 0.9f, 0.35f);
    floatPct (lpid::ecLevel, "Echo Level", 0.0f, 1.0f, 0.5f);
    return l;
}

bool LaModuleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int b = 1; b < layouts.outputBuses.size(); ++b)
    {
        const auto& s = layouts.outputBuses.getReference (b);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo()) return false;
    }
    return layouts.inputBuses.isEmpty();
}

void LaModuleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    blockMax = juce::jmax (32, samplesPerBlock);
    partBuf.assign ((size_t) (mt::kParts * 4 * blockMax), 0.0f);
    busBuf.assign ((size_t) (8 * blockMax), 0.0f);
    laEngine.prepare (sr, blockMax);
    munt.prepare (sr, blockMax);
    reverb.prepare (sr); tapDelay.prepare (sr); echo.prepare (sr);
    needResync = true;
}

// ---------------------------------------------------------------------------
//  ROM sets
// ---------------------------------------------------------------------------
juce::StringArray LaModuleProcessor::romLabels() const
{
    juce::StringArray s;
    for (auto& r : romlib::storedSets()) s.add (r->label);
    return s;
}

bool LaModuleProcessor::setRom (const juce::String& label, bool keepMemory)
{
    std::shared_ptr<const la32::Rom> rom;
    for (auto& r : romlib::storedSets()) if (r->label == label) rom = r;
    if (rom == nullptr) return false;
    const auto dir = romlib::setDir (label);
    const juce::ScopedLock sl (getCallbackLock());
    if (active != nullptr) active->allSoundOff();
    if (keepMemory) mem.rom = rom; else mem.init (rom);
    romLabel = label;
    munt.close();
    if (MuntEngine::available()) munt.open (dir.getChildFile ("control.rom"), dir.getChildFile ("pcm.rom"));
    munt.prepare (sr, blockMax);
    needResync = true;
    ++mem.version;
    return true;
}

romlib::ImportResult LaModuleProcessor::importRoms (const juce::Array<juce::File>& items)
{
    std::vector<uint64_t> known;
    for (auto& r : romlib::storedSets()) known.push_back (r->contentHash);
    auto res = romlib::importRoms (items, known);
    if (mem.rom == nullptr && ! res.added.empty()) setRom (res.added.front()->label);
    return res;
}

juce::String LaModuleProcessor::muntStatus() const
{
    if (! MuntEngine::available()) return MuntEngine::unavailableReason();
    if (mem.rom == nullptr) return "no ROM set";
    if (! munt.isOpen()) return "Munt could not open this ROM set";
    return {};
}

// ---------------------------------------------------------------------------
//  UI -> audio commands
// ---------------------------------------------------------------------------
bool LaModuleProcessor::push (const Cmd& c)
{
    static juce::SpinLock lock;
    const juce::SpinLock::ScopedLockType sl (lock);
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 < 1) return false;
    cmds[(size_t) (n1 > 0 ? s1 : s2)] = c;
    fifo.finishedWrite (1);
    return true;
}

void LaModuleProcessor::pushSysex (const std::vector<uint8_t>& s)
{
    Cmd c; c.kind = cmdSysex;
    if (s.size() > sizeof (c.data)) return;
    c.len = (int) s.size(); std::memcpy (c.data, s.data(), s.size());
    push (c);
}

void LaModuleProcessor::requestProgram (int part, int program) { Cmd c; c.kind = cmdProgram; c.part = part; c.value = program; push (c); }
void LaModuleProcessor::requestReset() { Cmd c; c.kind = cmdReset; push (c); }
void LaModuleProcessor::requestPanic() { Cmd c; c.kind = cmdPanic; push (c); }

int LaModuleProcessor::loadSyx (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return 0;
    const auto* b = static_cast<const uint8_t*> (mb.getData());
    const int n = (int) mb.getSize();
    int count = 0;
    for (int i = 0; i < n; ++i)
    {
        if (b[i] != 0xF0) continue;
        int j = i + 1; while (j < n && b[j] != 0xF7) ++j;
        if (j >= n) break;
        const uint8_t* d = b + i + 1; const int len = j - i - 1;          // without F0 / F7
        // MT-32 DT1: re-chunk long writes so each fits a command
        if (len >= 8 && d[0] == 0x41 && d[2] == 0x16 && d[3] == 0x12)
        {
            int a = mt::addr (d[4] & 127, d[5] & 127, d[6] & 127);
            const uint8_t* data = d + 7; int dl = len - 8;
            for (int off = 0; off < dl; off += 128)
            { pushSysex (mt::Memory::dt1 (a + off, data + off, juce::jmin (128, dl - off))); ++count; }
        }
        i = j;
    }
    return count;
}

void LaModuleProcessor::parameterChanged (const juce::String& id, float v)
{
    if (applyingFromMemory) return;
    const int value = juce::roundToInt (v);
    if (id == lpid::rvMode)  pushSysex (mt::Memory::systemWrite (1, value));
    if (id == lpid::rvTime)  pushSysex (mt::Memory::systemWrite (2, value));
    if (id == lpid::rvLevel) pushSysex (mt::Memory::systemWrite (3, value));
}

void LaModuleProcessor::timerCallback()
{
    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const float nv = p->convertTo0to1 (plain);
            if (std::abs (p->getValue() - nv) > 1.0e-4f) p->setValueNotifyingHost (nv);
        }
    };
    if (sysSync.exchange (false))
    {
        applyingFromMemory = true;
        set (lpid::rvMode, (float) mem.reverbMode());
        set (lpid::rvTime, (float) mem.reverbTime());
        set (lpid::rvLevel, (float) mem.reverbLevel());
        applyingFromMemory = false;
    }
    if (chanSync.exchange (false))
        for (int p = 0; p < mt::kParts; ++p)
            set (lpid::part (p, "ch"), (float) (chanFromSysex[p] >= 16 ? 0 : chanFromSysex[p] + 1));
}

// ---------------------------------------------------------------------------
//  Audio
// ---------------------------------------------------------------------------
void LaModuleProcessor::applySysex (const uint8_t* d, int n, bool fromMidi)
{
    uint8_t before[mt::kParts]; for (int p = 0; p < mt::kParts; ++p) before[p] = (uint8_t) mem.channelAssign (p);
    if (! mem.sysex (d, n)) return;
    if (active == &munt) munt.sysex (d, n, 0);
    if (mem.systemChanged) { mem.systemChanged = false; sysSync = true; }
    if (fromMidi)
    {
        bool changed = false;
        for (int p = 0; p < mt::kParts; ++p) { chanFromSysex[p] = mem.channelAssign (p); changed |= chanFromSysex[p] != before[p]; }
        if (changed) chanSync = true;
    }
}

void LaModuleProcessor::resyncMunt()
{
    for (auto& s : mem.dump()) munt.sysex (s.data(), (int) s.size(), 0);
    for (int p = 0; p < mt::kParts; ++p)
    {
        auto cc = [&] (int c, int v) { if (v >= 0) { const uint8_t m[3] { 0xB0, (uint8_t) c, (uint8_t) v }; munt.message (p, m, 3, 0); } };
        cc (7, lastVolume[p]); cc (10, lastPan[p]); cc (11, lastExpr[p]);
    }
}

void LaModuleProcessor::handleMidi (const juce::MidiMessage& m)
{
    midiActivity = 1.0f;
    if (m.isSysEx()) { applySysex (m.getRawData(), m.getRawDataSize(), true); return; }
    const int ch = m.getChannel();
    if (ch <= 0) return;
    for (int p = 0; p < mt::kParts; ++p)
    {
        if ((int) pCh[p]->load() != ch) continue;
        const uint8_t* raw = m.getRawData();
        const int len = m.getRawDataSize();
        if (m.isProgramChange())
        {
            if (p < 8) { mem.programChange (p, m.getProgramChangeNumber()); active->message (p, raw, len, 0); }
            continue;
        }
        if (m.isController())
        {
            const int c = m.getControllerNumber(), v = m.getControllerValue();
            if (c == 7) lastVolume[p] = v; else if (c == 10) lastPan[p] = v; else if (c == 11) lastExpr[p] = v;
        }
        if (m.isNoteOn()) activity[p] = 1.0f;
        active->message (p, raw, len, 0);
    }
}

void LaModuleProcessor::renderEngine (int from, int to)
{
    if (to <= from) return;
    PartBuffers b;
    for (int p = 0; p < mt::kParts; ++p)
    { b.dryL[p] = pb (p, 0) + from; b.dryR[p] = pb (p, 1) + from; b.fxL[p] = pb (p, 2) + from; b.fxR[p] = pb (p, 3) + from; }
    active->render (b, to - from);
}

void LaModuleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (partBuf.empty()) return;

    // engine choice (MUNT falls back to COHESION LA when Munt is not available)
    const bool wantMunt = pEngine->load() < 0.5f && munt.isOpen();
    ModuleEngine* e = wantMunt ? static_cast<ModuleEngine*> (&munt) : static_cast<ModuleEngine*> (&laEngine);
    if (e != active) { if (active) active->allSoundOff(); active = e; if (wantMunt) needResync = true; }
    activeIsMunt = wantMunt;
    if (needResync) { if (wantMunt) { munt.allSoundOff(); resyncMunt(); } needResync = false; }

    // commands from the editor
    {
        int s1, n1, s2, n2;
        const int ready = fifo.getNumReady();
        fifo.prepareToRead (ready, s1, n1, s2, n2);
        auto run = [&] (const Cmd& c)
        {
            switch (c.kind)
            {
                case cmdSysex:   applySysex (c.data, c.len, false); break;
                case cmdProgram:
                    if (c.part >= 0 && c.part < 8)
                    {
                        mem.programChange (c.part, c.value);
                        const uint8_t pc[2] { 0xC0, (uint8_t) (c.value & 127) };
                        active->message (c.part, pc, 2, 0);
                    }
                    break;
                case cmdReset:
                    laEngine.allSoundOff(); munt.allSoundOff();
                    mem.init (mem.rom); sysSync = true;
                    if (active == &munt) resyncMunt();
                    break;
                case cmdPanic: laEngine.allSoundOff(); munt.allSoundOff(); break;
                default: break;
            }
        };
        for (int i = 0; i < n1; ++i) run (cmds[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) run (cmds[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
    }

    double bpm = 120.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;

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

        // ---- mixer ----
        float* rvL = busBuf.data(); float* rvR = rvL + blockMax; float* ecL = rvR + blockMax; float* ecR = ecL + blockMax;
        float* wtL = ecR + blockMax; float* wtR = wtL + blockMax;
        juce::FloatVectorOperations::clear (busBuf.data(), 6 * blockMax);
        auto main = getBusBuffer (buffer, false, 0);
        float* mL = main.getWritePointer (0, s0);
        float* mR = main.getNumChannels() > 1 ? main.getWritePointer (1, s0) : mL;

        bool anySolo = false;
        for (int p = 0; p < mt::kParts; ++p) anySolo |= pSolo[p]->load() > 0.5f;
        const bool munted = active == &munt;
        for (int p = 0; p < mt::kParts; ++p)
        {
            const bool mute = pMute[p]->load() > 0.5f;
            const bool solo = pSolo[p]->load() > 0.5f;
            const float g = (mute || (anySolo && ! solo)) ? 0.0f : faderGain (pLvl[p]->load());
            const float pan = pPan[p]->load();
            const float gl = g * (pan > 0.0f ? 1.0f - pan : 1.0f), gr = g * (pan < 0.0f ? 1.0f + pan : 1.0f);
            const float rev = pRev[p]->load();
            const float ech = pEch[p]->load();

            float* oL = mL; float* oR = mR;
            if (pOut[p]->load() > 0.5f)
                if (auto* bus = getBus (false, p + 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() == 2)
                {
                    auto aux = getBusBuffer (buffer, false, p + 1);
                    oL = aux.getWritePointer (0, s0); oR = aux.getWritePointer (1, s0);
                }
            const float* dL = pb (p, 0); const float* dR = pb (p, 1); const float* fL = pb (p, 2); const float* fR = pb (p, 3);
            if (g <= 0.0f) continue;
            for (int i = 0; i < len; ++i)
            {
                const float l = dL[i] * gl, r = dR[i] * gr;
                oL[i] += l; oR[i] += r;
                ecL[i] += l * ech; ecR[i] += r * ech;
            }
            float* tL = munted ? wtL : rvL; float* tR = munted ? wtR : rvR;
            for (int i = 0; i < len; ++i) { tL[i] += fL[i] * gl * rev; tR[i] += fR[i] * gr * rev; }
        }

        // ---- own MT-32 style reverb (the Munt engine brings its own) ----
        if (! munted)
        {
            const int mode = mem.reverbMode(), time = mem.reverbTime(), level = mem.reverbLevel();
            fx::FxParams fp;
            if (mode == 3)
            {
                fp.ecTime = 60.0f + time * 45.0f; fp.ecSync = 0; fp.ecFb = 0.45f; fp.ecDamp = 0.35f; fp.ecWidth = 1.0f; fp.ecPing = true; fp.ecMix = 1.0f;
                tapDelay.process (rvL, rvR, len, fp);
            }
            else
            {
                static const float size[3] = { 0.3f, 0.75f, 0.5f }, damp[3] = { 0.45f, 0.35f, 0.1f }, base[3] = { 0.22f, 0.32f, 0.28f };
                fp.rvSize = size[mode]; fp.rvDamp = damp[mode]; fp.rvDecay = base[mode] + time * 0.05f;
                fp.rvPre = mode == 1 ? 18.0f : 6.0f; fp.rvWidth = 1.0f; fp.rvLowCut = 90.0f; fp.rvMix = 1.0f;
                reverb.process (rvL, rvR, len, fp);
            }
            const float lv = std::pow (level / 7.0f, 1.5f);
            for (int i = 0; i < len; ++i) { wtL[i] += rvL[i] * lv; wtR[i] += rvR[i] * lv; }
        }

        // ---- echo send ----
        {
            fx::FxParams fp;
            const bool sync = pEcSync->load() > 0.5f;
            fp.ecSync = sync ? (int) pEcDiv->load() + 1 : 0;
            fp.ecTime = pEcTime->load();
            fp.ecFb = pEcFb->load();
            fp.ecDamp = 0.3f; fp.ecWidth = 0.8f; fp.ecPing = true; fp.ecMix = 1.0f; fp.bpm = bpm;
            echo.process (ecL, ecR, len, fp);
            const float el = pEcLevel->load();
            // on / off with a 10 ms fade (the effects keep running, so switching on brings back a natural tail)
            const float rvT = pRvOn->load() > 0.5f ? 1.0f : 0.0f, ecT = pEcOn->load() > 0.5f ? 1.0f : 0.0f;
            const float step = 1.0f / (float) (0.01 * sr);
            for (int i = 0; i < len; ++i)
            {
                rvGain += juce::jlimit (-step, step, rvT - rvGain);
                ecGain += juce::jlimit (-step, step, ecT - ecGain);
                mL[i] += wtL[i] * rvGain + ecL[i] * el * ecGain;
                mR[i] += wtR[i] * rvGain + ecR[i] * el * ecGain;
            }
        }
    }

    // master on every enabled output
    const float master = pMaster->load();
    buffer.applyGain (master);
    partials = active->activePartials();
}

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------
void LaModuleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("rom", romLabel, nullptr);
    state.setProperty ("mem", mem.toBase64(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void LaModuleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);
    const auto label = state.getProperty ("rom").toString();
    const auto memory = state.getProperty ("mem").toString();
    if (label.isNotEmpty() && setRom (label, memory.isNotEmpty()))
    {
        const juce::ScopedLock sl (getCallbackLock());
        if (memory.isNotEmpty() && ! mem.fromBase64 (memory)) mem.init (mem.rom);
        needResync = true;
    }
}

juce::AudioProcessorEditor* LaModuleProcessor::createEditor() { return new LaModuleEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LaModuleProcessor(); }
