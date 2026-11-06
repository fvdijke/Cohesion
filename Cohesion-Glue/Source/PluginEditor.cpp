#include "PluginEditor.h"

CohesionAudioProcessorEditor::CohesionAudioProcessorEditor (CohesionAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    // ---- title / subtitle ----
    title.setText ("COHESION - GLUE", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setFont (juce::Font ("Segoe UI", 18.0f, juce::Font::bold).withExtraKerningFactor (0.14f));
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible (title);

    subtitle.setText ("by Pepperwood", juce::dontSendNotification);
    subtitle.setJustificationType (juce::Justification::centredLeft);
    subtitle.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain).withExtraKerningFactor (0.10f));
    subtitle.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    content.addAndMakeVisible (subtitle);

    // ---- preset menu + nav + A/B + save ----
    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centred);
    presetLabel.setFont (juce::Font ("Segoe UI", 10.5f, juce::Font::plain).withExtraKerningFactor (0.10f));
    presetLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
    content.addAndMakeVisible (presetLabel);

    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1000)
        {
            const int ui = id - 1000;
            if (juce::isPositiveAndBelow (ui, (int) userFiles.size()))
                if (auto xml = juce::XmlDocument::parse (userFiles[(size_t) ui]))
                    if (xml->hasTagName (proc.apvts.state.getType()))
                        proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
        else if (id >= 1)
        {
            proc.setCurrentProgram (id - 1);
        }
    };
    content.addAndMakeVisible (presetBox);

    auto smallBtn = [this] (juce::TextButton& b, const juce::String& tip)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fe1cb));
        b.setTooltip (tip);
        content.addAndMakeVisible (b);
    };
    smallBtn (prevBtn, "Vorige preset");
    smallBtn (nextBtn, "Volgende preset");
    smallBtn (abBtn,   "A/B: wissel tussen twee instellingen");
    smallBtn (saveBtn, "Huidige instellingen als preset opslaan");

    prevBtn.onClick = [this] { selectProgram (proc.getCurrentProgram() - 1); };
    nextBtn.onClick = [this] { selectProgram (proc.getCurrentProgram() + 1); };

    abBtn.onClick = [this]
    {
        if (! onB) { stateA = proc.apvts.copyState(); onB = true;  proc.apvts.replaceState (stateB.createCopy()); abBtn.setButtonText ("B"); }
        else       { stateB = proc.apvts.copyState(); onB = false; proc.apvts.replaceState (stateA.createCopy()); abBtn.setButtonText ("A"); }
    };

    saveBtn.onClick = [this]
    {
        auto dir = userPresetDir();
        dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Bewaar Cohesion preset",
                                                       dir.getChildFile ("My Preset.cohesion"), "*.cohesion");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                f = f.withFileExtension ("cohesion");
                if (auto xml = proc.apvts.copyState().createXml())
                    xml->writeTo (f);
                refreshUserPresets();
            });
    };

    // ---- MAGIC + Amount ----
    magic.setTooltip ("Schakel de hele Cohesion-keten in of uit");
    content.addAndMakeVisible (magic);
    buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, "magic", magic));

    content.addAndMakeVisible (amount);
    amount.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    amount.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    amount.setTooltip ("Macro: duwt compressie, makeup, low en air tegelijk omhoog");
    sliderAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, "amount", amount));
    amountLabel.setText ("AMOUNT", juce::dontSendNotification);
    amountLabel.setJustificationType (juce::Justification::centred);
    amountLabel.setFont (9.5f);
    amountLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
    content.addAndMakeVisible (amountLabel);

    // ---- readouts + trims ----
    content.addAndMakeVisible (inReadout);
    content.addAndMakeVisible (grReadout);
    content.addAndMakeVisible (outReadout);
    setupTrim (inTrim,  inTrimL,  "inputGain",  "Input",  "Ingangsniveau vóór de keten");
    setupTrim (outTrim, outTrimL, "outputGain", "Output", "Uitgangsniveau na de limiter");

    // ---- display + meters ----
    content.addAndMakeVisible (curve);
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);

    // ---- group headers ----
    auto makeHeader = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (juce::Font ("Segoe UI", 10.5f, juce::Font::bold).withExtraKerningFactor (0.14f));
        l.setColour (juce::Label::textColourId, juce::Colour (0xff5dcaa5));
        content.addAndMakeVisible (l);
    };
    makeHeader (compHeader, "COMPRESSOR");
    makeHeader (eqHeader,   "EQ");
    makeHeader (outHeader,  "OUTPUT");
    makeHeader (scHeader,   "SIDECHAIN");

    // ---- compressor ----
    setupFader (threshold, thresholdL, "threshold", "Threshold", " dB", "Niveau waarboven de compressor ingrijpt");
    setupFader (makeup,    makeupL,    "makeup",    "Makeup",    " dB", "Compenseer het volumeverlies na compressie");
    setupCombo (ratio,   ratioL,   "ratio",   { "2:1", "4:1", "10:1" }, "Ratio",   "Hoe hard er boven de threshold wordt gecomprimeerd");
    setupCombo (attack,  attackL,  "attack",  { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, "Attack", "Hoe snel de compressie inzet");
    setupCombo (release, releaseL, "release", { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, "Release", "Hoe snel de compressie loslaat");
    setupCombo (knee,    kneeL,    "knee",    { "Soft", "Hard" }, "Knee", "Soft = zachte, muzikale inzet; Hard = direct/punchy");

    // ---- EQ ----
    setupFader (hpf, hpfL, "hpf", "HPF", " Hz", "Hoogdoorlaatfilter: ruimt sub/rommel onderin op");
    setupFader (low, lowL, "low", "Low", " dB", "Lage shelf (120 Hz): gewicht/bas");
    setupFader (air, airL, "air", "Air", " dB", "Hoge shelf (9 kHz): lucht/glans");
    setupFader (lpf, lpfL, "lpf", "LPF", " Hz", "Laagdoorlaatfilter: temt de top");

    // ---- output ----
    setupFader (mix,     mixL,     "mix",     "Mix",     " %",  "Parallel-/NY-compressie: meng droog en bewerkt");
    setupFader (ceiling, ceilingL, "ceiling", "Ceiling", " dB", "Maximaal uitgangsniveau (limiter-plafond)");

    // ---- sidechain ----
    scOn.setTooltip ("Hoogdoorlaat op het detectiesignaal: bas pompt de comp niet");
    content.addAndMakeVisible (scOn);
    buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, "scOn", scOn));
    scListen.setTooltip ("Luister naar het detectiesignaal dat de comp 'ziet'");
    content.addAndMakeVisible (scListen);
    buttonAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, "scListen", scListen));
    setupFader (scFreq, scFreqL, "scFreq", "Freq", " Hz", "Frequentie van de sidechain-hoogdoorlaat");

    // ---- help ----
    helpButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    helpButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fe1cb));
    helpButton.setTooltip ("Uitleg en tips");
    helpButton.onClick = [this] { helpView.setVisible (true); helpView.toFront (true); };
    content.addAndMakeVisible (helpButton);

    helpView.setVisible (false);
    content.addChildComponent (helpView);

    // ---- tooltips on/off (persisted in plugin state) ----
    const bool tipsOn = (bool) proc.apvts.state.getProperty ("tooltipsOn", true);
    tipsToggle.setToggleState (tipsOn, juce::dontSendNotification);
    if (tipsOn) tooltip = std::make_unique<juce::TooltipWindow> (this, 500);
    tipsToggle.setTooltip ("Tooltips aan/uit");
    tipsToggle.onClick = [this]
    {
        const bool on = tipsToggle.getToggleState();
        proc.apvts.state.setProperty ("tooltipsOn", on, nullptr);
        if (on && tooltip == nullptr) tooltip = std::make_unique<juce::TooltipWindow> (this, 500);
        else if (! on)               tooltip.reset();
    };
    content.addAndMakeVisible (tipsToggle);

    // A/B slots start equal to the current state
    stateA = proc.apvts.copyState();
    stateB = stateA.createCopy();

    refreshUserPresets();

    setResizable (true, true);
    constrainer.setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    constrainer.setSizeLimits ((int) (kBaseW * 0.6), (int) (kBaseH * 0.6),
                               (int) (kBaseW * 1.6), (int) (kBaseH * 1.6));
    setConstrainer (&constrainer);
    setSize (kBaseW, kBaseH);
}

CohesionAudioProcessorEditor::~CohesionAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

juce::File CohesionAudioProcessorEditor::userPresetDir() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Pepperwood").getChildFile ("Cohesion").getChildFile ("Presets");
}

void CohesionAudioProcessorEditor::selectProgram (int idx)
{
    idx = juce::jlimit (0, proc.getNumPrograms() - 1, idx);
    proc.setCurrentProgram (idx);
    presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
}

void CohesionAudioProcessorEditor::refreshUserPresets()
{
    const int keepId = presetBox.getSelectedId();
    presetBox.clear (juce::dontSendNotification);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);

    userFiles.clear();
    auto dir = userPresetDir();
    if (dir.isDirectory())
    {
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.cohesion"))
            userFiles.push_back (f);
    }
    if (! userFiles.empty())
    {
        presetBox.addSeparator();
        for (int i = 0; i < (int) userFiles.size(); ++i)
            presetBox.addItem (userFiles[(size_t) i].getFileNameWithoutExtension(), 1000 + i);
    }

    if (keepId > 0) presetBox.setSelectedId (keepId, juce::dontSendNotification);
    else            presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void CohesionAudioProcessorEditor::setupFader (juce::Slider& s, juce::Label& l, const juce::String& id,
                                               const juce::String& text, const juce::String& suffix, const juce::String& tip)
{
    content.addAndMakeVisible (s);                 // parent first so the text box picks up our LookAndFeel
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);
    s.setTextValueSuffix (suffix);
    s.setTooltip (tip);
    sliderAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, id, s));

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centredLeft);
    l.setFont (11.0f);
    l.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
    content.addAndMakeVisible (l);
}

void CohesionAudioProcessorEditor::setupCombo (juce::ComboBox& c, juce::Label& l, const juce::String& id,
                                               const juce::StringArray& items, const juce::String& text, const juce::String& tip)
{
    c.addItemList (items, 1);
    c.setTooltip (tip);
    content.addAndMakeVisible (c);
    comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, id, c));

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centredLeft);
    l.setFont (11.0f);
    l.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
    content.addAndMakeVisible (l);
}

void CohesionAudioProcessorEditor::setupTrim (juce::Slider& s, juce::Label& l, const juce::String& id,
                                              const juce::String& text, const juce::String& tip)
{
    content.addAndMakeVisible (s);                 // parent first (text box inherits our LookAndFeel)
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 12);
    s.setTextValueSuffix (" dB");
    s.setTooltip (tip);
    sliderAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, id, s));

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (9.0f);
    l.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
    content.addAndMakeVisible (l);
}

void CohesionAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff2b2f36), 0, 0,
                             juce::Colour (0xff15171b), 0, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (6.0f), 8.0f, 1.5f);
}

void CohesionAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseW;
    content.setTransform ({});
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (juce::AffineTransform::scale (s));
    layoutContent();
}

void CohesionAudioProcessorEditor::layoutContent()
{
    auto b = juce::Rectangle<int> (0, 0, kBaseW, kBaseH).reduced (14);
    const int centerX = kBaseW / 2;

    // ---- header ----
    auto header = b.removeFromTop (72);
    title.setBounds    (header.getX(), header.getY() + 8,  220, 26);
    subtitle.setBounds (header.getX(), header.getY() + 29, 160, 16);

    // right: MAGIC + Amount
    auto rc = header.removeFromRight (230);
    auto amt = rc.removeFromRight (68);
    amountLabel.setBounds (amt.getX(), amt.getY() + 6, amt.getWidth(), 12);
    amount.setBounds (amt.getX() + 6, amt.getY() + 18, 56, 50);
    rc.removeFromRight (8);
    magic.setBounds (rc.getRight() - 130, rc.getCentreY() - 22, 130, 44);

    // center: preset group  [ < | box | > ]  A/B  Save
    const int boxW = 180, navW = 26, abW = 34, saveW = 56;
    const int groupW = navW + 4 + boxW + 4 + navW + 10 + abW + 6 + saveW;
    int gx = centerX - groupW / 2;
    const int rowY = header.getY() + 22, rowH = 30;
    presetLabel.setBounds (gx + navW + 4, header.getY() + 4, boxW, 12);
    prevBtn.setBounds (gx, rowY, navW, rowH);                 gx += navW + 4;
    presetBox.setBounds (gx, rowY, boxW, rowH);               gx += boxW + 4;
    nextBtn.setBounds (gx, rowY, navW, rowH);                 gx += navW + 10;
    abBtn.setBounds (gx, rowY, abW, rowH);                    gx += abW + 6;
    saveBtn.setBounds (gx, rowY, saveW, rowH);

    b.removeFromTop (4);

    // ---- readouts + trims ----
    auto rds = b.removeFromTop (56);
    const int third = rds.getWidth() / 3;
    auto c1 = rds.removeFromLeft (third);
    auto c2 = rds.removeFromLeft (third);
    auto c3 = rds;

    auto inTrimCell = c1.removeFromRight (60);
    inReadout.setBounds (c1.reduced (2, 0));
    inTrimL.setBounds (inTrimCell.getX(), inTrimCell.getY(), 60, 12);
    inTrim.setBounds (inTrimCell.getX() + 5, inTrimCell.getY() + 12, 50, 44);

    grReadout.setBounds (c2.reduced (2, 0));

    auto outTrimCell = c3.removeFromRight (60);
    outReadout.setBounds (c3.reduced (2, 0));
    outTrimL.setBounds (outTrimCell.getX(), outTrimCell.getY(), 60, 12);
    outTrim.setBounds (outTrimCell.getX() + 5, outTrimCell.getY() + 12, 50, 44);

    b.removeFromTop (6);

    // ---- display + meters ----
    auto disp = b.removeFromTop (248);
    auto meters = disp.removeFromRight (74);
    inMeter.setBounds  (meters.removeFromLeft (34));
    meters.removeFromLeft (6);
    outMeter.setBounds (meters.removeFromLeft (34));
    curve.setBounds (disp.withTrimmedRight (8));

    b.removeFromTop (10);

    // ---- footer strip: help + tips (kept clear of the controls) ----
    auto footer = b.removeFromBottom (26);
    helpButton.setBounds (footer.getRight() - 26, footer.getY(), 26, 26);
    tipsToggle.setBounds (footer.getRight() - 26 - 8 - 66, footer.getY() + 1, 66, 24);
    b.removeFromBottom (6);

    // ---- bottom: three grouped columns ----
    auto bottom = b;
    const int colW = bottom.getWidth() / 3;
    auto colA = bottom.removeFromLeft (colW).reduced (4, 0);
    auto colB = bottom.removeFromLeft (colW).reduced (10, 0);
    auto colC = bottom.reduced (6, 0);

    auto faderRow = [] (juce::Rectangle<int>& col, juce::Label& l, juce::Slider& sl)
    {
        auto cell = col.removeFromTop (34);
        l.setBounds (cell.removeFromTop (14));
        sl.setBounds (cell);
    };
    auto comboInCell = [] (juce::Rectangle<int> cell, juce::Label& l, juce::ComboBox& c)
    {
        l.setBounds (cell.removeFromTop (14));
        c.setBounds (cell.reduced (0, 1));
    };
    auto comboPair = [&] (juce::Rectangle<int> row, juce::Label& la, juce::ComboBox& ca,
                          juce::Label& lb, juce::ComboBox& cb)
    {
        const int half = (row.getWidth() - 12) / 2;
        comboInCell (row.removeFromLeft (half), la, ca);
        comboInCell (row.removeFromRight (half), lb, cb);
    };

    // Column A - COMPRESSOR
    compHeader.setBounds (colA.removeFromTop (16));
    colA.removeFromTop (2);
    faderRow (colA, thresholdL, threshold);
    faderRow (colA, makeupL,    makeup);
    colA.removeFromTop (4);
    comboPair (colA.removeFromTop (40), ratioL, ratio, attackL, attack);
    colA.removeFromTop (10);
    comboPair (colA.removeFromTop (40), releaseL, release, kneeL, knee);

    // Column B - EQ
    eqHeader.setBounds (colB.removeFromTop (16));
    colB.removeFromTop (2);
    faderRow (colB, hpfL, hpf);
    faderRow (colB, lowL, low);
    faderRow (colB, airL, air);
    faderRow (colB, lpfL, lpf);

    // Column C - OUTPUT + SIDECHAIN
    outHeader.setBounds (colC.removeFromTop (16));
    colC.removeFromTop (2);
    faderRow (colC, mixL,     mix);
    faderRow (colC, ceilingL, ceiling);
    colC.removeFromTop (6);
    scHeader.setBounds (colC.removeFromTop (16));
    colC.removeFromTop (2);
    {
        auto btnRow = colC.removeFromTop (30);
        const int bhalf = (btnRow.getWidth() - 8) / 2;
        scOn.setBounds     (btnRow.removeFromLeft (bhalf));
        scListen.setBounds (btnRow.removeFromRight (bhalf));
    }
    colC.removeFromTop (6);
    faderRow (colC, scFreqL, scFreq);

    // ---- overlay (base coords) ----
    helpView.setBounds (0, 0, kBaseW, kBaseH);
}
