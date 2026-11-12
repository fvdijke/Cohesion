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
    juce::String dot() { return juce::String::fromUTF8 (" \xc2\xb7 "); }
    // "Essential Keys-sfzBanks" -> "EK", "GeneralUser GS" -> "GU"
    juce::String tagOf (const juce::String& path)
    {
        const auto n = juce::File (path).getFileNameWithoutExtension();
        juce::String t;
        for (auto c : n) if (juce::CharacterFunctions::isUpperCase (c)) { t << juce::String::charToString (c); if (t.length() == 2) break; }
        return t.isNotEmpty() ? t : n.substring (0, 2).toUpperCase();
    }
    juce::String bankProg (int bank, int prog) { return juce::String (bank).paddedLeft ('0', 3) + ":" + juce::String (prog).paddedLeft ('0', 3); }
}

// ===========================================================================
//  Help
// ===========================================================================
void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));
    auto panelR = getLocalBounds().toFloat().withSizeKeepingCentre (820.0f, 540.0f);
    g.setColour (ui::panel); g.fillRoundedRectangle (panelR, 10.0f);
    g.setColour (ui::teal.withAlpha (0.45f)); g.drawRoundedRectangle (panelR.reduced (0.5f), 10.0f, 1.0f);
    auto r = panelR.reduced (30.0f, 22.0f);
    g.setColour (juce::Colours::white); g.setFont (ui::font (17.0f, true).withExtraKerningFactor (0.12f));
    g.drawText ("COHESION - LA FONT", r.removeFromTop (26.0f), juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (ui::font (11.0f));
    g.drawText (juce::String::fromUTF8 ("SoundFont Player  \xc2\xb7  by Pepperwood  \xc2\xb7  click anywhere to close"), r.removeFromTop (18.0f), juce::Justification::centredLeft);
    r.removeFromTop (10.0f);
    juce::AttributedString a;
    a.setWordWrap (juce::AttributedString::byWord);
    a.setLineSpacing (2.0f);
    auto head = [&] (const char* t) { a.append (juce::String::fromUTF8 (t) + "\n", ui::font (15.0f, true), ui::teal); };
    auto body = [&] (const char* t) { a.append (juce::String::fromUTF8 (t) + "\n\n", ui::font (13.5f), ui::text); };
    head ("Getting started");
    body ("1. Click SOUNDFONT > Add SoundFont files (or a folder) to put your .sf2 files in the list. They are referenced, not copied.\n"
          "2. Select a part and choose its SoundFont and preset in the SOUND box, or click the part's name for its presets. "
          "SOUNDFONT > Use for all parts sets one SoundFont for every part.\n"
          "3. Parts 1\xe2\x80\x93" "16 listen on MIDI channels 1\xe2\x80\x93" "16 (change this per part with its CH picker); part 10 plays the drum kits (bank 128).");
    head ("Parts and effects");
    body ("Each strip has a fader, pan, mute / solo and an output: MAIN or the part's own output (OUT 2\xe2\x80\x93" "17; enable the extra outputs in your DAW). "
          "Click a strip to select it: the LCD and the SOUND, REVERB and CHORUS boxes show that part. Reverb and chorus are set per part and "
          "sent with MIDI CC 91 / 93; click a box's title to switch it on or off. Program changes, bank select and GM / GS / XG resets work as on a GM module. "
          "GM RESET resets all parts, PANIC stops all notes.");
    head ("Credits");
    body ("Sound engine: FluidSynth \xc2\xa9 the FluidSynth contributors, licensed under the GNU Lesser General Public License 2.1 or later "
          "(github.com/FluidSynth/fluidsynth). It is not part of this plugin's code: it is included as the separate library libfluidsynth-3.dll next to the plugin, "
          "with its licence text (fluidsynth-LICENSE.txt), and may be replaced by your own build of FluidSynth.\n"
          "Built with JUCE.  COHESION - LA FONT \xc2\xa9 Pepperwood. All rights reserved.");
    head ("Disclaimer");
    body ("SoundFont is a registered trademark of Creative Technology Ltd. This product is not affiliated with or endorsed by Creative. "
          "No SoundFonts are included: each SoundFont remains the work of its authors and is subject to its own licence. "
          "This software is provided \"as is\", without any warranty.");
    a.draw (g, r);
}

// ===========================================================================
//  Part strip
// ===========================================================================
PartStrip::PartStrip (LaFontEditor& e, LaFontProcessor& p, int index) : ed (e), proc (p), idx (index)
{
    auto& a = proc.apvts;
    addAndMakeVisible (led);
    for (int c = 0; c <= 16; ++c) chBox.addItem (c == 0 ? juce::String ("OFF") : "CH " + juce::String (c), c + 1);
    chAtt = std::make_unique<ComboAtt> (a, fpid::part (idx, "ch"), chBox);
    addAndMakeVisible (chBox);
    name.onClick = [this] { ed.selectPart (idx); ed.showPresetMenu (idx); };
    addAndMakeVisible (name);
    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    faderAtt = std::make_unique<SliderAtt> (a, fpid::part (idx, "lvl"), fader);
    addAndMakeVisible (fader);
    pan = std::make_unique<Knob> (a, fpid::part (idx, "pan"), "PAN", false);
    addAndMakeVisible (*pan);
    for (auto* b : { &muteBtn, &soloBtn }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
    muteAtt = std::make_unique<ButtonAtt> (a, fpid::part (idx, "mute"), muteBtn);
    soloAtt = std::make_unique<ButtonAtt> (a, fpid::part (idx, "solo"), soloBtn);
    outBox.addItem ("MAIN", 1); outBox.addItem ("OUT " + juce::String (idx + 2), 2);
    outAtt = std::make_unique<ComboAtt> (a, fpid::part (idx, "out"), outBox);
    addAndMakeVisible (outBox);
}

void PartStrip::resized()
{
    auto r = getLocalBounds().reduced (5, 7);
    auto top = r.removeFromTop (16);
    led.setBounds (top.getCentreX() - 20, top.getY(), 12, 16);
    r.removeFromTop (4);
    chBox.setBounds (r.removeFromTop (20));
    r.removeFromTop (5);
    name.setBounds (r.removeFromTop (34));
    r.removeFromTop (3);
    infoArea = r.removeFromTop (14);
    auto out = r.removeFromBottom (20); outBox.setBounds (out);
    r.removeFromBottom (5);
    auto ms = r.removeFromBottom (20);
    muteBtn.setBounds (ms.removeFromLeft (ms.getWidth() / 2).reduced (2, 0));
    soloBtn.setBounds (ms.reduced (2, 0));
    r.removeFromBottom (6);
    fxArea = r.removeFromBottom (14);
    r.removeFromBottom (6);
    r.removeFromTop (6);
    auto mid = r;
    fader.setBounds (mid.removeFromLeft (mid.getWidth() / 2).reduced (4, 0));
    pan->setBounds (mid.withSizeKeepingCentre (mid.getWidth(), 46));
}

void PartStrip::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat(), selected ? ui::teal : ui::panelEdge);
    auto r = getLocalBounds().reduced (5, 7);
    g.setColour (ui::teal); g.setFont (ui::font (14.0f, true));
    g.drawText (juce::String (idx + 1), r.removeFromTop (16).withTrimmedLeft (14), juce::Justification::centred);

    const float a = dimmed ? 0.4f : 1.0f;
    juce::AttributedString info;
    info.append (tag.isNotEmpty() ? tag + dot() : juce::String(), ui::font (10.5f), ui::label.withAlpha (a));
    info.append (juce::String (voices), ui::font (10.5f, true), (voices > 0 ? ui::teal : ui::label).withAlpha (a));
    info.append (" v", ui::font (10.5f), ui::label.withAlpha (a));
    info.setJustification (juce::Justification::centred);
    info.draw (g, infoArea.toFloat());

    auto f = fxArea;
    auto pill = [&] (juce::Rectangle<int> b, const char* t, bool on)
    {
        auto rr = b.reduced (2, 1).toFloat();
        g.setColour (on ? ui::tealDark.withAlpha (a) : ui::boxBg); g.fillRoundedRectangle (rr, 3.0f);
        g.setColour (on ? ui::teal.withAlpha (0.8f * a) : juce::Colour (0xff2b3138)); g.drawRoundedRectangle (rr, 3.0f, 1.0f);
        g.setColour (on ? juce::Colour (0xffeafff6).withAlpha (a) : ui::label.withAlpha (0.6f));
        g.setFont (ui::font (9.0f, true)); g.drawText (t, rr, juce::Justification::centred);
    };
    pill (f.removeFromLeft (f.getWidth() / 2), "REV", rvOn);
    pill (f, "CHO", choOn);
}

void PartStrip::mouseDown (const juce::MouseEvent&) { ed.selectPart (idx); }

void PartStrip::refresh (bool sel)
{
    selected = sel;
    auto& a = proc.apvts;
    bool anySolo = false;
    for (int p = 0; p < LaFontProcessor::kParts; ++p) anySolo |= a.getRawParameterValue (fpid::part (p, "solo"))->load() > 0.5f;
    const bool mute = muteBtn.getToggleState(), solo = soloBtn.getToggleState();
    dimmed = mute || (anySolo && ! solo);
    for (juce::Component* c : { (juce::Component*) &fader, (juce::Component*) pan.get() }) c->setAlpha (dimmed ? 0.3f : 1.0f);
    name.alpha = dimmed ? 0.35f : 1.0f;
    styleButton (muteBtn, mute, muteOn, muteText);
    styleButton (soloBtn, solo, soloOn, soloText);
    muteBtn.setColour (juce::TextButton::buttonOnColourId, muteOn); muteBtn.setColour (juce::TextButton::textColourOnId, muteText);
    soloBtn.setColour (juce::TextButton::buttonOnColourId, soloOn); soloBtn.setColour (juce::TextButton::textColourOnId, soloText);

    const auto font = proc.partFont (idx);
    switch (proc.partState (idx))
    {
        case FontEngine::loading:      name.text = "loading..."; break;
        case FontEngine::failedToLoad: name.text = "file not found"; break;
        case FontEngine::empty:        name.text = "-"; break;
        default:                       { auto n = proc.partPresetName (idx); name.text = n.isNotEmpty() ? n : "(no preset " + bankProg (proc.partBank (idx), proc.partProgram (idx)) + ")"; }
    }
    tag = font.isNotEmpty() ? tagOf (font) : juce::String();
    voices = proc.voices (idx);
    rvOn = a.getRawParameterValue (fpid::part (idx, "rv_on"))->load() > 0.5f;
    choOn = a.getRawParameterValue (fpid::part (idx, "cho_on"))->load() > 0.5f;
    led.level = juce::jmax (led.level * 0.82f, proc.takePartActivity (idx));
    led.repaint(); name.repaint();
    repaint();
}

// ===========================================================================
//  Editor
// ===========================================================================
LaFontEditor::LaFontEditor (LaFontProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    canvas.onPaint = [this] (juce::Graphics& g) { paintCanvas (g); };
    addAndMakeVisible (canvas);
    auto& a = proc.apvts;

    styleButton (fontBtn, true);
    fontBtn.setTooltip ("Add or remove SoundFonts");
    fontBtn.onClick = [this] { showFontMenu(); };
    canvas.addAndMakeVisible (fontBtn);
    canvas.addAndMakeVisible (midiLed);
    master = std::make_unique<Knob> (a, fpid::master, "MASTER", false);
    canvas.addAndMakeVisible (*master);
    helpBtn.onClick = [this] { showHelp(); };
    help.onClose = [this] { help.setVisible (false); };
    canvas.addAndMakeVisible (helpBtn);

    canvas.addAndMakeVisible (lcd);
    resetBtn.onClick = [this] { proc.requestReset(); status = "GM reset: every part back to program 1"; };
    panicBtn.onClick = [this] { proc.requestPanic(); };
    for (auto* b : { &resetBtn, &panicBtn }) { styleButton (*b, false); canvas.addAndMakeVisible (*b); }

    for (int i = 0; i < LaFontProcessor::kParts; ++i)
    {
        strips[i] = std::make_unique<PartStrip> (*this, proc, i);
        canvas.addAndMakeVisible (*strips[i]);
    }

    fontBox.onChange = [this]
    {
        const int i = fontBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (i, fontItems.size()) && fontItems[i] != proc.partFont (selected)) proc.setPartFont (selected, fontItems[i]);
    };
    canvas.addAndMakeVisible (fontBox);
    styleButton (presetBtn, false);
    presetBtn.onClick = [this] { showPresetMenu (selected); };
    prevBtn.onClick = [this] { proc.stepPreset (selected, -1); };
    nextBtn.onClick = [this] { proc.stepPreset (selected, 1); };
    for (auto* b : { &presetBtn, &prevBtn, &nextBtn }) canvas.addAndMakeVisible (*b);
    styleButton (prevBtn, false); styleButton (nextBtn, false);
    for (juce::Component* c : { (juce::Component*) &rvOnBtn, (juce::Component*) &choOnBtn }) canvas.addAndMakeVisible (*c);
    canvas.addChildComponent (help);
    bindFx();

    const double scale = juce::jlimit (0.6, 2.0, (double) proc.apvts.state.getProperty ("uiScale", 1.0));
    setResizable (true, true);
    setResizeLimits ((int) (kW * 0.6), (int) (kH * 0.6), kW * 2, kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (juce::roundToInt (kW * scale), juce::roundToInt (kH * scale));
    sizeRestored = true;
    canvas.setBounds (0, 0, kW, kH);
    layout();
    sendLookAndFeelChange();
    refreshFontBox();
    refreshAll();
    startTimerHz (30);
}

LaFontEditor::~LaFontEditor() { stopTimer(); setLookAndFeel (nullptr); }

void LaFontEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0b0d10)); }

void LaFontEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;
    canvas.setBounds (0, 0, kW, kH);
    canvas.setTransform (juce::AffineTransform::scale (scale));
    if (sizeRestored) proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}

void LaFontEditor::selectPart (int p)
{
    if (p != selected) { selected = p; bindFx(); refreshFontBox(); }
    refreshAll();
}

void LaFontEditor::bindFx()
{
    auto& a = proc.apvts;
    const int p = selected;
    rvOnAtt.reset(); choOnAtt.reset();
    rvOnAtt = std::make_unique<ButtonAtt> (a, fpid::part (p, "rv_on"), rvOnBtn);
    choOnAtt = std::make_unique<ButtonAtt> (a, fpid::part (p, "cho_on"), choOnBtn);
    rvOnBtn.setButtonText ("REVERB" + dot() + "PART " + juce::String (p + 1));
    choOnBtn.setButtonText ("CHORUS" + dot() + "PART " + juce::String (p + 1));
    rvRoom   = std::make_unique<Knob> (a, fpid::part (p, "rv_room"), "ROOM");
    rvDamp   = std::make_unique<Knob> (a, fpid::part (p, "rv_damp"), "DAMP");
    rvWidth  = std::make_unique<Knob> (a, fpid::part (p, "rv_width"), "WIDTH");
    rvLevel  = std::make_unique<Knob> (a, fpid::part (p, "rv_level"), "LEVEL");
    choDepth = std::make_unique<Knob> (a, fpid::part (p, "cho_depth"), "DEPTH");
    choRate  = std::make_unique<Knob> (a, fpid::part (p, "cho_rate"), "RATE");
    choLevel = std::make_unique<Knob> (a, fpid::part (p, "cho_level"), "LEVEL");
    choVoices = std::make_unique<Knob> (a, fpid::part (p, "cho_voices"), "VOICES");
    int x = 600;
    for (auto* k : { rvRoom.get(), rvDamp.get(), rvWidth.get(), rvLevel.get() }) { canvas.addAndMakeVisible (*k); k->setBounds (x, 588, 90, 100); x += 98; }
    x = 1018;
    for (auto* k : { choDepth.get(), choRate.get(), choLevel.get(), choVoices.get() }) { canvas.addAndMakeVisible (*k); k->setBounds (x, 588, 90, 100); x += 98; }
    help.toFront (false);
}

void LaFontEditor::layout()
{
    fontBtn.setBounds (340, 24, 140, 28);
    midiLed.setBounds (1300, 26, 16, 22);
    master->setBounds (1330, 10, 52, 50);
    helpBtn.setBounds (1396, 22, 24, 24);
    help.setBounds (0, 0, kW, kH);

    lcd.setBounds (30, 84, 600, 64);
    resetBtn.setBounds (660, 120, 100, 24);
    panicBtn.setBounds (768, 120, 84, 24);

    const int x0 = 20, w = 1400, gap = 4, sw = (w - gap * 15) / 16;
    for (int i = 0; i < LaFontProcessor::kParts; ++i) strips[i]->setBounds (x0 + i * (sw + gap), 168, sw, 372);

    fontBox.setBounds (36, 598, 520, 28);
    presetBtn.setBounds (36, 656, 440, 28);
    prevBtn.setBounds (482, 656, 34, 28);
    nextBtn.setBounds (522, 656, 34, 28);
    rvOnBtn.setBounds (588, 558, 260, 18);
    choOnBtn.setBounds (1006, 558, 260, 18);
}

void LaFontEditor::paintCanvas (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff1b1f25), 0, 0, juce::Colour (0xff0d0f12), 0, (float) kH, false);
    g.setGradientFill (bg); g.fillAll();

    // title + subtitles: same place and style as the other COHESION plugins
    const auto titleFont = ui::font (18.0f, true).withExtraKerningFactor (0.14f);
    g.setColour (juce::Colours::white); g.setFont (titleFont);
    g.drawText ("COHESION - LA FONT", 22, 26, 300, 26, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (ui::font (11.0f).withExtraKerningFactor (0.10f));
    g.drawText ("by Pepperwood", 22, 47, 180, 16, juce::Justification::centredLeft);
    const int subX = 22 + juce::roundToInt (titleFont.getStringWidthFloat ("COHESION - "));
    g.drawText ("SoundFont Player", subX, 47, 180, 16, juce::Justification::centredLeft);
    g.setColour (ui::label); g.setFont (ui::font (10.0f, true));
    g.drawText ("MIDI", 1256, 26, 40, 22, juce::Justification::centredRight);

    // LCD row
    panelBox (g, { 20.0f, 74.0f, 1400.0f, 84.0f });
    g.setColour (ui::label); g.setFont (ui::font (10.0f, true));
    g.drawText ("VOICES" + dot() + "ALL PARTS", 660, 84, 220, 14, juce::Justification::centredLeft);
    g.setColour (totalVoices > 0 ? ui::teal : ui::text); g.setFont (ui::font (12.0f, true));
    g.drawText (juce::String (totalVoices) + " / " + juce::String (FontEngine::kPolyphony), 960, 83, 120, 16, juce::Justification::centredRight);
    auto bar = juce::Rectangle<float> (660.0f, 103.0f, 420.0f, 7.0f);
    g.setColour (juce::Colour (0xff13181d)); g.fillRoundedRectangle (bar, 3.5f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (bar, 3.5f); g.reduceClipRegion (clip);
        float x = bar.getX();
        for (int p = 0; p < LaFontProcessor::kParts; ++p)
        {
            const float w = bar.getWidth() * voiceShare[p] / (float) FontEngine::kPolyphony;
            if (w <= 0.05f) continue;
            g.setColour ((p & 1) ? juce::Colour (0xff2fd99a) : ui::teal);
            g.fillRect (x, bar.getY(), juce::jmin (w, bar.getRight() - x), bar.getHeight());
            x += w; if (x >= bar.getRight()) break;
        }
    }
    g.setColour (ui::dim); g.setFont (ui::font (11.0f));
    g.drawFittedText (status, 1110, 84, 300, 64, juce::Justification::centredLeft, 4);

    // bottom boxes
    panelBox (g, { 20.0f, 550.0f, 552.0f, 150.0f });
    panelBox (g, { 580.0f, 550.0f, 410.0f, 150.0f });
    panelBox (g, { 998.0f, 550.0f, 422.0f, 150.0f });
    g.setColour (ui::teal); g.setFont (ui::font (10.5f, true));
    g.drawText ("PART " + juce::String (selected + 1) + dot() + "SOUND", 36, 558, 300, 18, juce::Justification::centredLeft);
    g.setColour (ui::label); g.setFont (ui::font (10.0f, true));
    g.drawText ("SOUNDFONT", 36, 582, 200, 14, juce::Justification::centredLeft);
    g.drawText ("PRESET", 36, 640, 200, 14, juce::Justification::centredLeft);
}

void LaFontEditor::refreshFontBox()
{
    fontBox.clear (juce::dontSendNotification);
    fontItems = fontlib::paths();
    for (int i = 0; i < fontItems.size(); ++i) fontBox.addItem (juce::File (fontItems[i]).getFileNameWithoutExtension(), i + 1);
    const int cur = fontItems.indexOf (proc.partFont (selected));
    if (cur >= 0) fontBox.setSelectedId (cur + 1, juce::dontSendNotification);
    else fontBox.setText (proc.partFont (selected).isNotEmpty() ? juce::File (proc.partFont (selected)).getFileNameWithoutExtension() : juce::String ("No SoundFont"),
                          juce::dontSendNotification);
}

void LaFontEditor::addFonts (bool folder)
{
    chooser = std::make_unique<juce::FileChooser> (folder ? "Choose a folder with SoundFonts (.sf2)" : "Choose SoundFont files (.sf2)",
                                                   juce::File(), "*.sf2;*.SF2", true);
    const int flags = juce::FileBrowserComponent::openMode
                    | (folder ? juce::FileBrowserComponent::canSelectDirectories
                              : juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems);
    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto items = fc.getResults();
        if (items.isEmpty()) return;
        const auto r = fontlib::add (items);
        juce::StringArray lines;
        if (! r.added.isEmpty())    lines.add ("Added: " + juce::String (r.added.size()) + " SoundFont(s)");
        if (! r.already.isEmpty())  lines.add ("Already in the list: " + r.already.joinIntoString (", "));
        if (! r.rejected.isEmpty()) lines.add ("Not added: " + r.rejected.joinIntoString ("; "));
        if (lines.isEmpty())        lines.add ("No .sf2 files found");
        status = lines.joinIntoString ("\n");
        // the first SoundFont ever: every part plays it
        bool anyFont = false;
        for (int p = 0; p < LaFontProcessor::kParts; ++p) anyFont |= proc.partFont (p).isNotEmpty();
        if (! anyFont && ! r.added.isEmpty()) proc.setFontForAllParts (r.added[0]);
        refreshFontBox(); refreshAll();
        if (! r.rejected.isEmpty())
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Add SoundFonts").withMessage (status).withButton ("OK").withAssociatedComponent (this), nullptr);
    });
}

void LaFontEditor::showFontMenu()
{
    const auto cur = proc.partFont (selected);
    const auto curName = juce::File (cur).getFileNameWithoutExtension();
    juce::PopupMenu m;
    m.addSectionHeader ("SoundFonts");
    m.addItem (1, "Add SoundFont files...");
    m.addItem (2, "Add a folder with SoundFonts...");
    m.addSeparator();
    m.addItem (3, cur.isNotEmpty() ? "Use \"" + curName + "\" for all parts" : juce::String ("Use for all parts"), cur.isNotEmpty());
    m.addItem (4, cur.isNotEmpty() ? "Remove \"" + curName + "\" from the list" : juce::String ("Remove from the list"), cur.isNotEmpty() && fontlib::paths().contains (cur));
    m.addSeparator();
    m.addItem (5, "Show the SoundFont's folder", cur.isNotEmpty());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fontBtn), [this, cur, curName] (int r)
    {
        if (r == 1) addFonts (false);
        if (r == 2) addFonts (true);
        if (r == 3) { proc.setFontForAllParts (cur); status = "All parts: " + curName; refreshAll(); }
        if (r == 4) { fontlib::remove (cur); status = curName + " removed from the list (the file is not touched; parts using it keep it)"; refreshFontBox(); }
        if (r == 5) juce::File (cur).revealToUser();
    });
}

void LaFontEditor::showPresetMenu (int part)
{
    auto info = proc.partInfo (part);
    if (info == nullptr || ! info->ok()) { status = "Choose a SoundFont for this part first (SOUND box or SOUNDFONT button)"; return; }
    juce::PopupMenu menu;
    menu.addSectionHeader ("Part " + juce::String (part + 1) + dot() + info->shortName());
    std::map<int, juce::PopupMenu> banks;
    for (size_t i = 0; i < info->presets.size(); ++i)
    {
        const auto& pr = info->presets[i];
        banks[pr.bank].addItem ((int) i + 1, juce::String (pr.program + 1).paddedLeft ('0', 3) + "  " + pr.name, true,
                                pr.bank == proc.partBank (part) && pr.program == proc.partProgram (part));
    }
    if (banks.size() == 1) menu = banks.begin()->second;
    else for (auto& [b, sub] : banks) menu.addSubMenu (b == 128 ? juce::String ("Bank 128 (drum kits)") : "Bank " + juce::String (b), sub);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (strips[part].get()), [this, part, info] (int r)
    {
        if (r > 0) { const auto& pr = info->presets[(size_t) r - 1]; proc.selectPreset (part, pr.bank, pr.program); }
    });
}

void LaFontEditor::refreshAll()
{
    const int p = selected;
    const float rvA = rvOnBtn.getToggleState() ? 1.0f : 0.35f, choA = choOnBtn.getToggleState() ? 1.0f : 0.35f;
    for (auto* k : { rvRoom.get(), rvDamp.get(), rvWidth.get(), rvLevel.get() }) k->setAlpha (rvA);
    for (auto* k : { choDepth.get(), choRate.get(), choLevel.get(), choVoices.get() }) k->setAlpha (choA);
    for (int i = 0; i < LaFontProcessor::kParts; ++i) strips[i]->refresh (i == selected);

    if (! LaFontProcessor::engineAvailable() && status.isEmpty()) status = "FluidSynth not available: " + LaFontProcessor::engineStatus();
    const int ch = (int) proc.apvts.getRawParameterValue (fpid::part (p, "ch"))->load();
    const auto chText = (ch == 0 ? juce::String ("OFF") : "CH " + juce::String (ch)).paddedRight (' ', 5);
    const auto font = proc.partFont (p);
    const auto state = proc.partState (p);
    if (font.isEmpty()) { lcd.line1 = "NO SOUNDFONT LOADED"; lcd.line2 = fontlib::paths().isEmpty() ? "SOUNDFONT > Add SoundFont files" : "choose one in the SOUND box"; }
    else
    {
        lcd.line1 = chText + " | " + bankProg (proc.partBank (p), proc.partProgram (p)) + " | " + proc.partPresetName (p);
        lcd.line2 = juce::File (font).getFileNameWithoutExtension()
                  + (state == FontEngine::loading ? "   (loading...)" : state == FontEngine::failedToLoad ? "   (file not found)" : juce::String());
    }
    lcd.repaint();
    presetBtn.setButtonText (font.isNotEmpty() ? bankProg (proc.partBank (p), proc.partProgram (p)) + "  " + proc.partPresetName (p) : juce::String ("-"));
    canvas.repaint();
}

void LaFontEditor::timerCallback()
{
    midiLed.level = juce::jmax (midiLed.level * 0.8f, proc.takeMidiActivity()); midiLed.repaint();
    totalVoices = proc.totalVoices();
    for (int p = 0; p < LaFontProcessor::kParts; ++p) voiceShare[p] = voiceShare[p] * 0.6f + 0.4f * (float) proc.voices (p);
    refreshAll();
}
