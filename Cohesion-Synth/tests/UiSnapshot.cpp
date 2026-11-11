// Renders the editor (every tab + the library) to PNG files for visual checks.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

#include <cstdlib>
static void setEnv (const char* k, const juce::String& v)
{
   #if JUCE_WINDOWS
    _putenv_s (k, v.toRawUTF8());
   #else
    if (v.isEmpty()) unsetenv (k); else setenv (k, v.toRawUTF8(), 1);
   #endif
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    // ---- SysEx import test in a throw-away folder (argv[2] = a bank file) ----
    if (argc > 2)
    {
        const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion-sysex-test");
        tmp.deleteRecursively(); tmp.createDirectory();
        setEnv ("COHESION_SYNTH_SYSEX_DIR", tmp.getFullPathName());
        const juce::File bank { juce::String (argv[2]) };
        int before = 0, afterImport = 0, again = 0, reloaded = 0;
        {
            CohesionSynthAudioProcessor a;
            before = a.numPresets();
            auto r1 = a.importSysEx ({ bank });
            afterImport = a.numPresets();
            auto r2 = a.importSysEx ({ bank });          // importing the same bank again must not duplicate
            again = a.numPresets();
            std::printf ("import #1: %d voices, %d blank, %d duplicates, category '%s'\n", r1.voices, r1.blank, r1.duplicates, r1.firstCategory.toRawUTF8());
            std::printf ("import #2: %d voices, %d duplicates\n", r2.voices, r2.duplicates);
            // loading an imported preset switches the engine to FM with that voice
            a.loadPreset (before);
            std::printf ("loaded preset '%s' -> FM voice '%s', FM on %d, OSC1 on %d\n", a.presetName (before).toRawUTF8(),
                         a.getFmPatch().getName().toRawUTF8(), (int) a.apvts.getRawParameterValue (pid::dxOn)->load(), (int) a.apvts.getRawParameterValue (pid::oscOn (1))->load());
        }
        { CohesionSynthAudioProcessor b; reloaded = b.numPresets(); }   // start-up reload from the folder
        std::printf ("presets: factory %d -> after import %d -> re-import %d -> fresh instance %d  [%s]\n", before, afterImport, again, reloaded,
                     (afterImport > before && again == afterImport && reloaded == afterImport) ? "OK" : "FAIL");
        tmp.deleteRecursively();
        setEnv ("COHESION_SYNTH_SYSEX_DIR", {});
    }
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    // ---- ROM import test (argv[3] = a folder with MT-32 / CM-32L ROMs) in a throw-away ROM folder ----
    juce::File romTmp;
    if (argc > 3)
    {
        romTmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion-rom-test");
        romTmp.deleteRecursively(); romTmp.createDirectory();
        setEnv ("COHESION_SYNTH_ROM_DIR", romTmp.getFullPathName());
        CohesionSynthAudioProcessor a;
        const int before = a.numPresets();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto r = a.importRoms ({ juce::File { juce::String (argv[3]) } });
        std::printf ("ROM import: %d set(s) added, %d duplicate version(s) skipped, %d file(s) scanned, %.0f ms\n",
                     r.sets, r.duplicates, r.files, juce::Time::getMillisecondCounterHiRes() - t0);
        if (r.failed.size() > 0) std::printf ("  not recognised: %s\n", r.failed.joinIntoString (", ").toRawUTF8());
        for (auto& d : romTmp.findChildFiles (juce::File::findDirectories, false)) std::printf ("  stored set: %s\n", d.getFileName().toRawUTF8());
        std::printf ("  presets %d -> %d\n", before, a.numPresets());
        CohesionSynthAudioProcessor b;
        std::printf ("  fresh instance reloads: %d presets [%s]\n", b.numPresets(), b.numPresets() == a.numPresets() ? "OK" : "FAIL");
    }

    CohesionSynthAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // optional: load the first voice of a DX7 bank (argv[2]) into the FM engine (no files written)
    if (argc > 2)
    {
        juce::MemoryBlock mb; juce::File { juce::String (argv[2]) }.loadFileAsData (mb);
        auto res = dx7::parseSysEx (mb.getData(), mb.getSize());
        for (auto& v : res.voices)
            if (! v.isBlank())
            {
                proc.loadPreset (0);
                if (auto* p = proc.apvts.getParameter (pid::dxOn)) p->setValueNotifyingHost (1.0f);
                for (int o = 1; o <= 2; ++o) if (auto* p = proc.apvts.getParameter (pid::oscOn (o))) p->setValueNotifyingHost (0.0f);
                if (auto* p = proc.apvts.getParameter (pid::fltCutoff)) p->setValueNotifyingHost (1.0f);
                if (auto* p = proc.apvts.getParameter (pid::ampR)) p->setValueNotifyingHost (p->convertTo0to1 (8000.0f));
                proc.setFmPatch (v, juce::String::fromUTF8 ("DX7 \xc2\xb7 ") + juce::File { juce::String (argv[2]) }.getFileNameWithoutExtension());
                break;
            }
        // play a chord through the complete processor (MIDI -> voices -> FX -> limiter)
        juce::AudioBuffer<float> buf (2, 512);
        float holdPk = 0, tailPk = 0; bool finite = true;
        for (int s = 0; s < (int) (4.0 * 48000); s += 512)
        {
            juce::MidiBuffer mb2;
            if (s == 0) for (int n : { 48, 52, 55 }) mb2.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
            if (s <= 48000 && 48000 < s + 512) for (int n : { 48, 52, 55 }) mb2.addEvent (juce::MidiMessage::noteOff (1, n), 48000 - s);
            buf.clear(); proc.processBlock (buf, mb2);
            const float pk = buf.getMagnitude (0, 512);
            for (int k = 0; k < 512; ++k) if (! std::isfinite (buf.getSample (0, k))) finite = false;
            if (s < 48000) holdPk = std::max (holdPk, pk); else if (s > 3 * 48000) tailPk = std::max (tailPk, pk);
        }
        std::printf ("processor FM path: held peak %.3f, tail (2 s after release) %.5f, finite %s\n", holdPk, tailPk, finite ? "yes" : "NO");
    }
    // load the first LA preset (AcouPiano1 of the first ROM set) and play a chord through the processor
    if (argc > 3)
    {
        for (int i = 0; i < proc.numPresets(); ++i)
            if (proc.presetIsImported (i) && proc.presetCategory (i).endsWith ("A") && ! proc.presetCategory (i).startsWith ("DX7"))
            { proc.loadPreset (i); std::printf ("LA preset loaded: %s / %s\n", proc.presetCategory (i).toRawUTF8(), proc.presetName (i).toRawUTF8()); break; }
        proc.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512); float held = 0, tail = 0; bool finite = true;
        for (int s = 0; s < (int) (6.0 * 48000); s += 512)
        {
            juce::MidiBuffer m;
            if (s == 0) for (int n : { 48, 52, 55, 60 }) m.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
            if (s <= 96000 && 96000 < s + 512) for (int n : { 48, 52, 55, 60 }) m.addEvent (juce::MidiMessage::noteOff (1, n), 96000 - s);
            buf.clear(); proc.processBlock (buf, m);
            for (int k = 0; k < 512; ++k) if (! std::isfinite (buf.getSample (0, k))) finite = false;
            const float pk = buf.getMagnitude (0, 512);
            if (s < 96000) held = std::max (held, pk); else if (s > 5 * 48000) tail = std::max (tail, pk);
        }
        std::printf ("processor LA path: held peak %.3f, tail 3 s after release %.5f, finite %s\n", held, tail, finite ? "yes" : "NO");
    }

    std::unique_ptr<CohesionSynthAudioProcessorEditor> ed (dynamic_cast<CohesionSynthAudioProcessorEditor*> (proc.createEditor()));
    ed->setOpaque (true);
    auto save = [&] (const juce::String& name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream os (outDir.getChildFile (name + ".png"));
        os.setPosition (0); os.truncate();
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", outDir.getChildFile (name + ".png").getFullPathName().toRawUTF8());
    };
    const char* names[] = { "1-osc-mix", "2-fm", "3-la", "4-filter-mod", "5-fx", "6-arp" };
    for (int t = 0; t < 6; ++t) { ed->showTab (t); save (names[t]); }
    ed->showTab (0); ed->openLibrary(); save ("7-library");
    ed->setSize (728, 504);   ed->showTab (5); save ("8-scaled-70");
    ed->setSize (1560, 1080); ed->showTab (4); save ("9-scaled-150");
    if (romTmp != juce::File()) { romTmp.deleteRecursively(); setEnv ("COHESION_SYNTH_ROM_DIR", {}); }
    return 0;
}
