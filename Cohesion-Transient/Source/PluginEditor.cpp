#include "PluginEditor.h"

CohesionTransientAudioProcessorEditor::CohesionTransientAudioProcessorEditor (CohesionTransientAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    auto lab = [this] (juce::Label& l, const juce::String& t, float sz, juce::Colour c)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font ("Segoe UI", sz, juce::Font::plain));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    };
    lab (title, "COHESION - TRANSIENT", 18.0f, juce::Colours::white);
    title.setFont (juce::Font ("Segoe UI", 18.0f, juce::Font::bold).withExtraKerningFactor (0.14f));
    lab (subtitle, "by Pepperwood", 11.0f, juce::Colours::white.withAlpha (0.55f));
    lab (subcap, "Transient Shaper", 10.0f, juce::Colour (0xff6c7a74));

    addChildComponent (updateBadge);
    addAndMakeVisible (viz);

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

    addKnob (attackS,  attackL,  "attack",      "ATTACK",      " dB");
    addKnob (sustainS, sustainL, "sustain",     "SUSTAIN",     " dB");
    addKnob (sensS,    sensL,    "sensitivity", "SENSITIVITY", " %");
    addKnob (outS,     outL,     "output",      "OUTPUT",      " dB");

    setSize (kBaseW, kBaseH);
    startTimerHz (20);
}

CohesionTransientAudioProcessorEditor::~CohesionTransientAudioProcessorEditor() { setLookAndFeel (nullptr); }

void CohesionTransientAudioProcessorEditor::addKnob (juce::Slider& s, juce::Label& l, const juce::String& id,
                                                     const juce::String& name, const juce::String& suffix)
{
    addAndMakeVisible (s);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
    s.setTextValueSuffix (suffix);
    atts.push_back (std::make_unique<SliderAtt> (proc.apvts, id, s));
    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold).withExtraKerningFactor (0.08f));
    l.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
    addAndMakeVisible (l);
}

void CohesionTransientAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff2b2f36), 0, 0, juce::Colour (0xff15171b), 0, (float) getHeight(), false);
    g.setGradientFill (bg); g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (6.0f), 8.0f, 1.5f);

    g.setColour (juce::Colour (0xff9fb0aa)); g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::plain).withExtraKerningFactor (0.14f));
    g.drawText ("PRESET", 600, 14, 80, 12, juce::Justification::centred);

    // output meter (top-right)
    g.setColour (juce::Colour (0xff9fb0aa)); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
    g.drawText ("OUT", 896, 14, 30, 12, juce::Justification::centred);
    const float h = juce::jlimit (0.0f, 1.0f, outMeter) * 34.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float bx = 902.0f + i * 12.0f;
        g.setColour (juce::Colour (0xff0e1013)); g.fillRoundedRectangle (bx, 30.0f, 7.0f, 34.0f, 2.0f);
        g.setColour (juce::Colour (0xff39ffb0)); g.fillRoundedRectangle (bx, 64.0f - h, 7.0f, h, 2.0f);
    }
}

void CohesionTransientAudioProcessorEditor::resized()
{
    title.setBounds    (22, 26, 320, 26);
    subtitle.setBounds (22, 47, 180, 16);
    updateBadge.placeAfter (subtitle);
    subcap.setBounds   (360, 46, 180, 14);

    prevBtn.setBounds   (520, 30, 24, 30);
    presetBtn.setBounds (548, 30, 184, 30);
    nextBtn.setBounds   (736, 30, 24, 30);

    viz.setBounds (12, 74, 956, 266);

    const int cx[4] = { 160, 395, 630, 865 };
    juce::Slider* ks[4] = { &attackS, &sustainS, &sensS, &outS };
    juce::Label*  kl[4] = { &attackL, &sustainL, &sensL, &outL };
    for (int i = 0; i < 4; ++i)
    {
        ks[i]->setBounds (cx[i] - 44, 360, 88, 104);   // dial + value below
        kl[i]->setBounds (cx[i] - 64, 480, 128, 16);
    }
}

void CohesionTransientAudioProcessorEditor::timerCallback()
{
    presetBtn.setButtonText (proc.getProgramName (proc.getCurrentProgram()));
    const float target = proc.getOutPeak();
    outMeter = juce::jmax (target, outMeter * 0.8f);
    repaint (896, 14, 40, 56);
}
