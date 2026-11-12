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
    juce::String partLabel (int p) { return p < 8 ? "PART " + juce::String (p + 1) : juce::String ("RHYTHM"); }
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
    nameBtn.getProperties().set ("fontSize", 12.5f);
    addAndMakeVisible (nameBtn);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    faderAtt = std::make_unique<SliderAtt> (a, lpid::part (idx, "lvl"), fader);
    addAndMakeVisible (fader);
    pan = std::make_unique<Knob> (a, lpid::part (idx, "pan"), "PAN", false);
    addAndMakeVisible (*pan);

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
    nameBtn.setBounds (r.removeFromTop (30));
    r.removeFromTop (3);
    partialsArea = r.removeFromTop (15);                       // "n / 32" (painted)
    r.removeFromTop (6);
    auto out = r.removeFromBottom (20); outBox.setBounds (out.reduced (4, 0));
    r.removeFromBottom (5);
    auto ms = r.removeFromBottom (20);
    muteBtn.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (3, 0));
    soloBtn.setBounds (ms.reduced (3, 0));
    r.removeFromBottom (6);
    fxArea = r.removeFromBottom (16);                           // REV / ECHO indicators (painted)
    r.removeFromBottom (6);
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

    // this part's partials, of its own 32 (orange close to the limit)
    {
        const auto col = partials >= 28 ? juce::Colour (0xfff0997b) : partials > 0 ? ui::teal : ui::label;
        juce::AttributedString a;
        a.append (juce::String (partials), ui::font (11.0f, true), col.withAlpha (dimmed ? 0.4f : 1.0f));
        a.append (" / 32", ui::font (11.0f), ui::label.withAlpha (dimmed ? 0.4f : 1.0f));
        a.setJustification (juce::Justification::centred);
        a.draw (g, partialsArea.toFloat());
    }

    // this part's reverb / echo state (edited in the boxes below when the part is selected)
    auto f = fxArea;
    auto pill = [&] (juce::Rectangle<int> b, const char* t, bool on)
    {
        auto rr = b.reduced (3, 1).toFloat();
        g.setColour (on ? ui::tealDark.withAlpha (dimmed ? 0.4f : 1.0f) : ui::boxBg); g.fillRoundedRectangle (rr, 3.0f);
        g.setColour (on ? ui::teal.withAlpha (dimmed ? 0.4f : 0.8f) : juce::Colour (0xff2b3138)); g.drawRoundedRectangle (rr, 3.0f, 1.0f);
        g.setColour (on ? juce::Colour (0xffeafff6).withAlpha (dimmed ? 0.4f : 1.0f) : ui::label.withAlpha (0.6f));
        g.setFont (ui::font (9.0f, true)); g.drawText (t, rr, juce::Justification::centred);
    };
    pill (f.removeFromLeft (f.getWidth() / 2), "REV", rvOn);
    pill (f, "ECHO", ecOn);
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
    for (juce::Component* c : { (juce::Component*) &nameBtn, (juce::Component*) &fader, (juce::Component*) pan.get() }) c->setAlpha (al);
    styleButton (muteBtn, mute, muteOn, muteText);
    styleButton (soloBtn, solo, soloOn, soloText);
    muteBtn.setColour (juce::TextButton::buttonOnColourId, muteOn); muteBtn.setColour (juce::TextButton::textColourOnId, muteText);
    soloBtn.setColour (juce::TextButton::buttonOnColourId, soloOn); soloBtn.setColour (juce::TextButton::textColourOnId, soloText);
    nameBtn.setButtonText (proc.partName (idx));
    rvOn = a.getRawParameterValue (lpid::part (idx, "rv_on"))->load() > 0.5f;
    ecOn = a.getRawParameterValue (lpid::part (idx, "ec_on"))->load() > 0.5f;
    partials = proc.partPartials (idx);
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

    // --- header: ROM (read ROMs from a folder) + ROM set picker ---
    styleButton (romBtn, true);
    romBtn.setTooltip ("Read MT-32 / CM-32L ROMs from a folder (.zip files inside are scanned too)");
    romBtn.onClick = [this] { readRomFolder(); };
    canvas.addAndMakeVisible (romBtn);
    romBox.onChange = [this]
    {
        const auto label = romBox.getText();
        if (label.isNotEmpty() && label != proc.currentRomLabel()) { proc.setRom (label); status.clear(); refreshAll(); }
    };
    canvas.addAndMakeVisible (romBox);
    canvas.addAndMakeVisible (midiLed);
    master = std::make_unique<Knob> (a, lpid::master, "MASTER", false);
    canvas.addAndMakeVisible (*master);
    helpBtn.onClick = [this] { help.setVisible (true); help.toFront (false); };
    help.onClose = [this] { help.setVisible (false); };
    canvas.addAndMakeVisible (helpBtn);
    canvas.addChildComponent (help);

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
    resetBtn.onClick = [this] { proc.requestReset(); status = "MT-32 reset"; };
    panicBtn.onClick = [this] { proc.requestPanic(); };
    for (auto* b : { &loadBtn, &resetBtn, &panicBtn }) { styleButton (*b, false); canvas.addAndMakeVisible (*b); }

    // --- strips ---
    for (int i = 0; i < mt::kParts; ++i)
    {
        strips[i] = std::make_unique<PartStrip> (*this, proc, i);
        canvas.addAndMakeVisible (*strips[i]);
    }

    // --- reverb / echo of the selected part ---
    const char* modes[4] = { "ROOM", "HALL", "PLATE", "TAP DELAY" };
    for (int i = 0; i < 4; ++i)
    {
        rvModeBtn[i].setButtonText (modes[i]);
        rvModeBtn[i].onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter (lpid::part (selected, "rv_mode"))) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        };
        canvas.addAndMakeVisible (rvModeBtn[i]);
    }
    rvOnBtn.setTooltip ("Reverb of the selected part on / off");
    ecOnBtn.setTooltip ("Echo of the selected part on / off");
    ecSyncBtn.setClickingTogglesState (true);
    ecSyncBtn.setColour (juce::TextButton::buttonOnColourId, ui::tealDark);       // green when on, like the other effect buttons
    ecSyncBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffeafff6));
    for (juce::Component* c : { (juce::Component*) &rvOnBtn, (juce::Component*) &ecOnBtn, (juce::Component*) &ecSyncBtn }) canvas.addAndMakeVisible (*c);
    bindFx();

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

void LaModuleEditor::selectPart (int p)
{
    if (p != selected) { selected = p; bindFx(); }
    refreshAll();
}

void LaModuleEditor::bindFx()
{
    auto& a = proc.apvts;
    const int p = selected;
    rvOnAtt.reset(); ecOnAtt.reset(); ecSyncAtt.reset();
    rvOnAtt   = std::make_unique<ButtonAtt> (a, lpid::part (p, "rv_on"), rvOnBtn);
    ecOnAtt   = std::make_unique<ButtonAtt> (a, lpid::part (p, "ec_on"), ecOnBtn);
    ecSyncAtt = std::make_unique<ButtonAtt> (a, lpid::part (p, "ec_sync"), ecSyncBtn);
    const juce::String dot = juce::String::fromUTF8 (" \xc2\xb7 ");
    rvOnBtn.setButtonText ("REVERB" + dot + partLabel (p));
    ecOnBtn.setButtonText ("ECHO" + dot + partLabel (p));
    rvTime  = std::make_unique<Knob> (a, lpid::part (p, "rv_time"), "TIME");
    rvLevel = std::make_unique<Knob> (a, lpid::part (p, "rv_level"), "LEVEL");
    ecDiv   = std::make_unique<Knob> (a, lpid::part (p, "ec_div"), "TIME");
    ecTime  = std::make_unique<Knob> (a, lpid::part (p, "ec_time"), "TIME");
    ecFb    = std::make_unique<Knob> (a, lpid::part (p, "ec_fb"), "FEEDBACK");
    ecLevel = std::make_unique<Knob> (a, lpid::part (p, "ec_level"), "LEVEL");
    for (auto* k : { rvTime.get(), rvLevel.get(), ecDiv.get(), ecTime.get(), ecFb.get(), ecLevel.get() }) canvas.addAndMakeVisible (*k);
    layoutFx();
}

void LaModuleEditor::layout()
{
    // header
    romBtn.setBounds (300, 25, 96, 26);
    romBox.setBounds (404, 27, 170, 22);
    midiLed.setBounds (850, 26, 16, 22);
    master->setBounds (926, 12, 52, 48);
    helpBtn.setBounds (996, 22, 24, 24);
    help.setBounds (0, 0, kW, kH);

    // LCD row (y 76..148)
    lcd.setBounds (30, 88, 420, 48);
    loadBtn.setBounds (474, 114, 110, 24);
    resetBtn.setBounds (592, 114, 84, 24);
    panicBtn.setBounds (684, 114, 84, 24);

    // strips (y 160..520)
    const int x0 = 20, w = 1000, gap = 6, sw = (w - gap * 8) / 9;
    for (int i = 0; i < mt::kParts; ++i) strips[i]->setBounds (x0 + i * (sw + gap), 160, sw, 360);

    for (int i = 0; i < 4; ++i) rvModeBtn[i].setBounds (36 + i * 82, 562, i == 3 ? 90 : 76, 24);
    ecSyncBtn.setBounds (952, 538, 56, 22);
    rvOnBtn.setBounds (32, 537, 200, 18);
    ecOnBtn.setBounds (584, 537, 200, 18);
    layoutFx();
}

void LaModuleEditor::layoutFx()
{
    if (rvTime == nullptr) return;
    rvTime->setBounds (390, 540, 70, 98);
    rvLevel->setBounds (466, 540, 70, 98);
    ecDiv->setBounds (600, 552, 90, 86);
    ecTime->setBounds (600, 552, 90, 86);
    ecFb->setBounds (720, 552, 90, 86);
    ecLevel->setBounds (840, 552, 90, 86);
}

void LaModuleEditor::paintCanvas (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff1b1f25), 0, 0, juce::Colour (0xff0d0f12), 0, (float) kH, false);
    g.setGradientFill (bg); g.fillAll();

    // title + subtitles: same place and style as the other COHESION plugins
    const auto titleFont = ui::font (18.0f, true).withExtraKerningFactor (0.14f);
    const auto subFont   = ui::font (11.0f).withExtraKerningFactor (0.10f);
    g.setColour (juce::Colours::white); g.setFont (titleFont);
    g.drawText ("COHESION - LA MODULE", 22, 26, 300, 26, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (subFont);
    g.drawText ("by Pepperwood", 22, 47, 180, 16, juce::Justification::centredLeft);
    const int subX = 22 + juce::roundToInt (titleFont.getStringWidthFloat ("COHESION - "));
    g.drawText ("MT32 Emulator", subX, 47, 160, 16, juce::Justification::centredLeft);
    g.setColour (ui::label); g.setFont (ui::font (10.0f, true));
    g.drawText ("MIDI", 806, 26, 40, 22, juce::Justification::centredRight);

    // LCD panel
    panelBox (g, { 20.0f, 76.0f, 1000.0f, 74.0f });
    g.setColour (ui::label); g.setFont (ui::font (10.0f, true));
    g.drawText (juce::String::fromUTF8 ("PARTIALS \xc2\xb7 ALL PARTS"), 474, 84, 200, 14, juce::Justification::centredLeft);
    g.setColour (partialCount > 0 ? ui::teal : ui::text); g.setFont (ui::font (12.0f, true));
    g.drawText (juce::String (partialCount), 668, 83, 100, 16, juce::Justification::centredRight);
    // bar: each part's share, scaled to 96 partials (three MT-32s' worth)
    auto bar = juce::Rectangle<float> (474.0f, 102.0f, 294.0f, 7.0f);
    g.setColour (juce::Colour (0xff13181d)); g.fillRoundedRectangle (bar, 3.5f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (bar, 3.5f); g.reduceClipRegion (clip);
        float x = bar.getX();
        for (int p = 0; p < mt::kParts; ++p)
        {
            const float w = bar.getWidth() * partialShare[p] / 96.0f;
            if (w <= 0.05f) continue;
            g.setColour ((p & 1) ? juce::Colour (0xff2fd99a) : ui::teal);
            g.fillRect (x, bar.getY(), juce::jmin (w, bar.getRight() - x), bar.getHeight());
            x += w;
            if (x >= bar.getRight()) break;
        }
    }
    g.setColour (ui::dim); g.setFont (ui::font (10.5f));
    g.drawFittedText (status, 790, 84, 222, 54, juce::Justification::centredLeft, 4);

    // reverb / echo panels (they edit the selected part)
    panelBox (g, { 20.0f, 530.0f, 540.0f, 116.0f });
    panelBox (g, { 572.0f, 530.0f, 448.0f, 116.0f });
    g.setColour (ui::dim); g.setFont (ui::font (10.0f));
    g.drawFittedText (juce::String::fromUTF8 ("MT-32 reverb of this part (its own Munt unit) \xc2\xb7 SysEx 10 00 01 sets all parts"),
                      36, 596, 340, 36, juce::Justification::centredLeft, 2);
}

void LaModuleEditor::refreshRomBox()
{
    romBox.clear (juce::dontSendNotification);
    const auto labels = proc.romLabels();
    for (int i = 0; i < labels.size(); ++i) romBox.addItem (labels[i], i + 1);
    const int cur = labels.indexOf (proc.currentRomLabel());
    if (cur >= 0) romBox.setSelectedId (cur + 1, juce::dontSendNotification);
    else romBox.setText ("No ROM set", juce::dontSendNotification);
}

void LaModuleEditor::readRomFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the folder with your MT-32 / CM-32L ROMs (.zip files inside are scanned too)", juce::File());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
    {
        const auto items = fc.getResults();
        if (items.isEmpty()) return;
        const auto r = proc.importRoms (items);
        if (r.added.empty() && r.duplicates == 0) status = "No MT-32 / CM-32L control + PCM ROM pair found in that folder";
        else status = "ROM sets added: " + juce::String ((int) r.added.size())
                    + (r.duplicates > 0 ? "  |  " + juce::String (r.duplicates) + " already present" : juce::String());
        if (! r.added.empty() && proc.currentRomLabel() != r.added.front()->label) proc.setRom (r.added.front()->label);
        refreshRomBox();
        refreshAll();
    });
}

void LaModuleEditor::showLibrary (int part)
{
    auto rom = proc.currentRom();
    if (rom == nullptr) { status = "Read your ROMs first with the ROM button"; return; }
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
    const int p = selected;
    auto& a = proc.apvts;
    const int mode = (int) a.getRawParameterValue (lpid::part (p, "rv_mode"))->load();
    for (int i = 0; i < 4; ++i) styleButton (rvModeBtn[i], mode == i);
    const bool sync = ecSyncBtn.getToggleState();
    styleButton (ecSyncBtn, sync);
    ecDiv->setVisible (sync); ecTime->setVisible (! sync);
    const float rvA = rvOnBtn.getToggleState() ? 1.0f : 0.35f, ecA = ecOnBtn.getToggleState() ? 1.0f : 0.35f;
    for (auto* c : { (juce::Component*) &rvModeBtn[0], (juce::Component*) &rvModeBtn[1], (juce::Component*) &rvModeBtn[2],
                     (juce::Component*) &rvModeBtn[3], (juce::Component*) rvTime.get(), (juce::Component*) rvLevel.get() }) c->setAlpha (rvA);
    for (auto* c : { (juce::Component*) &ecSyncBtn, (juce::Component*) ecDiv.get(), (juce::Component*) ecTime.get(),
                     (juce::Component*) ecFb.get(), (juce::Component*) ecLevel.get() }) c->setAlpha (ecA);

    if (status.isEmpty() && proc.muntStatus().isNotEmpty()) status = proc.muntStatus();
    for (int i = 0; i < mt::kParts; ++i) strips[i]->refresh (i == selected);

    // LCD: channel | program | name in fixed columns
    const int ch = (int) a.getRawParameterValue (lpid::part (p, "ch"))->load();
    const juce::String chText = (ch == 0 ? juce::String ("OFF") : "CH " + juce::String (ch)).paddedRight (' ', 5);
    const juce::String prog = p < 8 ? juce::String (proc.partProgram (p) + 1).paddedLeft ('0', 3) : juce::String ("KIT");
    if (proc.currentRom() == nullptr) lcd.line = "NO ROM LOADED";
    else if (! proc.muntReady())       lcd.line = "ROM NOT SUPPORTED";
    else                               lcd.line = chText + " | " + prog + " | " + proc.partName (p);
    lcd.repaint();
    canvas.repaint();
}

void LaModuleEditor::timerCallback()
{
    midiLed.level = juce::jmax (midiLed.level * 0.8f, proc.takeMidiActivity()); midiLed.repaint();
    partialCount = proc.activePartials();
    for (int p = 0; p < mt::kParts; ++p) partialShare[p] = partialShare[p] * 0.6f + 0.4f * (float) proc.partPartials (p);
    refreshAll();
}
