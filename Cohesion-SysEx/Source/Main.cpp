// COHESION - SYSEX: build new Yamaha DX7 32-voice banks from voices picked out of a .syx collection.
//
//   "Cohesion SysEx.exe"                                   normal app
//   "Cohesion SysEx.exe" --snapshot out.png <collection>   render the UI to PNG (visual check) and quit
#include <juce_audio_utils/juce_audio_utils.h>
#include "MainComponent.h"

class CohesionSysExApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Cohesion SysEx"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String& cmd) override
    {
        auto args = juce::StringArray::fromTokens (cmd, true);
        for (auto& a : args) a = a.unquoted();
        if (args.contains ("--snapshot") && args.size() >= 3)
        {
            const int i = args.indexOf ("--snapshot");
            const juce::File out (args[i + 1]), dir (args[i + 2]);
            MainComponent mc (nullptr, false);
            mc.setSize (1320, 860);
            mc.scanNow (dir);
            mc.fillDemo();
            auto img = mc.createComponentSnapshot (mc.getLocalBounds(), true, 1.0f);
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat().writeImageToStream (img, os);
            quit();
            return;
        }

        juce::PropertiesFile::Options o;
        o.applicationName = "CohesionSysEx";
        o.filenameSuffix = ".settings";
        o.folderName = "Pepperwood";
        o.osxLibrarySubFolder = "Application Support";
        props.setStorageParameters (o);
        window = std::make_unique<MainWindow> (new MainComponent (props.getUserSettings(), true));
    }

    void shutdown() override { window = nullptr; props.closeFiles(); }
    void systemRequestedQuit() override { quit(); }

    struct MainWindow : juce::DocumentWindow
    {
        explicit MainWindow (juce::Component* content)
            : juce::DocumentWindow ("COHESION - SYSEX", juce::Colour (0xff15181c), juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, true);
            setResizeLimits (1100, 760, 4000, 3000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    juce::ApplicationProperties props;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (CohesionSysExApp)
