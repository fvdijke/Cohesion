#include "PluginEditor.h"

CohesionADTAudioProcessorEditor::CohesionADTAudioProcessorEditor (CohesionADTAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    auto lab = [this] (juce::Label& l, const juce::String& t, float sz, juce::Colour c, juce::Justification j)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font ("Segoe UI", sz, juce::Font::plain));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (j);
        addAndMakeVisible (l);
    };
    lab (title,    "COHESION - ADT", 18.0f, juce::Colours::white, juce::Justification::centredLeft);
    title.setFont (juce::Font ("Segoe UI", 18.0f, juce::Font::bold).withExtraKerningFactor (0.14f));
    lab (subtitle, "by Pepperwood", 11.0f, juce::Colours::white.withAlpha (0.55f), juce::Justification::centredLeft);
    lab (subcap,   "Artificial Double Tracking", 10.0f, juce::Colour (0xff6c7a74), juce::Justification::centredLeft);

    addChildComponent (updateBadge);
    addAndMakeVisible (viz);

    // preset selector
    for (auto* b : { &prevBtn, &nextBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fe1cb));
        addAndMakeVisible (*b);
    }
    presetBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    presetBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeafff6));
    addAndMakeVisible (presetBtn);
    prevBtn.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() - 1 + proc.getNumPrograms()) % proc.getNumPrograms()); };
    nextBtn.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    presetBtn.onClick = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn),
                         [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
    };

    addKnob (delayS,    delayL,    "delayMs",  "DELAY",    " ms");
    addKnob (spreadS,   spreadL,   "spread",   "SPREAD",   " %");
    addKnob (detuneS,   detuneL,   "detune",   "DETUNE",   " ct");
    addKnob (rateS,     rateL,     "lfoRate",  "RATE",     " Hz");
    addKnob (depthS,    depthL,    "lfoDepth", "DEPTH",    " %");
    addKnob (humanizeS, humanizeL, "humanize", "HUMANIZE", " %");
    addKnob (rotateS,   rotateL,   "phRotate", "ROTATE",   juce::String::fromUTF8 ("\xc2\xb0"));
    addKnob (widthS,    widthL,    "width",    "WIDTH",    " %");
    addKnob (lowMonoS,  lowMonoL,  "lowMono",  "LOW MONO", " Hz");
    addKnob (mixS,      mixL,      "mix",      "DRY/WET",  " %");
    addKnob (outS,      outL,      "outGain",  "OUTPUT",   " dB");

    shapeC.addItemList ({ "Sine", "Random" }, 1);
    addAndMakeVisible (shapeC);
    cAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, "lfoShape", shapeC));

    for (int i = 0; i < 2; ++i)
    {
        auto& b = voicesBtn[i];
        b.setButtonText (juce::String (i + 1));
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fb0aa));
        b.onClick = [this, i] { if (auto* pp = proc.apvts.getParameter ("voices")) pp->setValueNotifyingHost (pp->convertTo0to1 ((float) i)); };
        addAndMakeVisible (b);
    }

    addToggle (correctBtn,  "phCorrect", "CORRECT");
    addToggle (invertBtn,   "phInvert",  juce::String::fromUTF8 ("INVERT \xc3\x98"));
    addToggle (bassMonoBtn, "bassMono",  "BASS MONO");
    addToggle (vintageBtn,  "vintage",   "VINTAGE");
    addToggle (monoBtn,     "monoOut",   "MONO");

    setSize (kBaseW, kBaseH);
    startTimerHz (15);
}

CohesionADTAudioProcessorEditor::~CohesionADTAudioProcessorEditor() { setLookAndFeel (nullptr); }

void CohesionADTAudioProcessorEditor::addKnob (juce::Slider& s, juce::Label& l, const juce::String& id,
                                               const juce::String& name, const juce::String& suffix)
{
    addAndMakeVisible (s);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    s.setTextValueSuffix (suffix);
    sAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, id, s));
    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (9.5f);
    l.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
    addAndMakeVisible (l);
}

void CohesionADTAudioProcessorEditor::addToggle (juce::ToggleButton& b, const juce::String& id, const juce::String& txt)
{
    b.setButtonText (txt);
    addAndMakeVisible (b);
    bAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, id, b));
}

void CohesionADTAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff2b2f36), 0, 0, juce::Colour (0xff15171b), 0, (float) getHeight(), false);
    g.setGradientFill (bg); g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (6.0f), 8.0f, 1.5f);

    const int px[5] = { 12, 203, 394, 585, 776 };
    const char* names[5] = { "DOUBLE", "MODULATION", "PHASE", "WIDTH", "OUTPUT" };
    for (int i = 0; i < 5; ++i)
    {
        auto r = juce::Rectangle<float> ((float) px[i], 312.0f, 183.0f, 234.0f);
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (juce::Colour (0xff39ffb0)); g.fillEllipse (px[i] + 14.0f, 328.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xffe8eef0));
        g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold).withExtraKerningFactor (0.12f));
        g.drawText (names[i], px[i] + 32, 324, 150, 18, juce::Justification::centredLeft);
    }

    g.setColour (juce::Colour (0xff6c7a74)); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
    g.drawText ("VOICES", 104, 452, 70, 12, juce::Justification::centredLeft);
    g.drawText ("SHAPE",  299, 452, 70, 12, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff9fb0aa)); g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::plain).withExtraKerningFactor (0.12f));
    g.drawText ("PRESET", 490 - 40, 14, 80, 12, juce::Justification::centred);
}

void CohesionADTAudioProcessorEditor::resized()
{
    title.setBounds    (22, 26, 300, 26);
    subtitle.setBounds (22, 47, 180, 16);
    updateBadge.placeAfter (subtitle);
    subcap.setBounds   (330, 46, 220, 14);

    prevBtn.setBounds   (500, 30, 24, 30);
    presetBtn.setBounds (528, 30, 200, 30);
    nextBtn.setBounds   (732, 30, 24, 30);

    viz.setBounds (12, 74, 956, 226);

    auto knob = [] (juce::Slider& s, juce::Label& l, int cx, int cy)
    {
        l.setBounds (cx - 36, cy - 40, 72, 12);
        s.setBounds (cx - 28, cy - 24, 56, 56);   // rotary + textbox below
    };

    // P1 DOUBLE (px 12)
    knob (delayS, delayL, 58, 392);  knob (spreadS, spreadL, 140, 392);  knob (detuneS, detuneL, 58, 486);
    voicesBtn[0].setBounds (108, 466, 32, 22); voicesBtn[1].setBounds (144, 466, 32, 22);

    // P2 MODULATION (px 203)
    knob (rateS, rateL, 249, 392);  knob (depthS, depthL, 331, 392);  knob (humanizeS, humanizeL, 249, 486);
    shapeC.setBounds (299, 466, 76, 22);

    // P3 PHASE (px 394)
    knob (rotateS, rotateL, 485, 392);
    correctBtn.setBounds (404, 466, 80, 26);  invertBtn.setBounds (490, 466, 80, 26);

    // P4 WIDTH (px 585)
    knob (widthS, widthL, 631, 392);  knob (lowMonoS, lowMonoL, 713, 392);
    bassMonoBtn.setBounds (597, 470, 160, 26);

    // P5 OUTPUT (px 776)
    knob (mixS, mixL, 822, 392);  knob (outS, outL, 904, 392);
    vintageBtn.setBounds (788, 470, 80, 26);  monoBtn.setBounds (872, 470, 80, 26);
}

void CohesionADTAudioProcessorEditor::timerCallback()
{
    presetBtn.setButtonText (proc.getProgramName (proc.getCurrentProgram()));
    const int v = (int) proc.apvts.getRawParameterValue ("voices")->load();   // 0 -> "1", 1 -> "2"
    for (int i = 0; i < 2; ++i)
    {
        const bool on = (i == v);
        voicesBtn[i].setColour (juce::TextButton::buttonColourId, on ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        voicesBtn[i].setColour (juce::TextButton::textColourOffId, on ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        voicesBtn[i].repaint();
    }
}
