#pragma once

// ===========================================================================
//  MIXER tab: the three parts of a multi sound (VA, FM, LA) side by side.
//  Per part: what is loaded (+ LOAD into this part), level fader with meter,
//  pan, ECHO / REVERB sends, key range, ARP on/off, FM / LA through the VA
//  filters, MUTE / SOLO. On the right the send effects' returns and the master.
//  (Included from PluginEditor.h after LabeledKnob and the attachment aliases.)
// ===========================================================================

inline void styleToggle (juce::TextButton& b, juce::Colour on)
{
    b.setClickingTogglesState (true);
    b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    b.setColour (juce::TextButton::buttonOnColourId, on);
    b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fb0aa));
    b.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff0b0d10));
}

// vertical peak meter, -60 .. +6 dB
struct PeakMeter : juce::Component
{
    float level = 0.0f, hold = 0.0f; int holdAge = 0;
    void push (float pk)
    {
        level = juce::jmax (pk, level * 0.80f);
        if (pk >= hold) { hold = pk; holdAge = 0; } else if (++holdAge > 30) hold *= 0.92f;
        repaint();
    }
    static float pos (float g) { const float db = juce::Decibels::gainToDecibels (g, -60.0f); return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a0c0f)); g.fillRoundedRectangle (r, 2.0f);
        const float h = r.getHeight() * pos (level);
        const auto bar = r.withTop (r.getBottom() - h).reduced (1.0f, 0.0f);
        juce::ColourGradient cg (juce::Colour (0xff39ffb0), 0, r.getBottom(), juce::Colour (0xffffc04a), 0, r.getY(), false);
        cg.addColour (pos (juce::Decibels::decibelsToGain (-6.0f)), juce::Colour (0xff39ffb0));
        g.setGradientFill (cg); g.fillRect (bar);
        const float hy = r.getBottom() - r.getHeight() * pos (hold);
        g.setColour (hold > 1.0f ? juce::Colour (0xffff5a4a) : juce::Colour (0xffeafff6).withAlpha (0.8f));
        g.fillRect (r.getX() + 1.0f, hy, r.getWidth() - 2.0f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));                  // 0 dB mark
        g.fillRect (r.getX(), r.getBottom() - r.getHeight() * pos (1.0f), r.getWidth(), 1.0f);
    }
};

struct PartStrip : juce::Component
{
    CohesionSynthAudioProcessor& proc;
    const int part;
    std::function<void (int)> onLoad;

    juce::TextButton loadBtn { "LOAD..." }, onBtn { "ON" }, muteBtn { "MUTE" }, soloBtn { "SOLO" },
                     arpBtn { "ARP" }, filtBtn { "VA FILT+MOD" };
    std::unique_ptr<ButtonAtt> onAtt, muteAtt, soloAtt, arpAtt, filtAtt;
    std::unique_ptr<LabeledKnob> echo, reverb, pan, keyLo, keyHi;
    juce::Slider fader;
    std::unique_ptr<SliderAtt> faderAtt;
    PeakMeter meter;
    juce::String soundName;

    PartStrip (CohesionSynthAudioProcessor& p, int k) : proc (p), part (k)
    {
        auto& s = proc.apvts;
        loadBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0f6e56));
        loadBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeafff6));
        loadBtn.setTooltip ("Load a sound into the " + pid::partNames[k] + " part only");
        loadBtn.onClick = [this] { if (onLoad) onLoad (part); };
        addAndMakeVisible (loadBtn);

        styleToggle (onBtn, juce::Colour (0xff39ffb0));
        onAtt = std::make_unique<ButtonAtt> (s, part == pid::kVA ? pid::vaOn : part == pid::kFM ? pid::dxOn : pid::laOn, onBtn);
        addAndMakeVisible (onBtn);
        styleToggle (filtBtn, juce::Colour (0xff5dcaa5));
        filtBtn.setTooltip (part == pid::kVA
            ? "VA part through its filters and modulation matrix (FILTER / MOD tab). Off: the oscillators go dry (AMP envelope only)"
            : "Run this part through the VA part's filters and modulation matrix (pitch, amp, pan; FILTER / MOD tab)");
        filtAtt = std::make_unique<ButtonAtt> (s, part == pid::kVA ? juce::String (pid::vaFiltMod) : pid::partFilter (part), filtBtn);
        addAndMakeVisible (filtBtn);
        styleToggle (muteBtn, juce::Colour (0xffff7a5a));
        styleToggle (soloBtn, juce::Colour (0xffffc04a));
        styleToggle (arpBtn,  juce::Colour (0xff5dcaa5));
        arpBtn.setTooltip ("On: this part plays the ARP's notes (when the ARP is on). Off: it plays the keys you press");
        muteAtt = std::make_unique<ButtonAtt> (s, pid::partMute (part), muteBtn);
        soloAtt = std::make_unique<ButtonAtt> (s, pid::partSolo (part), soloBtn);
        arpAtt  = std::make_unique<ButtonAtt> (s, pid::partArp (part), arpBtn);
        for (auto* b : { &muteBtn, &soloBtn, &arpBtn }) addAndMakeVisible (*b);

        auto knob = [&] (std::unique_ptr<LabeledKnob>& kn, const juce::String& id, const juce::String& nm)
        { kn = std::make_unique<LabeledKnob> (s, id, nm); addAndMakeVisible (*kn); };
        knob (echo,   pid::partEcho (part),   "ECHO SEND");
        knob (reverb, pid::partReverb (part), "REVERB SEND");
        knob (pan,    pid::partPan (part),    "PAN");
        knob (keyLo,  pid::partKeyLo (part),  "KEY LOW");
        knob (keyHi,  pid::partKeyHi (part),  "KEY HIGH");

        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (fader);
        faderAtt = std::make_unique<SliderAtt> (s, pid::partLevel (part), fader);
        addAndMakeVisible (meter);
    }

    void refresh()
    {
        auto nm = proc.partSoundName (part);
        if (nm != soundName) { soundName = nm; repaint(); }
        meter.push (proc.takePartPeak (part));
        repaint (faderValueArea());
    }

    juce::Rectangle<int> faderValueArea() const { return { fader.getX() - 20, fader.getBottom() + 2, fader.getWidth() + 60, 16 }; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (juce::Colour (0xffe8eef0));
        g.setFont (juce::Font ("Segoe UI", 14.0f, juce::Font::bold));
        g.drawText (pid::partNames[part] + " PART", 14, 10, 140, 20, juce::Justification::centredLeft);

        // what is loaded in this part
        auto name = soundName.isNotEmpty() ? soundName : juce::String ("(nothing loaded)");
        g.setColour (soundName.isNotEmpty() ? juce::Colour (0xff9fe1cb) : juce::Colour (0xff5c6a64));
        g.setFont (juce::Font ("Segoe UI", 12.5f, juce::Font::plain));
        g.drawFittedText (name, 14, 36, getWidth() - 28, 32, juce::Justification::centredLeft, 2);

        g.setColour (juce::Colour (0xff9fb0aa));
        g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText ("LEVEL", fader.getX() - 10, fader.getY() - 14, fader.getWidth() + 20, 12, juce::Justification::centred);
        g.setColour (juce::Colour (0xffdfe6e2));
        g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        if (auto* prm = proc.apvts.getParameter (pid::partLevel (part)))
            g.drawText (prm->getCurrentValueAsText(), faderValueArea(), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (22);
        onBtn.setBounds (top.removeFromRight (46));
        r.removeFromTop (38);                              // the sound name
        loadBtn.setBounds (r.removeFromTop (26));
        r.removeFromTop (8);
        auto row1 = r.removeFromTop (74);
        const int kw = row1.getWidth() / 3;
        echo->setBounds (row1.removeFromLeft (kw)); reverb->setBounds (row1.removeFromLeft (kw)); pan->setBounds (row1);
        auto row2 = r.removeFromTop (74);
        keyLo->setBounds (row2.removeFromLeft (kw)); keyHi->setBounds (row2.removeFromLeft (kw));
        r.removeFromTop (6);
        auto row3 = r.removeFromTop (26);
        arpBtn.setBounds (row3.removeFromLeft (row3.getWidth() / 2 - 4)); row3.removeFromLeft (8); filtBtn.setBounds (row3);
        auto bottom = r.removeFromBottom (26);
        muteBtn.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 4)); bottom.removeFromLeft (8); soloBtn.setBounds (bottom);
        r.removeFromBottom (24);                           // the level value
        r.removeFromTop (20);                              // "LEVEL"
        auto fa = r.withSizeKeepingCentre (64, r.getHeight());
        fader.setBounds (fa.removeFromLeft (40));
        fa.removeFromLeft (8);
        meter.setBounds (fa.removeFromLeft (10).reduced (0, 6));
    }
};

struct MixerTab : juce::Component, private juce::Timer
{
    CohesionSynthAudioProcessor& proc;
    std::unique_ptr<PartStrip> strips[pid::kNumParts];
    juce::TextButton echoOn { "ON" }, revOn { "ON" }, limOn { "ON" };
    std::unique_ptr<ButtonAtt> echoOnAtt, revOnAtt, limOnAtt;
    std::unique_ptr<LabeledKnob> echoMix, revMix, mainK, limDrive;
    PeakMeter outMeter;
    std::function<void (int)> onLoad;   // LOAD... of a strip: open the LIBRARY for that part

    explicit MixerTab (CohesionSynthAudioProcessor& p) : proc (p)
    {
        for (int k = 0; k < pid::kNumParts; ++k)
        {
            strips[k] = std::make_unique<PartStrip> (proc, k);
            strips[k]->onLoad = [this] (int part) { if (onLoad) onLoad (part); };
            addAndMakeVisible (*strips[k]);
        }
        auto& s = proc.apvts;
        styleToggle (echoOn, juce::Colour (0xff39ffb0)); styleToggle (revOn, juce::Colour (0xff39ffb0)); styleToggle (limOn, juce::Colour (0xff39ffb0));
        echoOnAtt = std::make_unique<ButtonAtt> (s, pid::ecOn, echoOn);
        revOnAtt  = std::make_unique<ButtonAtt> (s, pid::rvOn, revOn);
        limOnAtt  = std::make_unique<ButtonAtt> (s, pid::limOn, limOn);
        for (auto* b : { &echoOn, &revOn, &limOn }) addAndMakeVisible (*b);
        echoMix  = std::make_unique<LabeledKnob> (s, pid::ecMix, "RETURN");
        revMix   = std::make_unique<LabeledKnob> (s, pid::rvMix, "RETURN");
        mainK    = std::make_unique<LabeledKnob> (s, pid::mainVol, "MAIN");
        limDrive = std::make_unique<LabeledKnob> (s, pid::limDrive, "DRIVE");
        for (auto* k : { echoMix.get(), revMix.get(), mainK.get(), limDrive.get() }) addAndMakeVisible (*k);
        addAndMakeVisible (outMeter);
        startTimerHz (30);
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        for (auto& st : strips) st->refresh();
        outMeter.push (proc.getOutputPeak());
    }
    void visibilityChanged() override { if (isVisible()) for (auto& st : strips) st->refresh(); }

    juce::Rectangle<int> fxArea, masterArea;

    void paint (juce::Graphics& g) override
    {
        auto panel = [&g] (juce::Rectangle<int> a, const juce::String& title)
        {
            g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (a.toFloat(), 7.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (a.toFloat(), 7.0f, 1.0f);
            g.setColour (juce::Colour (0xffe8eef0));
            g.setFont (juce::Font ("Segoe UI", 14.0f, juce::Font::bold));
            g.drawText (title, a.getX() + 14, a.getY() + 10, 200, 20, juce::Justification::centredLeft);
        };
        panel (fxArea, "SEND FX");
        panel (masterArea, "MASTER");
        g.setColour (juce::Colour (0xffdfe6e2));
        g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
        g.drawText ("ECHO",    echoOn.getBounds().withX (fxArea.getX() + 14).withWidth (70), juce::Justification::centredLeft);
        g.drawText ("REVERB",  revOn.getBounds().withX (fxArea.getX() + 14).withWidth (70), juce::Justification::centredLeft);
        g.drawText ("LIMITER", limOn.getBounds().withX (masterArea.getX() + 14).withWidth (70), juce::Justification::centredLeft);
        g.drawText ("OUTPUT",  mainK->getBounds().withX (masterArea.getX() + 14).withWidth (60), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff6c7a74));
        g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::plain));
        g.drawFittedText ("The parts' sends feed ECHO and REVERB (settings on the FX tab). COMP, FLANGER and PHASER run on every part.",
                          fxArea.getX() + 14, fxArea.getBottom() - 44, fxArea.getWidth() - 28, 36, juce::Justification::centredLeft, 3);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto right = r.removeFromRight (230);
        r.removeFromRight (12);
        const int sw = (r.getWidth() - 2 * 12) / 3;
        for (int k = 0; k < pid::kNumParts; ++k) { strips[k]->setBounds (r.removeFromLeft (sw)); r.removeFromLeft (12); }

        fxArea = right.removeFromTop (right.getHeight() / 2 - 6);
        right.removeFromTop (12);
        masterArea = right;

        // one row per send effect: name, RETURN knob, ON
        auto f = fxArea.reduced (14); f.removeFromTop (30);
        auto rowE = f.removeFromTop (74);
        echoOn.setBounds (rowE.removeFromRight (46).withSizeKeepingCentre (46, 26));
        rowE.removeFromLeft (66); echoMix->setBounds (rowE.withSizeKeepingCentre (84, 74));
        f.removeFromTop (6);
        auto rowR = f.removeFromTop (74);
        revOn.setBounds (rowR.removeFromRight (46).withSizeKeepingCentre (46, 26));
        rowR.removeFromLeft (66); revMix->setBounds (rowR.withSizeKeepingCentre (84, 74));

        // master: MAIN + output meter, limiter row
        auto m = masterArea.reduced (14); m.removeFromTop (30);
        auto rowL = m.removeFromTop (74);
        limOn.setBounds (rowL.removeFromRight (46).withSizeKeepingCentre (46, 26));
        rowL.removeFromLeft (66); limDrive->setBounds (rowL.withSizeKeepingCentre (84, 74));
        m.removeFromTop (6);
        auto rowM = m.removeFromTop (juce::jmin (m.getHeight(), 120));
        outMeter.setBounds (rowM.removeFromRight (46).withSizeKeepingCentre (12, rowM.getHeight() - 8));
        rowM.removeFromLeft (66); mainK->setBounds (rowM.withSizeKeepingCentre (84, 74));
    }
};
