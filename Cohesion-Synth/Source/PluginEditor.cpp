#include "PluginEditor.h"

CohesionSynthAudioProcessorEditor::CohesionSynthAudioProcessorEditor (CohesionSynthAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // --- OSC / MIX tab ---
    addAndMakeVisible (oscMix);
    for (int i = 0; i < 4; ++i)
    {
        oscPanels[i] = std::make_unique<OscPanel> (proc.apvts, i + 1);
        oscMix.addAndMakeVisible (*oscPanels[i]);
    }
    subPanel = std::make_unique<MiniPanel> (proc.apvts, "SUB", pid::subOn, pid::subWave,
                                            juce::StringArray { "Sine", "Triangle", "Square" },
                                            std::vector<std::pair<juce::String, juce::String>> {
                                                { pid::subOct, "OCT" }, { pid::subLevel, "LEVEL" }, { pid::subPan, "PAN" } });
    oscMix.addAndMakeVisible (*subPanel);
    noisePanel = std::make_unique<MiniPanel> (proc.apvts, "NOISE", pid::noiseOn, pid::noiseType,
                                              juce::StringArray { "White", "Pink" },
                                              std::vector<std::pair<juce::String, juce::String>> {
                                                  { pid::noiseLevel, "LEVEL" }, { pid::noisePan, "PAN" }, { pid::noiseTilt, "FILTER" } });
    oscMix.addAndMakeVisible (*noisePanel);
    mixer = std::make_unique<MixerPanel> (proc.apvts);
    oscMix.addAndMakeVisible (*mixer);

    // --- placeholder tabs ---
    filterTab = std::make_unique<PlaceholderPanel> ("FILTER / MOD  —  coming soon");
    fxTab     = std::make_unique<PlaceholderPanel> ("FX  —  coming soon");
    arpTab    = std::make_unique<PlaceholderPanel> ("ARP  —  coming soon");
    addChildComponent (*filterTab);
    addChildComponent (*fxTab);
    addChildComponent (*arpTab);

    // --- tabs ---
    const char* names[] = { "OSC / MIX", "FILTER / MOD", "FX", "ARP" };
    for (int i = 0; i < 4; ++i)
    {
        tabs[i].setButtonText (names[i]);
        tabs[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        tabs[i].onClick = [this, i] { showTab (i); };
        addAndMakeVisible (tabs[i]);
    }

    // --- header ---
    addAndMakeVisible (midiLed);
    mainKnob = std::make_unique<LabeledKnob> (proc.apvts, pid::mainVol, "MAIN");
    addAndMakeVisible (*mainKnob);
    presetName.setText ("Init  —  Default", juce::dontSendNotification);
    presetName.setJustificationType (juce::Justification::centred);
    presetName.setColour (juce::Label::textColourId, juce::Colour (0xffeafff6));
    presetName.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0e1013));
    addAndMakeVisible (presetName);
    presetPrev.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    presetNext.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    addAndMakeVisible (presetPrev);
    addAndMakeVisible (presetNext);

    showTab (0);
    setSize (1040, 720);
    startTimerHz (30);
}

CohesionSynthAudioProcessorEditor::~CohesionSynthAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CohesionSynthAudioProcessorEditor::showTab (int t)
{
    currentTab = t;
    oscMix.setVisible (t == 0);
    filterTab->setVisible (t == 1);
    fxTab->setVisible (t == 2);
    arpTab->setVisible (t == 3);
    for (int i = 0; i < 4; ++i)
    {
        tabs[i].setColour (juce::TextButton::buttonColourId, i == t ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        tabs[i].setColour (juce::TextButton::textColourOffId, i == t ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
    }
}

void CohesionSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff2b2f36), 0, 0, juce::Colour (0xff15171b), 0, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (juce::Colour (0xffffffff));
    g.setFont (juce::Font ("Segoe UI", 18.0f, juce::Font::bold));
    g.drawText ("COHESION - SYNTH", 26, 16, 400, 24, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
    g.drawText ("by Pepperwood", 26, 40, 300, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff9fb0aa));
    g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::plain));
    g.drawText ("PRESET", getWidth() / 2 - 60, 8, 120, 14, juce::Justification::centred);
}

void CohesionSynthAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    // header
    auto header = r.removeFromTop (70);
    auto right = header.removeFromRight (190);
    midiLed.setBounds (right.removeFromRight (80).withSizeKeepingCentre (70, 20).translated (-6, -8));
    mainKnob->setBounds (right.removeFromRight (70).withTrimmedTop (6).withTrimmedBottom (2));
    // preset block centred
    juce::Rectangle<int> pb (getWidth() / 2 - 170, 26, 340, 30);
    presetPrev.setBounds (pb.removeFromLeft (28));
    presetNext.setBounds (pb.removeFromRight (28));
    presetName.setBounds (pb.reduced (6, 0));

    // tabs
    auto tabRow = r.removeFromTop (44).reduced (0, 6);
    tabRow.removeFromLeft (248 - 0);
    int tw = 150, gap = 8, tx = 248;
    for (int i = 0; i < 4; ++i) { tabs[i].setBounds (tx, tabRow.getY(), tw, 32); tx += tw + gap; }

    // content
    auto content = r.reduced (16, 6);
    oscMix.setBounds (content);
    filterTab->setBounds (content);
    fxTab->setBounds (content);
    arpTab->setBounds (content);

    // OSC / MIX internal layout (relative to oscMix)
    auto c = oscMix.getLocalBounds();
    const int gapp = 10;
    auto topRow = c.removeFromTop (330);
    const int pw = (topRow.getWidth() - 3 * gapp) / 4;
    for (int i = 0; i < 4; ++i)
        oscPanels[i]->setBounds (topRow.getX() + i * (pw + gapp), topRow.getY(), pw, topRow.getHeight());

    c.removeFromTop (gapp);
    auto botRow = c;
    subPanel->setBounds (botRow.removeFromLeft (180));
    botRow.removeFromLeft (gapp);
    noisePanel->setBounds (botRow.removeFromLeft (180));
    botRow.removeFromLeft (gapp);
    mixer->setBounds (botRow);
}

void CohesionSynthAudioProcessorEditor::timerCallback()
{
    if (proc.midiActivity.exchange (0) > 0)
        midiLed.level = 1.0f;
    else
        midiLed.level *= 0.82f;
    midiLed.repaint();
}
