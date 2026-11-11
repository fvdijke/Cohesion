#include "PluginEditor.h"

namespace
{
    void styleButton (juce::TextButton& b, bool on, juce::Colour onBg = ui::tealDark, juce::Colour onText = juce::Colour (0xffeafff6))
    {
        b.setColour (juce::TextButton::buttonColourId, on ? onBg : ui::boxBg);
        b.setColour (juce::TextButton::textColourOffId, on ? onText : ui::dim);
    }
    void panelBox (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour edge = ui::panelEdge)
    {
        g.setColour (ui::panel); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (edge); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    }
    const juce::Colour muteOn (0xff5a2a14), muteText (0xffffe9df), soloOn (0xff5e4708), soloText (0xfffff4dc);
}

// ===========================================================================
//  Part strip
// ===========================================================================
PartStrip::PartStrip (LaModuleEditor& e, LaModuleProcessor& p, int index) : ed (e), proc (p), idx (index)
{
    auto& a = proc.apvts;
    addAndMakeVisible (led);
    for (int c = 0; c <= 16; ++c) chBox.addItem (c == 0 ? juce::String ("OFF") : "CH " + juce::String (c), c + 1);
    chAtt = std::make_unique<ComboAtt> (a, lpid::part (idx, "ch"), chBox);
    addAndMakeVisible (chBox);

    nameBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    nameBtn.setColour (juce::TextButton::textColourOffId, ui::text);
    nameBtn.onClick = [this] { ed.selectPart (idx); ed.showLibrary (idx); };
    nameBtn.setTooltip ("Choose a timbre");
    addAndMakeVisible (nameBtn);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    faderAtt = std::make_unique<SliderAtt> (a, lpid::part (idx, "lvl"), fader);
    addAndMakeVisible (fader);
    pan = std::make_unique<Knob> (a, lpid::part (idx, "pan"), "PAN", false);
    rev = std::make_unique<Knob> (a, lpid::part (idx, "rev"), "REV", false);
    ech = std::make_unique<Knob> (a, lpid::part (idx, "ech"), "ECHO", false);
    for (auto* k : { pan.get(), rev.get(), ech.get() }) addAndMakeVisible (*k);

    for (auto* b : { &muteBtn, &soloBtn }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
    muteAtt = std::make_unique<ButtonAtt> (a, lpid::part (idx, "mute"), muteBtn);
    soloAtt = std::make_unique<ButtonAtt> (a, lpid::part (idx, "solo"), soloBtn);

    outBox.addItem ("MAIN", 1); outBox.addItem ("OUT " + juce::String (idx + 2), 2);
    outAtt = std::make_unique<ComboAtt> (a, lpid::part (idx, "out"), outBox);
    addAndMakeVisible (outBox);
}

void PartStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 8);
    auto top = r.removeFromTop (16);
    led.setBounds (top.getCentreX() - 22, top.getY(), 12, 16);
    r.removeFromTop (4);
    chBox.setBounds (r.removeFromTop (20).reduced (4, 0));
    r.removeFromTop (4);
    nameBtn.setBounds (r.removeFromTop (34));
    r.removeFromTop (16);                                       // program number (painted)
    auto out = r.removeFromBottom (20); outBox.setBounds (out.reduced (4, 0));
    r.removeFromBottom (5);
    auto ms = r.removeFromBottom (20);
    muteBtn.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (3, 0));
    soloBtn.setBounds (ms.reduced (3, 0));
    r.removeFromBottom (5);
    auto sends = r.removeFromBottom (44);
    rev->setBounds (sends.removeFromLeft (sends.getWidth() / 2));
    ech->setBounds (sends);
    r.removeFromBottom (4);
    auto mid = r;
    fader.setBounds (mid.removeFromLeft (mid.getWidth() / 2).reduced (6, 0));
    pan->setBounds (mid.withSizeKeepingCentre (mid.getWidth(), 50));
}

void PartStrip::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat(), selected ? ui::teal : ui::panelEdge);
    auto r = getLocalBounds().reduced (6, 8);
    g.setColour (ui::teal); g.setFont (ui::font (14.0f, true));
    g.drawText (idx < 8 ? juce::String (idx + 1) : juce::String ("R"), r.removeFromTop (16).withTrimmedLeft (16), juce::Justification::centred);
    g.setColour (ui::label.withAlpha (dimmed ? 0.35f : 1.0f)); g.setFont (ui::font (10.5f));
    g.drawText (program, getLocalBounds().withY (8 + 16 + 4 + 20 + 4 + 34).withHeight (16), juce::Justification::centred);
}

void PartStrip::mouseDown (const juce::MouseEvent&) { ed.selectPart (idx); }

void PartStrip::refresh (bool sel)
{
    selected = sel;
    auto& a = proc.apvts;
    bool anySolo = false;
    for (int p = 0; p < mt::kParts; ++p) anySolo |= a.getRawParameterValue (lpid::part (p, "solo"))->load() > 0.5f;
    const bool mute = muteBtn.getToggleState(), solo = soloBtn.getToggleState();
    dimmed = mute || (anySolo && ! solo);
    const float al = dimmed ? 0.3f : 1.0f;
    for (juce::Component* c : { (juce::Component*) &nameBtn, (juce::Component*) &fader, (juce::Component*) pan.get(),
                                (juce::Component*) rev.get(), (juce::Component*) ech.get() })
        c->setAlpha (al);
    styleButton (muteBtn, mute, muteOn, muteText);
    styleButton (soloBtn, solo, soloOn, soloText);
    muteBtn.setColour (juce::TextButton::buttonOnColourId, muteOn); muteBtn.setColour (juce::TextButton::textColourOnId, muteText);
    soloBtn.setColour (juce::TextButton::buttonOnColourId, soloOn); soloBtn.setColour (juce::TextButton::textColourOnId, soloText);
    nameBtn.setButtonText (proc.partName (idx));
    program = idx < 8 ? "P " + juce::String (proc.partProgram (idx) + 1).paddedLeft ('0', 3) : juce::String ("KIT");
    led.level = juce::jmax (led.level * 0.82f, proc.takePartActivity (idx));
    led.repaint();
    repaint();
}

// ===========================================================================
//  Editor
// ===========================================================================
LaModuleEditor::LaModuleEditor (LaModuleProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    canvas.onPaint = [this] (juce::Graphics& g) { paintCanvas (g); };
    addAndMakeVisible (canvas);
    auto& a = proc.apvts;

    // --- header ---
    const char* engines[2] = { "MUNT", "COHESION LA" };
    for (int i = 0; i < 2; ++i)
    {
        engineBtn[i].setButtonText (engines[i]);
        engineBtn[i].onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter (lpid::engine)) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
            refreshAll();
        };
        canvas.addAndMakeVisible (engineBtn[i]);
    }
    romBox.onChange = [this]
    {
        const int id = romBox.getSelectedId();
        if (id == 1000)
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a folder with MT-32 / CM-32L ROM dumps (.zip files inside are scanned too)", juce::File());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
            {
                const auto items = fc.getResults();
                if (! items.isEmpty())
                {
                    const auto r = proc.importRoms (items);
                    status = "ROM sets added: " + juce::String ((int) r.added.size())
                           + (r.duplicates > 0 ? "  |  " + juce::String (r.duplicates) + " duplicate(s) skipped" : juce::String());
                }
                refreshRomBox();
            });
            refreshRomBox();
            return;
        }
        const auto label = romBox.getText();
        if (label.isNotEmpty() && label != proc.currentRomLabel()) { proc.setRom (label); refreshAll(); }
    };
    canvas.addAndMakeVisible (romBox);
    canvas.addAndMakeVisible (midiLed);
    master = std::make_unique<Knob> (a, lpid::master, "MASTER", false);
    canvas.addAndMakeVisible (*master);

    // --- LCD row ---
    canvas.addAndMakeVisible (lcd);
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load MT-32 SysEx (.syx)", juce::File(), "*.syx;*.SYX");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile()) status = f.getFileName() + ": " + juce::String (proc.loadSyx (f)) + " MT-32 message(s) loaded";
        });
    };
    resetBtn.onClick   = [this] { proc.requestReset(); status = "MT-32 reset"; };
    panicBtn.onClick   = [this] { proc.requestPanic(); };
    libraryBtn.onClick = [this] { showLibrary (selected); };
    for (auto* b : { &loadBtn, &resetBtn, &panicBtn, &libraryBtn }) { styleButton (*b, false); canvas.addAndMakeVisible (*b); }
    styleButton (libraryBtn, true);

    // --- strips ---
    for (int i = 0; i < mt::kParts; ++i)
    {
        strips[i] = std::make_unique<PartStrip> (*this, proc, i);
        canvas.addAndMakeVisible (*strips[i]);
    }

    // --- reverb / echo ---
    const char* modes[4] = { "ROOM", "HALL", "PLATE", "TAP DELAY" };
    for (int i = 0; i < 4; ++i)
    {
        rvModeBtn[i].setButtonText (modes[i]);
        rvModeBtn[i].onClick = [this, i] { if (auto* prm = proc.apvts.getParameter (lpid::rvMode)) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); };
        canvas.addAndMakeVisible (rvModeBtn[i]);
    }
    rvTime  = std::make_unique<Knob> (a, lpid::rvTime, "TIME");
    rvLevel = std::make_unique<Knob> (a, lpid::rvLevel, "LEVEL");
    ecDiv   = std::make_unique<Knob> (a, lpid::ecDiv, "TIME");
    ecTime  = std::make_unique<Knob> (a, lpid::ecTime, "TIME");
    ecFb    = std::make_unique<Knob> (a, lpid::ecFb, "FEEDBACK");
    ecLevel = std::make_unique<Knob> (a, lpid::ecLevel, "LEVEL");
    for (auto* k : { rvTime.get(), rvLevel.get(), ecDiv.get(), ecTime.get(), ecFb.get(), ecLevel.get() }) canvas.addAndMakeVisible (*k);
    ecSyncBtn.setClickingTogglesState (true);
    ecSyncAtt = std::make_unique<ButtonAtt> (a, lpid::ecSync, ecSyncBtn);
    canvas.addAndMakeVisible (ecSyncBtn);

    refreshRomBox();

    const double scale = juce::jlimit (0.6, 2.0, (double) proc.apvts.state.getProperty ("uiScale", 1.0));
    setResizable (true, true);
    setResizeLimits ((int) (kW * 0.6), (int) (kH * 0.6), kW * 2, kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (juce::roundToInt (kW * scale), juce::roundToInt (kH * scale));
    sizeRestored = true;
    canvas.setBounds (0, 0, kW, kH);
    layout();
    sendLookAndFeelChange();
    refreshAll();
    startTimerHz (30);
}

LaModuleEditor::~LaModuleEditor() { stopTimer(); setLookAndFeel (nullptr); }

void LaModuleEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0b0d10)); }

void LaModuleEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;
    canvas.setBounds (0, 0, kW, kH);
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (sizeRestored) proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}

void LaModuleEditor::layout()
{
    // header
    engineBtn[0].setBounds (300, 24, 70, 26);
    engineBtn[1].setBounds (370, 24, 104, 26);
    romBox.setBounds (488, 26, 150, 22);
    midiLed.setBounds (884, 26, 16, 22);
    master->setBounds (962, 12, 52, 48);

    // LCD row (y 76..148)
    lcd.setBounds (30, 88, 420, 48);
    loadBtn.setBounds (474, 114, 96, 24);
    resetBtn.setBounds (576, 114, 74, 24);
    panicBtn.setBounds (656, 114, 74, 24);
    libraryBtn.setBounds (736, 114, 96, 24);

    // strips (y 160..520)
    const int x0 = 20, w = 1000, gap = 6, sw = (w - gap * 8) / 9;
    for (int i = 0; i < mt::kParts; ++i) strips[i]->setBounds (x0 + i * (sw + gap), 160, sw, 360);

    // reverb (20..560) / echo (572..1020), y 530..646
    for (int i = 0; i < 4; ++i) rvModeBtn[i].setBounds (36 + i * 82, 562, i == 3 ? 90 : 76, 24);
    rvTime->setBounds (390, 540, 70, 98);
    rvLevel->setBounds (466, 540, 70, 98);
    ecSyncBtn.setBounds (952, 538, 56, 22);
    ecDiv->setBounds (600, 552, 90, 86);
    ecTime->setBounds (600, 552, 90, 86);
    ecFb->setBounds (720, 552, 90, 86);
    ecLevel->setBounds (840, 552, 90, 86);
}

void LaModuleEditor::paintCanvas (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff1b1f25), 0, 0, juce::Colour (0xff0d0f12), 0, (float) kH, false);
    g.setGradientFill (bg); g.fillAll();

    // title
    g.setColour (ui::text); g.setFont (ui::font (18.0f, true));
    g.drawText (juce::String::fromUTF8 ("COHESION \xe2\x80\x94 LA MODULE"), 26, 16, 280, 24, juce::Justification::centredLeft);
    g.setColour (ui::dim); g.setFont (ui::font (11.0f));
    g.drawText ("by Pepperwood", 26, 40, 200, 16, juce::Justification::centredLeft);
    g.setColour (ui::label); g.setFont (ui::font (10.0f, true));
    g.drawText ("MIDI", 840, 26, 40, 22, juce::Justification::centredRight);

    // LCD panel
    panelBox (g, { 20.0f, 76.0f, 1000.0f, 74.0f });
    g.setColour (ui::label); g.setFont (ui::font (10.0f, true));
    g.drawText ("PARTIALS", 474, 84, 100, 14, juce::Justification::centredLeft);
    g.setColour (ui::text); g.setFont (ui::font (11.0f));
    g.drawText (juce::String (partialCount) + " / 32", 732, 84, 100, 14, juce::Justification::centredRight);
    auto bar = juce::Rectangle<float> (474.0f, 101.0f, 358.0f, 6.0f);
    g.setColour (juce::Colour (0xff13181d)); g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (ui::teal.withAlpha (0.8f)); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, partialFrac)), 3.0f);
    g.setColour (ui::dim); g.setFont (ui::font (10.5f));
    g.drawFittedText (status, 848, 84, 164, 54, juce::Justification::centredLeft, 4);

    // reverb / echo panels
    panelBox (g, { 20.0f, 530.0f, 540.0f, 116.0f });
    panelBox (g, { 572.0f, 530.0f, 448.0f, 116.0f });
    g.setColour (ui::label); g.setFont (ui::font (10.0f, true));
    g.drawText ("REVERB", 36, 538, 100, 16, juce::Justification::centredLeft);
    g.drawText ("ECHO", 588, 538, 100, 16, juce::Justification::centredLeft);
    g.setColour (ui::dim); g.setFont (ui::font (10.0f));
    g.drawFittedText (juce::String::fromUTF8 (proc.usingMunt() ? "MT-32 system reverb (Munt) \xc2\xb7 set by SysEx 10 00 01"
                                                               : "MT-32 style system reverb \xc2\xb7 set by SysEx 10 00 01"),
                      36, 596, 340, 36, juce::Justification::centredLeft, 2);
}

void LaModuleEditor::refreshRomBox()
{
    romBox.clear (juce::dontSendNotification);
    const auto labels = proc.romLabels();
    for (int i = 0; i < labels.size(); ++i) romBox.addItem (labels[i], i + 1);
    romBox.addSeparator();
    romBox.addItem ("Import ROMs...", 1000);
    const int cur = labels.indexOf (proc.currentRomLabel());
    if (cur >= 0) romBox.setSelectedId (cur + 1, juce::dontSendNotification);
    else romBox.setText ("No ROM set", juce::dontSendNotification);
}

void LaModuleEditor::showLibrary (int part)
{
    auto rom = proc.currentRom();
    if (rom == nullptr) { status = juce::String::fromUTF8 ("Import a ROM set first (ROM picker \xe2\x86\x92 Import ROMs...)"); return; }
    if (part >= 8) { status = "The rhythm part plays the drum kit from the ROM / SysEx"; return; }
    juce::PopupMenu menu, ga, gb;
    for (int i = 0; i < (int) rom->melodic.size(); ++i)
    {
        const auto item = juce::String (i + 1).paddedLeft ('0', 3) + "  " + rom->melodic[(size_t) i].getName();
        (i < 64 ? ga : gb).addItem (i + 1, item, true, proc.partProgram (part) == i);
    }
    menu.addSectionHeader ("Part " + juce::String (part + 1));
    menu.addSubMenu ("Group A", ga);
    menu.addSubMenu ("Group B", gb);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (strips[part].get()), [this, part] (int r)
    {
        if (r > 0) proc.requestProgram (part, r - 1);
    });
}

void LaModuleEditor::refreshAll()
{
    const bool munt = proc.usingMunt();
    const int want = (int) proc.apvts.getRawParameterValue (lpid::engine)->load();
    styleButton (engineBtn[0], want == 0);
    styleButton (engineBtn[1], want == 1);
    for (int i = 0; i < 4; ++i) styleButton (rvModeBtn[i], (int) proc.apvts.getRawParameterValue (lpid::rvMode)->load() == i);
    const bool sync = ecSyncBtn.getToggleState();
    styleButton (ecSyncBtn, sync);
    ecDiv->setVisible (sync); ecTime->setVisible (! sync);
    if (want == 0 && ! munt && proc.muntStatus().isNotEmpty() && status.isEmpty())
        status = "MUNT unavailable (" + proc.muntStatus() + "): playing COHESION LA";
    for (int i = 0; i < mt::kParts; ++i) strips[i]->refresh (i == selected);

    const int p = selected;
    const juce::String num = p < 8 ? juce::String (p + 1) : juce::String ("R");
    lcd.line = p < 8 ? num + "|" + proc.partName (p).paddedRight (' ', 11) + juce::String (proc.partProgram (p) + 1).paddedLeft ('0', 3)
                     : juce::String ("R|Rhythm Set  KIT");
    lcd.repaint();
    canvas.repaint();
}

void LaModuleEditor::timerCallback()
{
    midiLed.level = juce::jmax (midiLed.level * 0.8f, proc.takeMidiActivity()); midiLed.repaint();
    partialCount = proc.activePartials();
    partialFrac = partialFrac * 0.6f + 0.4f * (partialCount / 32.0f);
    const auto v = proc.memoryVersion();
    if (v != lastVersion) { lastVersion = v; }
    refreshAll();
}
