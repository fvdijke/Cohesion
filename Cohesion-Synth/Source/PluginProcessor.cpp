#include "PluginProcessor.h"
#include "PluginEditor.h"

CohesionSynthAudioProcessor::CohesionSynthAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", cohesionSynthLayout())
{
    synth.addSound (new SynthSound());
    for (int i = 0; i < 16; ++i)
    {
        auto* v = new SynthVoice();
        v->setSnapshot (&snapshot);
        v->setGlideSource (&lastNoteHz);
        v->setFmSource (&fmSource);
        synth.addVoice (v);
    }
    presets = cohesionSynthPresets();
    setFmPatch (dx7::initVoice(), {});
    loadUserBanks();
    if (! apvts.state.hasProperty ("fxOrder"))
        apvts.state.setProperty ("fxOrder", "0,1,2,3,4", nullptr);
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
    auto f  = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto ip = [this] (const juce::String& id) { return (int) apvts.getRawParameterValue (id)->load(); };
    auto bp = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };
    arpp.on = bp (pid::arpOn); arpp.mode = ip (pid::arpMode); arpp.rate = ip (pid::arpRate); arpp.octaves = ip (pid::arpOct);
    arpp.gate = f (pid::arpGate); arpp.swing = f (pid::arpSwing); arpp.random = f (pid::arpRandom);
    arpp.sync = bp (pid::arpSync); arpp.hold = bp (pid::arpHold); arpp.latch = bp (pid::arpLatch);
    arpp.tempo = f (pid::arpTempo); arpp.steps = ip (pid::arpSteps); arpp.order = ip (pid::arpOrder);
    arpp.velAmt = f (pid::arpVelAmt); arpp.slide = f (pid::arpSlide); arpp.humanize = f (pid::arpHuman);
    for (int i = 0; i < arp::kSteps; ++i) { arpp.stepOn[i] = bp (pid::arpStepOn (i)); arpp.stepVel[i] = f (pid::arpStepVel (i)); }
}

void CohesionSynthAudioProcessor::updateFxParams()
{
    auto f  = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto ip = [this] (const char* id) { return (int) apvts.getRawParameterValue (id)->load(); };
    auto bp = [this] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };
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
    if (pr.fm != nullptr)
    {
        applySynthPreset (*this, apvts, dxInitPreset());
        setFmPatch (*pr.fm, pr.category);
    }
    else applySynthPreset (*this, apvts, pr);
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
//  SysEx banks
// ---------------------------------------------------------------------------
juce::File CohesionSynthAudioProcessor::userSysExDir()
{
    // tests redirect the folder; normal use: %APPDATA%/Pepperwood/Cohesion Synth/SysEx
    const auto overrideDir = juce::SystemStats::getEnvironmentVariable ("COHESION_SYNTH_SYSEX_DIR", {});
    if (overrideDir.isNotEmpty()) return juce::File (overrideDir);
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Pepperwood").getChildFile ("Cohesion Synth").getChildFile ("SysEx");
}

int CohesionSynthAudioProcessor::addBank (const juce::File& f, ImportResult& r)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { r.failed.add (f.getFileName()); return 0; }
    const auto res = dx7::parseSysEx (mb.getData(), mb.getSize());
    if (res.voices.empty()) { r.failed.add (f.getFileName()); return 0; }

    auto rel = f.getRelativePathFrom (userSysExDir()).replaceCharacter ('\\', '/');
    if (rel.startsWith ("..")) rel = f.getFileName();
    const juce::String category = juce::String::fromUTF8 ("DX7 \xc2\xb7 ") + rel.upToLastOccurrenceOf (".", false, false);

    int added = 0;
    juce::StringArray namesInBank;
    for (const auto& v : res.voices)
    {
        if (v.isBlank()) { ++r.blank; continue; }
        const auto d = v.toVced();
        std::string key (reinterpret_cast<const char*> (d.data()), d.size());
        if (! fmSeen.insert (key).second) { ++r.duplicates; continue; }
        juce::String name = v.getName();
        for (int k = 2; namesInBank.contains (name); ++k) name = v.getName() + " (" + juce::String (k) + ")";
        namesInBank.add (name);
        presets.push_back ({ name, category, {}, std::make_shared<const dx7::Patch> (v) });
        ++added;
    }
    if (added > 0 && r.firstCategory.isEmpty()) r.firstCategory = category;
    r.banks += res.banks + res.singles;
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
    updateSnapshot();
    fxChain.prepare (sampleRate, samplesPerBlock);
    monoScratch.setSize (1, juce::jmax (64, samplesPerBlock));
    setLatencySamples (fxChain.lim.latency());
    arpEngine.prepare (sampleRate);
}

bool CohesionSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CohesionSynthAudioProcessor::updateSnapshot()
{
    auto f  = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto ip = [this] (const juce::String& id) { return (int) apvts.getRawParameterValue (id)->load(); };
    auto bp = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };

    for (int i = 0; i < 4; ++i)
    {
        auto& o = snapshot.osc[i];
        const int n = i + 1;
        o.on     = bp (pid::oscOn (n));
        o.mode   = ip (pid::oscMode (n));
        o.wave   = ip (pid::oscWave (n));
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
    snapshot.dxOn = bp (pid::dxOn); snapshot.dxLevel = f (pid::dxLevel); snapshot.dxBright = f (pid::dxBright);
    snapshot.dxTime = f (pid::dxTime); snapshot.dxVel = f (pid::dxVel);
}

void CohesionSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();

    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn()) { midiActivity.store (1); break; }

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
    updateArpParams();
    arpEngine.process (midi, n, arpp, fxp.bpm, playing, ppq);

    updateSnapshot();
    snapshot.glideMs = arpp.on ? arpp.slide * 300.0f : 0.0f;
    synth.renderNextBlock (buffer, midi, 0, n);

    // --- FX chain (in the user's order) -> MAIN -> limiter ---
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
        juce::FloatVectorOperations::copy (R, L, n);
    }
    else R = buffer.getWritePointer (1);

    fxChain.process (L, R, n, fxp, order);
    juce::FloatVectorOperations::multiply (L, snapshot.mainVol, n);
    juce::FloatVectorOperations::multiply (R, snapshot.mainVol, n);
    fxChain.lim.process (L, R, n, fxp);

    if (mono)
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
}

juce::AudioProcessorEditor* CohesionSynthAudioProcessor::createEditor()
{
    return new CohesionSynthAudioProcessorEditor (*this);
}

void CohesionSynthAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CohesionSynthAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            syncFxOrderFromState();
            restoreFmFromState();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CohesionSynthAudioProcessor();
}
