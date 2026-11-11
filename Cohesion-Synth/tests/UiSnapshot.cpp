// Renders the editor (every tab + the library) to PNG files for visual checks.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    CohesionSynthAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
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
    const char* names[] = { "1-osc-mix", "2-filter-mod", "3-fx", "4-arp" };
    for (int t = 0; t < 4; ++t) { ed->showTab (t); save (names[t]); }
    ed->showTab (0); ed->openLibrary(); save ("5-library");
    ed->setSize (728, 504);   ed->showTab (3); save ("6-scaled-70");
    ed->setSize (1560, 1080); ed->showTab (2); save ("7-scaled-150");
    return 0;
}
