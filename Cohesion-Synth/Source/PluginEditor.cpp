#include "PluginEditor.h"

CohesionSynthAudioProcessorEditor::CohesionSynthAudioProcessorEditor (CohesionSynthAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // Everything lives on a fixed 1040 x 720 canvas that is scaled to the window size.
    canvas.onPaint   = [this] (juce::Graphics& g) { paintCanvas (g); };
    canvas.onResized = [this] { layoutCanvas(); };
    addAndMakeVisible (canvas);
    canvas.addChildComponent (updateBadge);   // UPDATE AVAILABLE under "by Pepperwood"

    // --- OSC / MIX tab ---
    canvas.addAndMakeVisible (oscMix);
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

    // --- FILTER / MOD tab ---
    filterTab = std::make_unique<FilterModTab> (proc.apvts);
    canvas.addChildComponent (*filterTab);

    // --- placeholder tabs ---
    fxTab     = std::make_unique<FxTab> (proc);
    arpTab    = std::make_unique<ArpTab> (proc);
    canvas.addChildComponent (*fxTab);
    canvas.addChildComponent (*arpTab);
    // the LOAD buttons of the engine tabs load into that part only (multi sounds)
    fmTab = std::make_unique<FmTab> (proc);
    fmTab->onOpenLibrary = [this] { library->open (pid::kFM); };
    canvas.addChildComponent (*fmTab);
    laTab = std::make_unique<LaTab> (proc);
    laTab->onOpenLibrary = [this] { library->open (pid::kLA); };
    canvas.addChildComponent (*laTab);
    mixer->onLoad = [this] { library->open (pid::kVA); };
    mixerTab = std::make_unique<MixerTab> (proc);
    mixerTab->onLoad = [this] (int part) { library->open (part); };
    canvas.addChildComponent (*mixerTab);

    // --- tabs ---
    const char* names[] = { "OSC / MIX", "FM", "LA", "FILTER / MOD", "FX", "ARP", "MIXER" };
    for (int i = 0; i < kNumTabs; ++i)
    {
        tabs[i].setButtonText (names[i]);
        tabs[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        tabs[i].onClick = [this, i] { showTab (i); };
        canvas.addAndMakeVisible (tabs[i]);
    }

    // --- header ---
    canvas.addAndMakeVisible (midiLed);
    mainKnob = std::make_unique<LabeledKnob> (proc.apvts, pid::mainVol, "MAIN");
    canvas.addAndMakeVisible (*mainKnob);
    proc.onPresetChanged = [this] { presetLoaded(); };
    glideKnob = std::make_unique<LabeledKnob> (proc.apvts, pid::glide, "GLIDE");
    velKnob   = std::make_unique<LabeledKnob> (proc.apvts, pid::velSens, "VELOCITY");
    canvas.addAndMakeVisible (*glideKnob);
    canvas.addAndMakeVisible (*velKnob);
    voiceModeBox.addItemList ({ "POLY", "MONO", "LEGATO" }, 1);
    voiceModeAtt = std::make_unique<ComboAtt> (proc.apvts, pid::voiceMode, voiceModeBox);
    canvas.addAndMakeVisible (voiceModeBox);
    presetName.setJustificationType (juce::Justification::centred);
    presetName.setColour (juce::Label::textColourId, juce::Colour (0xffeafff6));
    presetName.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0e1013));
    presetName.onClick = [this] { library->open(); };
    presetName.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    canvas.addAndMakeVisible (presetName);
    presetPrev.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    presetNext.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    presetPrev.onClick = [this] { proc.loadPreset (proc.currentPresetIndex() - 1); presetLoaded(); };
    presetNext.onClick = [this] { proc.loadPreset (proc.currentPresetIndex() + 1); presetLoaded(); };
    saveBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    saveBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeafff6));
    saveBtn.setTooltip ("Save the current sound as a User preset");
    saveBtn.onClick = [this] { openSaveDialog(); };
    canvas.addAndMakeVisible (saveBtn);
    canvas.addAndMakeVisible (presetPrev);
    canvas.addAndMakeVisible (presetNext);
    libraryBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0f6e56));
    libraryBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeafff6));
    libraryBtn.onClick = [this] { library->open(); };
    canvas.addAndMakeVisible (libraryBtn);
    library = std::make_unique<LibraryOverlay> (proc);
    library->onPresetLoaded = [this] { presetLoaded(); };
    library->onListChanged  = [this] { updatePresetName(); };   // banks / user presets removed or renamed
    library->onPartLoaded   = [this] (int) { updatePresetName(); updateDimming(); };   // one part of a multi sound
    canvas.addChildComponent (*library);
    updatePresetName();

    showTab (0);

    // --- resizable, fixed aspect ratio, size remembered in the plugin state ---
    // Read the stored scale first: setting the resize limits triggers a resize to the
    // minimum size, which must not overwrite the remembered size.
    const double scale = juce::jlimit (0.6, 2.0, (double) proc.apvts.state.getProperty ("uiScale", 1.0));
    setResizable (true, true);
    setResizeLimits ((int) (kW * 0.6), (int) (kH * 0.6), kW * 2, kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (juce::roundToInt (kW * scale), juce::roundToInt (kH * scale));
    sizeRestored = true;
    // Value boxes of sliders are created when the slider is constructed, i.e. before it
    // is parented into this editor, so they pick up JUCE's default look (light frames).
    // Re-send the look-and-feel so every value box is rebuilt in the Cohesion style.
    sendLookAndFeelChange();
    updateDimming();
    startTimerHz (30);
}

CohesionSynthAudioProcessorEditor::~CohesionSynthAudioProcessorEditor()
{
    proc.onPresetChanged = nullptr;
    setLookAndFeel (nullptr);
}

void CohesionSynthAudioProcessorEditor::showTab (int t)
{
    currentTab = t;
    oscMix.setVisible (t == 0);
    fmTab->setVisible (t == 1);
    laTab->setVisible (t == 2);
    filterTab->setVisible (t == 3);
    fxTab->setVisible (t == 4);
    arpTab->setVisible (t == 5);
    mixerTab->setVisible (t == 6);
    for (int i = 0; i < kNumTabs; ++i)
    {
        tabs[i].setColour (juce::TextButton::buttonColourId, i == t ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        tabs[i].setColour (juce::TextButton::textColourOffId, i == t ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
    }
}

void CohesionSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171b));   // behind the canvas (only visible while dragging)
}

void CohesionSynthAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;
    canvas.setBounds (0, 0, kW, kH);
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (sizeRestored) proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}

void CohesionSynthAudioProcessorEditor::paintCanvas (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff2b2f36), 0, 0, juce::Colour (0xff15171b), 0, (float) kH, false);
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
    g.drawText ("PRESET", kW / 2 - 60, 8, 120, 14, juce::Justification::centred);
    g.drawText ("VOICE", 186, 8, 90, 14, juce::Justification::centred);
}

void CohesionSynthAudioProcessorEditor::layoutCanvas()
{
    auto r = canvas.getLocalBounds();

    // header
    auto header = r.removeFromTop (70);
    auto right = header.removeFromRight (190);
    midiLed.setBounds (right.removeFromRight (80).withSizeKeepingCentre (70, 20).translated (-6, -8));
    mainKnob->setBounds (right.removeFromRight (70).withTrimmedTop (6).withTrimmedBottom (2));
    // preset block centred
    juce::Rectangle<int> pb (kW / 2 - 170, 26, 316, 30);
    presetPrev.setBounds (pb.removeFromLeft (28));
    presetNext.setBounds (pb.removeFromRight (28));
    presetName.setBounds (pb.reduced (6, 0));
    saveBtn.setBounds (kW / 2 + 152, 26, 56, 30);
    libraryBtn.setBounds (kW / 2 + 214, 26, 78, 30);
    voiceModeBox.setBounds (186, 26, 90, 30);
    updateBadge.setFontHeight (8.5f);
    updateBadge.placeAt (26, 56, 13);
    glideKnob->setBounds (282, 2, 62, 66);
    velKnob->setBounds (kW / 2 + 296, 2, 62, 66);
    library->setBounds (canvas.getLocalBounds());

    // tabs
    auto tabRow = r.removeFromTop (44).reduced (0, 6);
    const int tw = 132, gap = 8;
    int tx = (kW - (kNumTabs * tw + (kNumTabs - 1) * gap)) / 2;
    for (int i = 0; i < kNumTabs; ++i) { tabs[i].setBounds (tx, tabRow.getY(), tw, 32); tx += tw + gap; }

    // content
    auto content = r.reduced (16, 6);
    oscMix.setBounds (content);
    fmTab->setBounds (content);
    laTab->setBounds (content);
    filterTab->setBounds (content);
    fxTab->setBounds (content);
    arpTab->setBounds (content);
    mixerTab->setBounds (content);

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

void CohesionSynthAudioProcessorEditor::updatePresetName()
{
    presetName.setText (proc.headerName(), juce::dontSendNotification);
    presetName.setTooltip (proc.headerName() + "   (click: LIBRARY)");
}

// A loaded preset opens the tab of the engine that makes its sound: a DX7 voice (FM engine on,
// no oscillator) -> FM, an MT-32 / CM-32L timbre -> LA, everything else -> OSC / MIX.
void CohesionSynthAudioProcessorEditor::presetLoaded()
{
    updatePresetName();
    auto on = [this] (const juce::String& id) { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr && v->load() > 0.5f; };
    bool anyOsc = false;
    for (int i = 1; i <= 4; ++i) anyOsc = anyOsc || on (pid::oscOn (i));
    if (on (pid::dxOn) && ! anyOsc)      showTab (1);
    else if (on (pid::laOn) && ! anyOsc) showTab (2);
    else                                 showTab (0);
}

void CohesionSynthAudioProcessorEditor::openSaveDialog()
{
    const int cur = proc.currentPresetIndex();
    auto* w = new juce::AlertWindow ("Save preset", "Save the current sound as a User preset.\n"
                                     "It appears in the LIBRARY under \"User\".", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", proc.presetName (cur), "Name");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    if (auto* te = w->getTextEditor ("name")) { te->setInputRestrictions (64); te->selectAll(); }
    juce::Component::SafePointer<CohesionSynthAudioProcessorEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        const auto name = w->getTextEditorContents ("name");
        if (r == 1 && safe != nullptr) safe->saveAs (name, false);
    }), true);
}

void CohesionSynthAudioProcessorEditor::saveAs (const juce::String& name, bool overwrite)
{
    const int r = proc.saveUserPreset (name, overwrite);
    if (r == CohesionSynthAudioProcessor::kSaveExists)
    {
        juce::Component::SafePointer<CohesionSynthAudioProcessorEditor> safe (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Save preset",
            "A User preset \"" + name.trim() + "\" already exists. Replace it?", "Replace", "Cancel", this,
            juce::ModalCallbackFunction::create ([safe, name] (int ok) { if (ok != 0 && safe != nullptr) safe->saveAs (name, true); }));
        return;
    }
    if (r == CohesionSynthAudioProcessor::kSaveBadName)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", "Please enter a name.", {}, this);
        return;
    }
    if (r < 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset",
            "The preset could not be written to\n" + CohesionSynthAudioProcessor::userPresetDir().getFullPathName(), {}, this);
        return;
    }
    updatePresetName();
}

void CohesionSynthAudioProcessorEditor::timerCallback()
{
    if (proc.midiActivity.exchange (0) > 0)
        midiLed.level = 1.0f;
    else
        midiLed.level *= 0.82f;
    midiLed.repaint();

    updateDimming();
}

void CohesionSynthAudioProcessorEditor::updateDimming()
{
    // dim bypassed oscillators (panel + mixer strip)
    auto isOn = [this] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };
    for (int i = 0; i < 4; ++i)
    {
        const bool off = ! isOn (pid::oscOn (i + 1));
        oscPanels[i]->setDimmed (off);
        mixer->setStripDimmed (i, off);
    }
    subPanel->setDimmed (! isOn (pid::subOn));     mixer->setStripDimmed (4, ! isOn (pid::subOn));
    noisePanel->setDimmed (! isOn (pid::noiseOn)); mixer->setStripDimmed (5, ! isOn (pid::noiseOn));
}
