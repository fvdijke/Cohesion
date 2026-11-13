#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <map>
#include <mutex>
#include "RomLibrary.h"

CohesionSynthAudioProcessor::CohesionSynthAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", cohesionSynthLayout())
{
    synth.addSound (new SynthSound());
    synth.snap = &snapshot;
    for (int i = 0; i < 16; ++i)
    {
        auto* v = new SynthVoice();
        v->setSnapshot (&snapshot);
        v->setGlideSource (&lastNoteHz);
        v->setFmSource (&fmSource);
        v->setLaSource (&laSource);
        synth.addVoice (v);
    }
    syn::wavetables();   // build the wavetables now, never on the audio thread
    presets = cohesionSynthPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        if (gm::isGmCategory (presets[(size_t) i].category)) { gmFirst = i; break; }
    setFmPatch (dx7::initVoice(), {});
    loadUserBanks();
    loadUserRoms();
    loadUserPresets();
    if (! apvts.state.hasProperty ("fxOrder"))
        apvts.state.setProperty ("fxOrder", "0,1,2,3,4", nullptr);
    // record the parameter read sequences once (the audio thread then reads without lookups)
    updateSnapshot(); updateFxParams(); updateArpParams();
}

// ---------------------------------------------------------------------------
//  FX order (kept as a property of the plugin state, not as a parameter).
// ---------------------------------------------------------------------------
static bool parseFxOrder (const juce::String& s, int* out)
{
    auto toks = juce::StringArray::fromTokens (s, ",", "");
    if (toks.size() != fx::kNumFx) return false;
    bool seen[fx::kNumFx] {};
    for (int k = 0; k < fx::kNumFx; ++k)
    {
        const int v = toks[k].trim().getIntValue();
        if (v < 0 || v >= fx::kNumFx || seen[v]) return false;
        seen[v] = true; out[k] = v;
    }
    return true;
}

void CohesionSynthAudioProcessor::getFxOrder (int* out5) const
{
    for (int k = 0; k < fx::kNumFx; ++k) out5[k] = fxOrder[k].load();
}

void CohesionSynthAudioProcessor::setFxOrder (const int* in5)
{
    juce::StringArray s;
    for (int k = 0; k < fx::kNumFx; ++k) s.add (juce::String (in5[k]));
    int chk[fx::kNumFx];
    if (! parseFxOrder (s.joinIntoString (","), chk)) return;
    for (int k = 0; k < fx::kNumFx; ++k) fxOrder[k].store (chk[k]);
    apvts.state.setProperty ("fxOrder", s.joinIntoString (","), nullptr);
}

void CohesionSynthAudioProcessor::syncFxOrderFromState()
{
    int o[fx::kNumFx] = { 0, 1, 2, 3, 4 };
    if (! parseFxOrder (apvts.state.getProperty ("fxOrder").toString(), o))
        for (int k = 0; k < fx::kNumFx; ++k) o[k] = k;
    for (int k = 0; k < fx::kNumFx; ++k) fxOrder[k].store (o[k]);
}

void CohesionSynthAudioProcessor::updateArpParams()
{
    auto rd = arpSeq.reader (apvts);
    auto f  = [&rd] (juce::StringRef id) { return rd.f (id); };
    auto ip = [&rd] (juce::StringRef id) { return rd.i (id); };
    auto bp = [&rd] (juce::StringRef id) { return rd.b (id); };
    arpp.on = bp (pid::arpOn); arpp.mode = ip (pid::arpMode); arpp.rate = ip (pid::arpRate); arpp.octaves = ip (pid::arpOct);
    arpp.gate = f (pid::arpGate); arpp.swing = f (pid::arpSwing); arpp.random = f (pid::arpRandom);
    arpp.sync = bp (pid::arpSync); arpp.hold = bp (pid::arpHold); arpp.latch = bp (pid::arpLatch);
    arpp.tempo = f (pid::arpTempo); arpp.steps = ip (pid::arpSteps); arpp.order = ip (pid::arpOrder);
    arpp.velAmt = f (pid::arpVelAmt); arpp.slide = f (pid::arpSlide); arpp.humanize = f (pid::arpHuman);
    for (int i = 0; i < arp::kSteps; ++i) { arpp.stepOn[i] = bp (pid::arpStepOn (i)); arpp.stepVel[i] = f (pid::arpStepVel (i)); }
}

void CohesionSynthAudioProcessor::updateFxParams()
{
    auto rd = fxSeq.reader (apvts);
    auto f  = [&rd] (juce::StringRef id) { return rd.f (id); };
    auto ip = [&rd] (juce::StringRef id) { return rd.i (id); };
    auto bp = [&rd] (juce::StringRef id) { return rd.b (id); };
    fxp.compOn = bp (pid::compOn); fxp.compThresh = f (pid::compThresh); fxp.compRatio = f (pid::compRatio);
    fxp.compAtk = f (pid::compAtk); fxp.compRel = f (pid::compRel); fxp.compMakeup = f (pid::compMakeup);
    fxp.compKnee = f (pid::compKnee); fxp.compMix = f (pid::compMix);
    fxp.flOn = bp (pid::flOn); fxp.flRate = f (pid::flRate); fxp.flDepth = f (pid::flDepth); fxp.flFb = f (pid::flFb);
    fxp.flDelay = f (pid::flDelay); fxp.flWidth = f (pid::flWidth); fxp.flSync = ip (pid::flSync); fxp.flMix = f (pid::flMix);
    fxp.phOn = bp (pid::phOn); fxp.phRate = f (pid::phRate); fxp.phDepth = f (pid::phDepth); fxp.phFb = f (pid::phFb);
    fxp.phStages = ip (pid::phStages); fxp.phSpread = f (pid::phSpread); fxp.phSync = ip (pid::phSync); fxp.phMix = f (pid::phMix);
    fxp.ecOn = bp (pid::ecOn); fxp.ecTime = f (pid::ecTime); fxp.ecSync = ip (pid::ecSync); fxp.ecFb = f (pid::ecFb);
    fxp.ecDamp = f (pid::ecDamp); fxp.ecWidth = f (pid::ecWidth); fxp.ecPing = bp (pid::ecPing); fxp.ecMix = f (pid::ecMix);
    fxp.rvOn = bp (pid::rvOn); fxp.rvSize = f (pid::rvSize); fxp.rvDecay = f (pid::rvDecay); fxp.rvPre = f (pid::rvPre);
    fxp.rvDamp = f (pid::rvDamp); fxp.rvWidth = f (pid::rvWidth); fxp.rvLowCut = f (pid::rvLowCut); fxp.rvMix = f (pid::rvMix);
    fxp.limOn = bp (pid::limOn); fxp.limCeil = f (pid::limCeil); fxp.limRel = f (pid::limRel); fxp.limDrive = f (pid::limDrive);
}

// A stored parameter value. JUCE leaves "value" empty for a parameter that never changed since
// its tree node was made (e.g. a project from a version without that parameter): that means
// "default", not 0.
static bool storedValue (const juce::ValueTree& child, float& out)
{
    if (! child.isValid() || child.getProperty ("value").toString().trim().isEmpty()) return false;
    out = (float) (double) child.getProperty ("value");
    return true;
}

juce::String CohesionSynthAudioProcessor::presetDisplayName (int i) const
{
    if (i < 0 || i >= (int) presets.size()) return {};
    return presets[(size_t) i].category + juce::String::fromUTF8 ("  \xe2\x80\x94  ") + presets[(size_t) i].name;
}

void CohesionSynthAudioProcessor::loadPreset (int i)
{
    if (presets.empty()) return;
    const int n = (int) presets.size();
    presetIndex = ((i % n) + n) % n;
    const auto& pr = presets[(size_t) presetIndex];
    beginPresetSwitch();
    if (pr.fm != nullptr)
    {
        applySynthPreset (*this, apvts, dxInitPreset());
        setFmPatch (*pr.fm, pr.category);
    }
    else if (pr.la != nullptr)
    {
        applySynthPreset (*this, apvts, laInitPreset());
        const auto& list = pr.laRhythm ? pr.la->rhythm : pr.la->melodic;
        if (pr.laIndex >= 0 && pr.laIndex < (int) list.size()) setLaTimbre (pr.la, list[(size_t) pr.laIndex], pr.category);
    }
    else if (pr.userFile != juce::File())
    {
        if (! applyUserPreset (pr.userFile)) applySynthPreset (*this, apvts, presets.front());   // unreadable: Init
    }
    else applySynthPreset (*this, apvts, pr);

    // imported DX7 / LA voices: match the factory presets' loudness (measured once per voice)
    if (pr.fm != nullptr || pr.la != nullptr)
    {
        const auto lv = importedLevel (presetIndex);
        setParam (pid::mainVol, lv.first);
        setParam (pid::limDrive, lv.second);
    }
    // a whole preset: one sound (a User preset brings its own part names and multi flag)
    if (pr.userFile == juce::File())
    {
        for (int k = 0; k < pid::kNumParts; ++k) setPartSoundName (k, {});
        setPartSoundName (pr.fm != nullptr ? pid::kFM : pr.la != nullptr ? pid::kLA : pid::kVA, presetDisplayName (presetIndex));
        apvts.state.setProperty ("multi", false, nullptr);
    }
    markCurrentPreset();
    endPresetSwitch();
}

void CohesionSynthAudioProcessor::markCurrentPreset()
{
    apvts.state.setProperty ("presetName", presetDisplayName (presetIndex), nullptr);
}

void CohesionSynthAudioProcessor::setParam (const juce::String& id, float value)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

std::pair<float, float> CohesionSynthAudioProcessor::importedLevel (int i)
{
    auto it = importedLevels.find (i);
    if (it == importedLevels.end())
    {
        const auto& pr = presets[(size_t) i];
        it = importedLevels.emplace (i, measureImportedLevel (pr.la != nullptr && pr.laRhythm,
                                                              pr.fm != nullptr ? pid::kFM : pid::kLA)).first;
    }
    return it->second;
}

// ---------------------------------------------------------------------------
//  Multi sounds: one part of a preset at a time
// ---------------------------------------------------------------------------
static juce::String partNameProp (int part) { return "partName" + juce::String (part + 1); }

void CohesionSynthAudioProcessor::setPartSoundName (int part, const juce::String& name)
{
    apvts.state.setProperty (partNameProp (part), name, nullptr);
}

juce::String CohesionSynthAudioProcessor::partSoundName (int part) const
{
    return apvts.state.getProperty (partNameProp (part)).toString();
}

juce::String CohesionSynthAudioProcessor::headerName() const
{
    if (! isMulti())
    {   // the stored name: survives reopening the project and deleting / renaming the preset file
        const auto stored = apvts.state.getProperty ("presetName").toString();
        return stored.isNotEmpty() ? stored : presetDisplayName (presetIndex);
    }
    juce::StringArray s;
    auto on = [this] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };
    const bool active[3] = { on (pid::vaOn) && (on (pid::oscOn (1).toRawUTF8()) || on (pid::oscOn (2).toRawUTF8()) || on (pid::oscOn (3).toRawUTF8())
                             || on (pid::oscOn (4).toRawUTF8()) || on (pid::subOn) || on (pid::noiseOn)), on (pid::dxOn), on (pid::laOn) };
    for (int k = 0; k < pid::kNumParts; ++k)
    {
        auto nm = partSoundName (k).fromLastOccurrenceOf (juce::String::fromUTF8 ("\xe2\x80\x94  "), false, false);
        if (active[k] && nm.isNotEmpty()) s.add (pid::partNames[k] + ": " + nm);
    }
    return "Multi" + (s.isEmpty() ? juce::String() : juce::String::fromUTF8 ("  \xe2\x80\x94  ") + s.joinIntoString ("  |  "));
}

bool CohesionSynthAudioProcessor::presetFitsPart (int i, int part) const
{
    if (i < 0 || i >= (int) presets.size()) return false;
    const auto& pr = presets[(size_t) i];
    if (pr.userFile != juce::File()) return (pr.userParts >> part) & 1;   // only the parts that sound in it
    switch (part)
    {
        case pid::kFM: return pr.fm != nullptr;
        case pid::kLA: return pr.la != nullptr;
        default:       return pr.fm == nullptr && pr.la == nullptr;
    }
}

void CohesionSynthAudioProcessor::loadPresetIntoPart (int i, int part)
{
    if (! presetFitsPart (i, part)) return;
    const auto& pr = presets[(size_t) i];

    // the part's parameters: all to their default, then the preset's values
    std::unordered_map<juce::String, float> values;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (pid::partOfParam (rp->getParameterID()) == part)
                values[rp->getParameterID()] = rp->convertFrom0to1 (rp->getDefaultValue());
    auto take = [&values] (const SynthPreset& p) { for (auto& kv : p.v) if (values.count (kv.first)) values[kv.first] = kv.second; };

    float levelDb = 0.0f;   // the part's MIXER level that gives the preset's matched loudness (at MAIN 0.8)
    auto levelFrom = [] (float main, float drive)
    { return juce::jlimit (-60.0f, 18.0f, juce::Decibels::gainToDecibels (juce::jmax (1.0e-4f, main) / 0.8f) + drive); };

    std::unique_ptr<juce::XmlElement> userXml;
    juce::ValueTree userTree;
    if (pr.userFile != juce::File())
    {
        userXml = juce::XmlDocument::parse (pr.userFile);
        const auto* st = userXml != nullptr ? userXml->getChildByName (apvts.state.getType()) : nullptr;
        if (st == nullptr) return;
        userTree = juce::ValueTree::fromXml (*st);
        for (auto& kv : values)
        {
            const auto c = userTree.getChildWithProperty ("id", kv.first);
            storedValue (c, kv.second);
        }
        const auto lv = userTree.getChildWithProperty ("id", pid::partLevel (part));
        storedValue (lv, levelDb);
    }
    else if (part == pid::kFM) take (dxInitPreset());
    else if (part == pid::kLA) take (laInitPreset());
    else
    {
        take (pr);
        float main = 0.8f, drive = 0.0f;
        for (auto& kv : pr.v) { if (kv.first == pid::mainVol) main = kv.second; if (kv.first == pid::limDrive) drive = kv.second; }
        levelDb = levelFrom (main, drive);
    }

    beginPresetSwitch();
    for (auto& kv : values) setParam (kv.first, kv.second);
    if (pr.userFile != juce::File())
    {
        if (part == pid::kFM) for (const char* prop : { "dxVoice", "dxBank" })
            apvts.state.setProperty (prop, userTree.getProperty (prop), nullptr);
        if (part == pid::kLA) for (const char* prop : { "laTimbre", "laRom", "laBank" })
            apvts.state.setProperty (prop, userTree.getProperty (prop), nullptr);
        if (part == pid::kFM) restoreFmFromState();
        if (part == pid::kLA) restoreLaFromState();
    }
    else if (part == pid::kFM) setFmPatch (*pr.fm, pr.category);
    else if (part == pid::kLA)
    {
        const auto& list = pr.laRhythm ? pr.la->rhythm : pr.la->melodic;
        if (pr.laIndex >= 0 && pr.laIndex < (int) list.size()) setLaTimbre (pr.la, list[(size_t) pr.laIndex], pr.category);
    }
    if (pr.fm != nullptr || pr.la != nullptr)
    {
        const auto lv = importedLevel (i);
        levelDb = levelFrom (lv.first, lv.second);
    }
    setParam (pid::partLevel (part), levelDb);
    setPartSoundName (part, presetDisplayName (i));
    apvts.state.setProperty ("multi", true, nullptr);
    endPresetSwitch();
}

// Renders the current imported voice (as set up by loadPreset) offline, the same way
// tests/PresetRender measures the factory presets: a C-major chord (or one drum hit) at
// velocity 0.8, momentary-max loudness of 300 ms (50 ms for drums) windows, target -18 dB
// (-14 dB for drums). Returns the MAIN level and limiter DRIVE that reach the target.
// onlyPart >= 0: only that part sounds (the others of a multi sound are switched off).
std::pair<float, float> CohesionSynthAudioProcessor::measureImportedLevel (bool drum, int onlyPart)
{
    constexpr double sr = 48000.0;
    syn::Snapshot s;
    fillSnapshot (s);
    s.arpOn = false;
    for (auto& pp : s.part) { pp.keyLo = 0; pp.keyHi = 127; }
    if (onlyPart >= 0)
    {
        if (onlyPart != pid::kVA) { for (auto& o : s.osc) o.on = false; s.subOn = s.noiseOn = false; }
        if (onlyPart != pid::kFM) s.dxOn = false;
        if (onlyPart != pid::kLA) s.laOn = false;
    }
    juce::Synthesiser sy;
    for (int k = 0; k < 4; ++k)
    {
        auto* v = new SynthVoice();
        v->setSnapshot (&s); v->setFmSource (&fmSource); v->setLaSource (&laSource);
        sy.addVoice (v);
    }
    sy.addSound (new SynthSound());
    sy.setCurrentPlaybackSampleRate (sr);

    const std::vector<int> notes = drum ? std::vector<int> { 48 } : std::vector<int> { 48, 52, 55 };
    const double hold = drum ? 0.3 : 1.5;
    const int holdS = (int) (hold * sr), total = holdS + (int) (0.3 * sr), bs = 512;
    juce::AudioBuffer<float> b (2, bs);
    std::vector<float> mono; mono.reserve ((size_t) total + bs);
    for (int pos = 0; pos < total; pos += bs)
    {
        juce::MidiBuffer m;
        if (pos == 0) for (int nn : notes) m.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0);
        if (pos <= holdS && holdS < pos + bs) for (int nn : notes) m.addEvent (juce::MidiMessage::noteOff (1, nn), holdS - pos);
        b.clear();
        sy.renderNextBlock (b, m, 0, bs);
        for (int i = 0; i < bs; ++i) mono.push_back (0.5f * (b.getSample (0, i) + b.getSample (1, i)));
    }
    const int win = (int) ((drum ? 0.05 : 0.3) * sr);
    float loud = 0.0f;
    for (int st = 0; st + win <= total; st += (int) (0.025 * sr))
    {
        double e = 0; for (int k = 0; k < win; ++k) e += (double) mono[(size_t) (st + k)] * mono[(size_t) (st + k)];
        loud = juce::jmax (loud, (float) std::sqrt (e / win));
    }
    const float loudDb = juce::Decibels::gainToDecibels (loud, -100.0f);
    if (loudDb < -80.0f || ! std::isfinite (loudDb)) return { 0.8f, 0.0f };   // silent / odd voice: leave the default
    const float corr = (drum ? -14.0f : -18.0f) - loudDb;                      // measured at MAIN 1, DRIVE 0
    if (corr <= 0.0f) return { juce::jmax (0.05f, juce::Decibels::decibelsToGain (corr)), 0.0f };
    return { 1.0f, juce::jmin (18.0f, corr) };
}

void CohesionSynthAudioProcessor::beginPresetSwitch()
{
    const bool audioRunning = juce::Time::getMillisecondCounter() - lastBlockMs.load() < 250;
    if (! audioRunning) { synth.allNotesOff (0, false); return; }
    switchState.store (kFadeOut);
    for (int t = 0; t < 100 && switchState.load() != kSilent; ++t)   // a fade takes ~5 ms + one block
        juce::Thread::sleep (1);
}

void CohesionSynthAudioProcessor::handleAsyncUpdate()
{
    const int prog = pendingProgram.exchange (-1);
    if (prog < 0 || gmFirst < 0 || gmFirst + prog >= (int) presets.size()) return;
    loadPreset (gmFirst + prog);
    if (onPresetChanged) onPresetChanged();
}

void CohesionSynthAudioProcessor::hardStopVoices()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i))) v->hardStop();
}

void CohesionSynthAudioProcessor::endPresetSwitch()
{
    int s = switchState.load();
    if (s == kFadeOut || s == kSilent) switchState.compare_exchange_strong (s, kFadeIn);
}

void CohesionSynthAudioProcessor::applySwitchFade (juce::AudioBuffer<float>& buffer, int n)
{
    const int st = switchState.load();
    if (st == kRun && switchGain >= 1.0f) return;
    const float step = (float) (1.0 / (0.005 * sampleRateHz));   // 5 ms
    const int nCh = buffer.getNumChannels();
    for (int i = 0; i < n; ++i)
    {
        switchGain = (st == kFadeOut) ? juce::jmax (0.0f, switchGain - step) : juce::jmin (1.0f, switchGain + step);
        for (int c = 0; c < nCh; ++c) buffer.getWritePointer (c)[i] *= switchGain;
    }
    if (st == kFadeOut && switchGain <= 0.0f)
    {
        synth.allNotesOff (0, false);
        hardStopVoices();
        fxChain.resetAll();   // FX tails, flanger lines, the limiter's lookahead
        int s = kFadeOut; switchState.compare_exchange_strong (s, kSilent);
    }
    else if (st == kFadeIn && switchGain >= 1.0f)
    {
        int s = kFadeIn; switchState.compare_exchange_strong (s, kRun);
    }
}

// ---------------------------------------------------------------------------
//  FM voice: shared with the audio thread under a spin lock, stored in the state.
// ---------------------------------------------------------------------------
void CohesionSynthAudioProcessor::setFmPatch (const dx7::Patch& p, const juce::String& bank)
{
    { const juce::SpinLock::ScopedLockType sl (fmSource.lock); fmSource.patch = p; }
    fmBank = bank;
    const auto d = p.toVced();
    apvts.state.setProperty ("dxVoice", juce::String::toHexString (d.data(), (int) d.size(), 0), nullptr);
    apvts.state.setProperty ("dxBank", bank, nullptr);
    ++fmVersion;
}

dx7::Patch CohesionSynthAudioProcessor::getFmPatch()
{
    const juce::SpinLock::ScopedLockType sl (fmSource.lock);
    return fmSource.patch;
}

void CohesionSynthAudioProcessor::restoreFmFromState()
{
    const auto hex = apvts.state.getProperty ("dxVoice").toString();
    if (hex.length() != 310) return;
    juce::MemoryBlock mb; mb.loadFromHexString (hex);
    if (mb.getSize() == 155) setFmPatch (dx7::fromVced (static_cast<const uint8_t*> (mb.getData())), apvts.state.getProperty ("dxBank").toString());
}

// ---------------------------------------------------------------------------
//  LA engine: current timbre (shared with the audio thread) + ROM sets
// ---------------------------------------------------------------------------
void CohesionSynthAudioProcessor::setLaTimbre (std::shared_ptr<const la32::Rom> rom, const la32::Timbre& t, const juce::String& bank)
{
    { const juce::SpinLock::ScopedLockType sl (laSource.lock); laSource.rom = rom; laSource.timbre = t; }
    laBank = bank;
    juce::MemoryBlock mb; mb.append (t.common, 14); for (int k = 0; k < 4; ++k) mb.append (t.partial[k], 58);
    apvts.state.setProperty ("laTimbre", juce::String::toHexString (mb.getData(), (int) mb.getSize(), 0), nullptr);
    apvts.state.setProperty ("laRom", rom != nullptr ? rom->label : juce::String(), nullptr);
    apvts.state.setProperty ("laBank", bank, nullptr);
    ++laVersion;
}

std::pair<std::shared_ptr<const la32::Rom>, la32::Timbre> CohesionSynthAudioProcessor::getLaTimbre()
{
    const juce::SpinLock::ScopedLockType sl (laSource.lock);
    return { laSource.rom, laSource.timbre };
}

void CohesionSynthAudioProcessor::restoreLaFromState()
{
    const auto hex = apvts.state.getProperty ("laTimbre").toString();
    if (hex.length() != 246 * 2) return;
    juce::MemoryBlock mb; mb.loadFromHexString (hex);
    if (mb.getSize() != 246) return;
    la32::Timbre t;
    auto* d = static_cast<const uint8_t*> (mb.getData());
    std::memcpy (t.common, d, 14); std::memcpy (t.name, d, 10);
    for (int k = 0; k < 4; ++k) std::memcpy (t.partial[k], d + 14 + 58 * k, 58);
    const auto label = apvts.state.getProperty ("laRom").toString();
    std::shared_ptr<const la32::Rom> rom;
    for (auto& r : romSets) if (r->label == label) rom = r;
    if (rom == nullptr && ! romSets.empty()) rom = romSets.front();   // ROM set no longer present: closest fallback
    setLaTimbre (rom, t, apvts.state.getProperty ("laBank").toString());
}

juce::File CohesionSynthAudioProcessor::userRomDir() { return romlib::userRomDir(); }

bool CohesionSynthAudioProcessor::addRomSet (std::shared_ptr<const la32::Rom> rom, RomImportResult* r)
{
    for (auto& s : romSets) if (s->contentHash == rom->contentHash) { if (r) ++r->duplicates; return false; }
    romSets.push_back (rom);
    const juce::String dot = juce::String::fromUTF8 (" \xc2\xb7 ");
    for (int i = 0; i < (int) rom->melodic.size(); ++i)
    {
        SynthPreset p { rom->melodic[(size_t) i].getName(), rom->label + dot + (i < 64 ? "A" : "B"), {}, nullptr };
        p.la = rom; p.laIndex = i; p.laRhythm = false;
        presets.push_back (std::move (p));
    }
    for (int i = 0; i < (int) rom->rhythm.size(); ++i)
    {
        SynthPreset p { rom->rhythm[(size_t) i].getName(), rom->label + dot + "Rhythm", {}, nullptr };
        p.la = rom; p.laIndex = i; p.laRhythm = true;
        presets.push_back (std::move (p));
    }
    if (r) { ++r->sets; if (r->firstCategory.isEmpty()) r->firstCategory = rom->label + dot + "A"; }
    return true;
}

void CohesionSynthAudioProcessor::loadUserRoms()
{
    for (auto& rom : romlib::storedSets()) addRomSet (rom, nullptr);
}

CohesionSynthAudioProcessor::RomImportResult CohesionSynthAudioProcessor::importRoms (const juce::Array<juce::File>& items)
{
    std::vector<uint64_t> known;
    for (auto& s : romSets) known.push_back (s->contentHash);
    auto res = romlib::importRoms (items, known);
    RomImportResult r;
    r.duplicates = res.duplicates; r.files = res.files; r.failed = res.failed;
    for (auto& rom : res.added) addRomSet (rom, &r);
    return r;
}

// ---------------------------------------------------------------------------
//  SysEx banks
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  User presets: one .cspreset file (XML) per preset, holding the plugin state without
//  the window size. Loading sets every parameter (missing ones to their default) and
//  restores the FX order, DX7 voice, LA timbre and ARP pattern.
// ---------------------------------------------------------------------------
static constexpr const char* kPresetExt = ".cspreset";
static constexpr const char* kPresetTag = "CohesionSynthPreset";

juce::File CohesionSynthAudioProcessor::userPresetDir()
{
    // tests redirect the folder; normal use: %APPDATA%/Pepperwood/Cohesion Synth/Presets
    const auto overrideDir = juce::SystemStats::getEnvironmentVariable ("COHESION_SYNTH_PRESET_DIR", {});
    if (overrideDir.isNotEmpty()) return juce::File (overrideDir);
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Pepperwood").getChildFile ("Cohesion Synth").getChildFile ("Presets");
}

static juce::String legalPresetName (const juce::String& raw)
{
    return juce::File::createLegalFileName (raw.trim()).trim().substring (0, 64);
}

// Which parts sound in a User preset file: VA = an oscillator, SUB or NOISE on; FM = FM engine
// on; LA = LA engine on (bit k = pid::Part k). Only those parts can be loaded from it alone.
// the state with every parameter's current value written out (none left empty)
juce::ValueTree CohesionSynthAudioProcessor::completeState()
{
    auto st = apvts.copyState();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            auto c = st.getChildWithProperty ("id", rp->getParameterID());
            if (! c.isValid()) { c = juce::ValueTree ("PARAM"); c.setProperty ("id", rp->getParameterID(), nullptr); st.appendChild (c, nullptr); }
            c.setProperty ("value", rp->convertFrom0to1 (rp->getValue()), nullptr);
        }
    return st;
}

static int userPresetParts (const juce::File& f)
{
    const auto xml = juce::XmlDocument::parse (f);
    const auto* st = xml != nullptr ? xml->getChildByName ("PARAMS") : nullptr;
    if (st == nullptr) return 0;
    auto on = [st] (const juce::String& id, bool dflt = false)
    {
        for (auto* c : st->getChildIterator())
            if (c->getStringAttribute ("id") == id)
                return c->getStringAttribute ("value").trim().isNotEmpty() ? c->getDoubleAttribute ("value") > 0.5 : dflt;
        return dflt;
    };
    const bool va = on (pid::vaOn, true) && (on (pid::oscOn (1)) || on (pid::oscOn (2)) || on (pid::oscOn (3)) || on (pid::oscOn (4))
                    || on (pid::subOn) || on (pid::noiseOn));
    return (va ? 1 << pid::kVA : 0) | (on (pid::dxOn) ? 1 << pid::kFM : 0) | (on (pid::laOn) ? 1 << pid::kLA : 0);
}

bool CohesionSynthAudioProcessor::userPresetExists (const juce::String& name) const
{
    const auto n = legalPresetName (name);
    return n.isNotEmpty() && userPresetDir().getChildFile (n + kPresetExt).existsAsFile();
}

void CohesionSynthAudioProcessor::loadUserPresets()
{
    auto key = [] (const SynthPreset& p) { return p.category + "|" + p.name; };
    const juce::String current = (presetIndex >= 0 && presetIndex < (int) presets.size()) ? key (presets[(size_t) presetIndex]) : juce::String();

    presets.erase (std::remove_if (presets.begin(), presets.end(), [] (const SynthPreset& p) { return p.userFile != juce::File(); }),
                   presets.end());
    const auto dir = userPresetDir();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kPresetExt);
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
        for (auto& f : files)
        {
            SynthPreset p { f.getFileNameWithoutExtension(), kUserCategory, {}, nullptr };
            p.userFile = f;
            p.userParts = userPresetParts (f);
            presets.push_back (std::move (p));
        }
    }
    importedLevels.clear();   // indices after the user presets may have shifted
    presetIndex = 0;
    for (int i = 0; i < (int) presets.size(); ++i)
        if (key (presets[(size_t) i]) == current) { presetIndex = i; break; }
}

bool CohesionSynthAudioProcessor::applyUserPreset (const juce::File& f)
{
    const auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (kPresetTag)) return false;
    const auto* st = xml->getChildByName (apvts.state.getType());
    if (st == nullptr) return false;
    const auto tree = juce::ValueTree::fromXml (*st);
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto child = tree.getChildWithProperty ("id", rp->getParameterID());
            float v = 0.0f;
            rp->setValueNotifyingHost (storedValue (child, v) ? rp->convertTo0to1 (v) : rp->getDefaultValue());
        }
    for (const char* prop : { "fxOrder", "dxVoice", "dxBank", "laTimbre", "laRom", "laBank", "arpPattern",
                              "partName1", "partName2", "partName3", "multi" })
    {
        if (tree.hasProperty (prop)) apvts.state.setProperty (prop, tree.getProperty (prop), nullptr);
        else                         apvts.state.removeProperty (prop, nullptr);
    }
    syncFxOrderFromState();
    restoreFmFromState();
    restoreLaFromState();
    apvts.state.setProperty ("multi", false, nullptr);   // the header shows the User preset's name
    return true;
}

int CohesionSynthAudioProcessor::saveUserPreset (const juce::String& rawName, bool overwrite)
{
    const auto name = legalPresetName (rawName);
    if (name.isEmpty()) return kSaveBadName;
    const auto dir = userPresetDir();
    if (! dir.createDirectory()) return kSaveFailed;
    const auto f = dir.getChildFile (name + kPresetExt);
    if (f.existsAsFile() && ! overwrite) return kSaveExists;

    auto st = completeState();
    st.removeProperty ("uiScale", nullptr);   // the window size belongs to the project, not the sound
    juce::XmlElement root (kPresetTag);
    root.setAttribute ("name", name);
    root.setAttribute ("version", 1);
    root.addChildElement (st.createXml().release());
    if (! root.writeTo (f)) return kSaveFailed;

    loadUserPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].userFile == f)
        {
            presetIndex = i;
            apvts.state.setProperty ("multi", false, nullptr);   // the header now shows the User preset
            markCurrentPreset();
            return i;
        }
    return kSaveFailed;
}

int CohesionSynthAudioProcessor::renameUserPreset (int i, const juce::String& newName)
{
    if (! presetIsUser (i)) return kSaveFailed;
    const auto name = legalPresetName (newName);
    if (name.isEmpty()) return kSaveBadName;
    const auto from = presets[(size_t) i].userFile;
    const auto to = from.getSiblingFile (name + kPresetExt);
    const bool sameFile = to.getFullPathName().equalsIgnoreCase (from.getFullPathName());
    if (! sameFile && to.existsAsFile()) return kSaveExists;
    if (sameFile)
    {
        if (to.getFullPathName() == from.getFullPathName()) return i;
        // only the case changes: File::moveFileTo would delete the "other" (= the same) file first
        const auto hop = from.getNonexistentSibling (false).withFileExtension ("tmp");
        if (! from.moveFileTo (hop) || ! hop.moveFileTo (to)) return kSaveFailed;
    }
    else if (! from.moveFileTo (to)) return kSaveFailed;
    const bool wasCurrent = (i == presetIndex);
    loadUserPresets();
    for (int k = 0; k < (int) presets.size(); ++k)
        if (presets[(size_t) k].userFile == to) { if (wasCurrent) { presetIndex = k; markCurrentPreset(); } return k; }
    return kSaveFailed;
}

bool CohesionSynthAudioProcessor::deleteUserPreset (int i)
{
    if (! presetIsUser (i)) return false;
    const auto f = presets[(size_t) i].userFile;
    if (! f.moveToTrash() && ! f.deleteFile()) return false;
    loadUserPresets();
    return true;
}

juce::File CohesionSynthAudioProcessor::userSysExDir()
{
    // tests redirect the folder; normal use: %APPDATA%/Pepperwood/Cohesion Synth/SysEx
    const auto overrideDir = juce::SystemStats::getEnvironmentVariable ("COHESION_SYNTH_SYSEX_DIR", {});
    if (overrideDir.isNotEmpty()) return juce::File (overrideDir);
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Pepperwood").getChildFile ("Cohesion Synth").getChildFile ("SysEx");
}

static juce::String sysExCategory (const juce::File& f, const juce::File& dir)
{
    auto rel = f.getRelativePathFrom (dir).replaceCharacter ('\\', '/');
    if (rel.startsWith ("..")) rel = f.getFileName();
    return juce::String::fromUTF8 ("DX7 \xc2\xb7 ") + rel.upToLastOccurrenceOf (".", false, false);
}

int CohesionSynthAudioProcessor::removeSysExBank (const juce::String& category)
{
    // 1. the copies in the user's SysEx folder (the originals elsewhere are never touched)
    const auto dir = userSysExDir();
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.syx"))
            if (category.isEmpty() || sysExCategory (f, dir) == category)
                f.deleteFile();

    // 2. the voices in the library; keep the current preset index pointing at the same preset
    auto key = [] (const SynthPreset& p) { return p.category + "|" + p.name; };
    const juce::String current = (presetIndex >= 0 && presetIndex < (int) presets.size()) ? key (presets[(size_t) presetIndex]) : juce::String();
    int removed = 0;
    std::vector<SynthPreset> kept;
    kept.reserve (presets.size());
    for (auto& p : presets)
    {
        if (p.fm != nullptr && (category.isEmpty() || p.category == category))
        {
            const auto d = p.fm->toVced();
            fmSeen.erase (std::string (reinterpret_cast<const char*> (d.data()), d.size()));
            ++removed;
        }
        else kept.push_back (std::move (p));
    }
    presets = std::move (kept);
    importedLevels.clear();   // indices have shifted
    presetIndex = 0;
    for (int i = 0; i < (int) presets.size(); ++i)
        if (key (presets[(size_t) i]) == current) { presetIndex = i; break; }
    return removed;
}

// Parsed SysEx files, shared by every plugin instance in the process (keyed by path, size and
// date): a second instance or a project with several synths does not read and parse the
// whole library again, and the voices themselves are shared.
struct ParsedBank { std::vector<std::shared_ptr<const dx7::Patch>> voices; int banks = 0, singles = 0; };

static std::shared_ptr<const ParsedBank> parsedBank (const juce::File& f)
{
    static std::mutex m;
    static std::map<juce::String, std::pair<juce::String, std::shared_ptr<const ParsedBank>>> cache;
    const auto stamp = juce::String (f.getSize()) + "/" + juce::String (f.getLastModificationTime().toMilliseconds());
    {
        const std::lock_guard<std::mutex> lk (m);
        auto it = cache.find (f.getFullPathName());
        if (it != cache.end() && it->second.first == stamp) return it->second.second;
    }
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return nullptr;
    const auto res = dx7::parseSysEx (mb.getData(), mb.getSize());
    auto pb = std::make_shared<ParsedBank>();
    pb->banks = res.banks; pb->singles = res.singles;
    for (const auto& v : res.voices) pb->voices.push_back (std::make_shared<const dx7::Patch> (v));
    const std::lock_guard<std::mutex> lk (m);
    cache[f.getFullPathName()] = { stamp, pb };
    return pb;
}

int CohesionSynthAudioProcessor::addBank (const juce::File& f, ImportResult& r)
{
    const auto pb = parsedBank (f);
    if (pb == nullptr || pb->voices.empty()) { r.failed.add (f.getFileName()); return 0; }
    const juce::String category = sysExCategory (f, userSysExDir());

    int added = 0;
    juce::StringArray namesInBank;
    for (const auto& vp : pb->voices)
    {
        const auto& v = *vp;
        if (v.isBlank()) { ++r.blank; continue; }
        const auto d = v.toVced();
        std::string key (reinterpret_cast<const char*> (d.data()), d.size());
        if (! fmSeen.insert (key).second) { ++r.duplicates; continue; }
        juce::String name = v.getName();
        for (int k = 2; namesInBank.contains (name); ++k) name = v.getName() + " (" + juce::String (k) + ")";
        namesInBank.add (name);
        presets.push_back ({ name, category, {}, vp });
        ++added;
    }
    if (added > 0 && r.firstCategory.isEmpty()) r.firstCategory = category;
    r.banks += pb->banks + pb->singles;
    r.voices += added;
    return added;
}

void CohesionSynthAudioProcessor::loadUserBanks()
{
    const auto dir = userSysExDir();
    if (! dir.isDirectory()) return;
    auto files = dir.findChildFiles (juce::File::findFiles, true, "*.syx");
    files.sort();
    ImportResult r;
    for (auto& f : files) addBank (f, r);
}

CohesionSynthAudioProcessor::ImportResult CohesionSynthAudioProcessor::importSysEx (const juce::Array<juce::File>& items)
{
    ImportResult r;
    const auto dir = userSysExDir();
    dir.createDirectory();

    auto copyIn = [&] (const juce::File& src, juce::File dest)
    {
        if (dest.existsAsFile() && dest.getSize() == src.getSize())
        {
            juce::MemoryBlock a, b; src.loadFileAsData (a); dest.loadFileAsData (b);
            if (a == b) return dest;          // already imported
        }
        if (dest.existsAsFile()) dest = dest.getNonexistentSibling();
        dest.getParentDirectory().createDirectory();
        src.copyFileTo (dest);
        return dest;
    };

    for (const auto& item : items)
    {
        if (item.isDirectory())
        {
            auto files = item.findChildFiles (juce::File::findFiles, true, "*.syx");
            files.sort();
            const auto root = dir.getChildFile (item.getFileName());
            for (auto& f : files)
            {
                ++r.files;
                addBank (copyIn (f, root.getChildFile (f.getRelativePathFrom (item))), r);
            }
        }
        else if (item.existsAsFile())
        {
            ++r.files;
            addBank (copyIn (item, dir.getChildFile (item.getFileName())), r);
        }
    }
    return r;
}

void CohesionSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    sampleRateHz = sampleRate;
    updateSnapshot();
    fxChain.prepare (sampleRate, samplesPerBlock);
    monoScratch.setSize (1, juce::jmax (64, samplesPerBlock));
    partBuf.setSize (2 * pid::kNumParts, juce::jmax (64, samplesPerBlock));
    playedNotes.ensureSize (2048); routedMidi.ensureSize (4096);
    for (auto& b : monoLfoBuf) b.assign ((size_t) juce::jmax (64, samplesPerBlock), 0.0f);
    for (auto& g : monoLfo) g.reset (0.0);
    setLatencySamples (fxChain.lim.latency());
    arpEngine.prepare (sampleRate);
}

bool CohesionSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CohesionSynthAudioProcessor::fillSnapshot (syn::Snapshot& out)
{
    auto& snapshot = out;
    auto rd = snapSeq.reader (apvts);
    auto f  = [&rd] (juce::StringRef id) { return rd.f (id); };
    auto ip = [&rd] (juce::StringRef id) { return rd.i (id); };
    auto bp = [&rd] (juce::StringRef id) { return rd.b (id); };

    for (int i = 0; i < 4; ++i)
    {
        auto& o = snapshot.osc[i];
        const int n = i + 1;
        o.on     = bp (pid::oscOn (n));
        o.mode   = ip (pid::oscMode (n));
        o.wave   = ip (pid::oscWave (n));
        const int bank = ip (pid::oscWtBank (n));
        if (o.mode == 1) o.wave = bank;   // WT: the oscillator renders wavetable bank `wave`
        o.oct    = ip (pid::oscOct (n));
        o.semi   = ip (pid::oscSemi (n));
        o.fine   = f  (pid::oscFine (n));
        o.wtpos  = f  (pid::oscWtPos (n));
        o.unison = ip (pid::oscUni (n));
        o.detune = f  (pid::oscDet (n));
        o.pan    = f  (pid::oscPan (n));
        o.level  = f  (pid::oscLevel (n));
        o.phase  = f  (pid::oscPhase (n));
    }

    snapshot.subOn      = bp (pid::subOn);
    snapshot.subWave    = ip (pid::subWave);
    snapshot.subOct     = ip (pid::subOct);
    snapshot.subLevel   = f  (pid::subLevel);
    snapshot.subPan     = f  (pid::subPan);
    snapshot.noiseOn    = bp (pid::noiseOn);
    snapshot.noiseType  = ip (pid::noiseType);
    snapshot.noiseLevel = f  (pid::noiseLevel);
    snapshot.noisePan   = f  (pid::noisePan);
    snapshot.noiseTilt  = f  (pid::noiseTilt);

    snapshot.fmAmt   = f (pid::fmAmt);
    snapshot.ringAmt = f (pid::ringAmt);

    snapshot.fltType    = ip (pid::fltType);
    snapshot.fltCutoff  = f  (pid::fltCutoff);
    snapshot.fltRes     = f  (pid::fltRes);
    snapshot.fltDrive   = f  (pid::fltDrive);
    snapshot.fltKeyTrk  = f  (pid::fltKeyTrk);
    snapshot.fltEnvAmt  = f  (pid::fltEnvAmt);
    snapshot.flt1EnvSrc = ip (pid::flt1EnvSrc);

    snapshot.flt2Type   = ip (pid::flt2Type);
    snapshot.flt2Cutoff = f  (pid::flt2Cutoff);
    snapshot.flt2Res    = f  (pid::flt2Res);
    snapshot.flt2Drive  = f  (pid::flt2Drive);
    snapshot.flt2KeyTrk = f  (pid::flt2KeyTrk);
    snapshot.flt2EnvAmt = f  (pid::flt2EnvAmt);
    snapshot.flt2Slope  = ip (pid::flt2Slope);
    snapshot.flt2EnvSrc = ip (pid::flt2EnvSrc);

    snapshot.routing      = ip (pid::fltRouting);
    snapshot.routeBalance = f  (pid::fltBalance);

    snapshot.env[0] = { f (pid::ampA), f (pid::ampD), f (pid::ampS), f (pid::ampR) };
    for (int n = 2; n <= 4; ++n)
        snapshot.env[n - 1] = { f (pid::envA (n)), f (pid::envD (n)), f (pid::envS (n)), f (pid::envR (n)) };

    for (int n = 1; n <= 4; ++n)
    {
        auto& l = snapshot.lfo[n - 1];
        l.wave   = ip (pid::lfoWave (n));
        l.rateHz = f  (pid::lfoRate (n));
        l.sync   = bp (pid::lfoSync (n));
        l.div    = ip (pid::lfoDiv (n));
        l.fade   = f  (pid::lfoFade (n));
        l.phase  = f  (pid::lfoPhase (n));
        l.smooth = f  (pid::lfoSmooth (n));
        l.mono   = bp (pid::lfoMono (n));
    }

    for (int s = 0; s < syn::kNumMod; ++s)
        snapshot.mod[s] = { ip (pid::modSrc (s)), ip (pid::modDst (s)), f (pid::modAmt (s)) };

    snapshot.mainVol = f (pid::mainVol);
    snapshot.voiceMode = ip (pid::voiceMode);
    snapshot.glideMs = f (pid::glide);
    snapshot.velSens = f (pid::velSens);
    snapshot.dxOn = bp (pid::dxOn); snapshot.dxLevel = f (pid::dxLevel); snapshot.dxBright = f (pid::dxBright);
    snapshot.dxTime = f (pid::dxTime); snapshot.dxVel = f (pid::dxVel);
    // VA part OFF: as if no oscillator / sub / noise were on (every part check follows)
    if (! bp (pid::vaOn)) { for (auto& o : snapshot.osc) o.on = false; snapshot.subOn = snapshot.noiseOn = false; }
    snapshot.vaFiltMod = bp (pid::vaFiltMod);
    snapshot.laOn = bp (pid::laOn); snapshot.laLevel = f (pid::laLevel); snapshot.laCutoff = f (pid::laCutoff); snapshot.laTime = f (pid::laTime);

    for (int k = 0; k < pid::kNumParts; ++k)
    {
        auto& pp = snapshot.part[k];
        const float lvl = f (pid::partLevel (k));
        pp.gain = lvl <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (lvl);
        pp.pan = f (pid::partPan (k));
        pp.mute = bp (pid::partMute (k)); pp.solo = bp (pid::partSolo (k));
        pp.sendEcho = f (pid::partEcho (k)); pp.sendReverb = f (pid::partReverb (k));
        pp.keyLo = ip (pid::partKeyLo (k)); pp.keyHi = ip (pid::partKeyHi (k));
        pp.arp = bp (pid::partArp (k)); pp.viaFilter = bp (pid::partFilter (k));
    }
}

void CohesionSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    lastBlockMs.store (juce::Time::getMillisecondCounter());
    if (switchState.load() == kSilent) { midi.clear(); switchGain = 0.0f; return; }   // a new preset is being applied

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn()) midiActivity.store (1);
        // a program change selects the matching General MIDI preset (loaded on the message thread)
        if (msg.isProgramChange() && gmFirst >= 0) { pendingProgram.store (msg.getProgramChangeNumber()); triggerAsyncUpdate(); }
    }

    // --- host transport ---
    bool playing = false;
    std::optional<double> ppq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) if (*bpm > 20.0 && *bpm < 400.0) fxp.bpm = *bpm;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    fxBpm.store (fxp.bpm);

    // --- arpeggiator (rewrites the note events) ---
    // With the ARP on, its notes go out on MIDI channel 2 and the played notes on channel 1:
    // every part takes one of the two (MIXER: ARP switch per part).
    updateArpParams();
    updateSnapshot();
    bool partActive[pid::kNumParts];
    partActive[pid::kVA] = snapshot.osc[0].on || snapshot.osc[1].on || snapshot.osc[2].on || snapshot.osc[3].on
                           || snapshot.subOn || snapshot.noiseOn;
    partActive[pid::kFM] = snapshot.dxOn;
    partActive[pid::kLA] = snapshot.laOn;
    bool needPlayed = false;
    for (int k = 0; k < pid::kNumParts; ++k) needPlayed = needPlayed || (arpp.on && partActive[k] && ! snapshot.part[k].arp);
    playedNotes.clear();
    if (needPlayed)
        for (const auto meta : midi)
        {
            auto m = meta.getMessage();
            if (m.isNoteOnOrOff()) { m.setChannel (1); playedNotes.addEvent (m, meta.samplePosition); }
        }
    arpEngine.process (midi, n, arpp, fxp.bpm, playing, ppq);
    if (arpp.on)
    {
        routedMidi.clear();
        for (const auto meta : midi)
        {
            auto m = meta.getMessage();
            if (m.isNoteOnOrOff()) { m.setChannel (2); routedMidi.addEvent (m, meta.samplePosition); }
            else if (m.getChannel() > 0)
            {   // bend, mod wheel, sustain, ...: to the ARP voices (channel 2) and the played ones (1)
                m.setChannel (2); routedMidi.addEvent (m, meta.samplePosition);
                m.setChannel (1); routedMidi.addEvent (m, meta.samplePosition);
            }
            else routedMidi.addEvent (m, meta.samplePosition);
        }
        routedMidi.addEvents (playedNotes, 0, n, 0);
        midi.swapWith (routedMidi);
    }
    if (prevArpOn && ! arpp.on) synth.allNotesOff (2, true);   // release what the ARP left sounding
    prevArpOn = arpp.on;
    snapshot.arpOn = arpp.on;

    snapshot.glideMs = juce::jmax (arpp.on ? arpp.slide * 300.0f : 0.0f, snapshot.glideMs);   // arp SLIDE or GLIDE
    synth.mode = snapshot.voiceMode;
    snapshot.bpm = fxp.bpm;
    // shared (MONO) LFOs: one free-running LFO per slot, rendered for this block
    for (int i = 0; i < 4; ++i)
    {
        snapshot.monoLfo[i] = nullptr;
        if (! snapshot.lfo[i].mono) continue;
        auto& buf = monoLfoBuf[(size_t) i];
        if ((int) buf.size() < n) continue;   // block larger than announced: per-voice LFO this block
        monoLfo[i].setRate (syn::lfoHz (snapshot.lfo[i], snapshot.bpm), sampleRateHz);
        for (int k = 0; k < n; ++k) buf[(size_t) k] = monoLfo[i].next (snapshot.lfo[i].wave, snapshot.lfo[i].smooth);
        snapshot.monoLfo[i] = buf.data();
    }
    // --- the voices render every part to its own bus (VA 0/1, FM 2/3, LA 4/5) ---
    if (partBuf.getNumSamples() < n) partBuf.setSize (2 * pid::kNumParts, n, false, false, true);
    for (int c = 0; c < 2 * pid::kNumParts; ++c) partBuf.clear (c, 0, n);
    synth.renderNextBlock (partBuf, midi, 0, n);

    // --- MIXER: level, pan, mute / solo per part (ramped over the block) ---
    bool anySolo = false;
    for (int k = 0; k < pid::kNumParts; ++k) anySolo = anySolo || snapshot.part[k].solo;
    float* pl[pid::kNumParts]; float* pr[pid::kNumParts];
    float sendE[pid::kNumParts], sendR[pid::kNumParts];
    for (int k = 0; k < pid::kNumParts; ++k)
    {
        const auto& pp = snapshot.part[k];
        const bool audible = ! pp.mute && (! anySolo || pp.solo);
        const float g = audible ? pp.gain : 0.0f;
        const float tL = g * (pp.pan > 0.0f ? 1.0f - pp.pan : 1.0f), tR = g * (pp.pan < 0.0f ? 1.0f + pp.pan : 1.0f);
        pl[k] = partBuf.getWritePointer (2 * k); pr[k] = partBuf.getWritePointer (2 * k + 1);
        if (partGain[k][0] == tL && partGain[k][1] == tR)
        {
            if (tL != 1.0f) juce::FloatVectorOperations::multiply (pl[k], tL, n);
            if (tR != 1.0f) juce::FloatVectorOperations::multiply (pr[k], tR, n);
        }
        else
        {
            const float dL = (tL - partGain[k][0]) / (float) n, dR = (tR - partGain[k][1]) / (float) n;
            float gL = partGain[k][0], gR = partGain[k][1];
            for (int i = 0; i < n; ++i) { gL += dL; gR += dR; pl[k][i] *= gL; pr[k][i] *= gR; }
            partGain[k][0] = tL; partGain[k][1] = tR;
        }
        sendE[k] = pp.sendEcho; sendR[k] = pp.sendReverb;
        const float pk = juce::jmax (juce::FloatVectorOperations::findMaximum (pl[k], n), -juce::FloatVectorOperations::findMinimum (pl[k], n),
                                     juce::FloatVectorOperations::findMaximum (pr[k], n), -juce::FloatVectorOperations::findMinimum (pr[k], n));
        partPeak[k].store (juce::jmax (pk, partPeak[k].load()));   // the editor reads and clears it
    }

    // --- FX (inserts per part, ECHO / REVERB as sends, in the user's order) -> MAIN -> limiter ---
    updateFxParams();
    int order[fx::kNumFx];
    for (int k = 0; k < fx::kNumFx; ++k) order[k] = fxOrder[k].load();

    float* L = buffer.getWritePointer (0);
    float* R = nullptr;
    const bool mono = buffer.getNumChannels() < 2;
    if (mono)
    {
        if (monoScratch.getNumSamples() < n) monoScratch.setSize (1, n, false, false, true);
        R = monoScratch.getWritePointer (0);
    }
    else R = buffer.getWritePointer (1);

    fxChain.processParts (pl, pr, partActive, sendE, sendR, L, R, n, fxp, order);
    juce::FloatVectorOperations::multiply (L, snapshot.mainVol, n);
    juce::FloatVectorOperations::multiply (R, snapshot.mainVol, n);
    fxChain.lim.process (L, R, n, fxp);

    if (mono)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);

    applySwitchFade (buffer, n);
}

juce::AudioProcessorEditor* CohesionSynthAudioProcessor::createEditor()
{
    return new CohesionSynthAudioProcessorEditor (*this);
}

void CohesionSynthAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = completeState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CohesionSynthAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            // parameters stored without a value (never changed in an older project): default
            for (auto* prm : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                {
                    auto c = tree.getChildWithProperty ("id", rp->getParameterID());
                    float v;
                    if (c.isValid() && ! storedValue (c, v))
                        c.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
                }
            apvts.replaceState (tree);
            // the library position of the stored preset (PREV / NEXT go on from there)
            const auto stored = apvts.state.getProperty ("presetName").toString();
            for (int i = 0; i < (int) presets.size() && stored.isNotEmpty(); ++i)
                if (presetDisplayName (i) == stored) { presetIndex = i; break; }
            syncFxOrderFromState();
            restoreFmFromState();
            restoreLaFromState();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CohesionSynthAudioProcessor();
}
