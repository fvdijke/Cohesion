#include "PluginEditor.h"

CohesionEQCompAudioProcessorEditor::CohesionEQCompAudioProcessorEditor (CohesionEQCompAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    title.setText ("COHESION - EQ/COMP", juce::dontSendNotification);
    title.setFont (juce::Font ("Segoe UI", 18.0f, juce::Font::bold).withExtraKerningFactor (0.14f));
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible (title);

    subtitle.setText ("by Pepperwood", juce::dontSendNotification);
    subtitle.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain).withExtraKerningFactor (0.1f));
    subtitle.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    content.addAndMakeVisible (subtitle);
    content.addChildComponent (updateBadge);

    for (auto* t : { &eqTab, &mbTab, &asTab })
    {
        t->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        t->setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fb0aa));
        content.addAndMakeVisible (t);
    }
    eqTab.onClick = [this] { showTab (0); };
    mbTab.onClick = [this] { showTab (1); };
    asTab.onClick = [this] { showTab (2); };

    // BANDS 2/3/4/5 (shown on MULTIBAND tab)
    bandsL.setText ("BANDS", juce::dontSendNotification);
    bandsL.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::plain).withExtraKerningFactor (0.1f));
    bandsL.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
    bandsL.setJustificationType (juce::Justification::centredRight);
    content.addAndMakeVisible (bandsL);
    for (int i = 0; i < 4; ++i)
    {
        auto& b = bandsBtn[i];
        b.setButtonText (juce::String (i + 2));
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fb0aa));
        b.onClick = [this, i] { setParam ("mbBands", (float) i); mbPanel.resized(); mbPanel.repaint(); };
        content.addAndMakeVisible (b);
    }

    onBtn.setButtonText ("ON");
    onBtn.onClick = [this]
    {
        const auto id = onTargetId();
        if (id.isNotEmpty()) setParam (id, onBtn.getToggleState() ? 1.0f : 0.0f);
    };
    content.addAndMakeVisible (onBtn);

    auto addTrim = [this] (juce::Slider& s, juce::Label& l, const juce::String& id, const juce::String& nm)
    {
        content.addAndMakeVisible (s);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 12);
        s.setTextValueSuffix (" dB");
        sAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, id, s));
        l.setText (nm, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (9.5f); l.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
        content.addAndMakeVisible (l);
    };
    addTrim (inS,  inL,  "inGain",  "IN");
    addTrim (outS, outL, "outGain", "OUT");

    content.addAndMakeVisible (eqPanel);
    content.addAndMakeVisible (mbPanel);
    content.addAndMakeVisible (asPanel);

    tooltip = std::make_unique<juce::TooltipWindow> (this, 500);

    chainBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
    chainBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fe1cb));
    chainBtn.setTooltip ("Signaalvolgorde: EQ voor of na de multiband-comp (klik om te wisselen)");
    chainBtn.onClick = [this]
    {
        setParam ("chainOrder", proc.apvts.getRawParameterValue ("chainOrder")->load() > 0.5f ? 0.0f : 1.0f);
    };
    content.addAndMakeVisible (chainBtn);

    help = std::make_unique<HelpOverlay>();
    addChildComponent (*help);
    content.addAndMakeVisible (helpBtn);
    helpBtn.onClick = [this]
    {
        const bool show = ! help->isVisible();
        help->setVisible (show);
        if (show) help->toFront (true);
    };

    showTab (2);   // open on ASSIST (workflow starts with analysis)
    startTimerHz (20);

    setResizable (true, true);
    constrainer.setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    constrainer.setSizeLimits ((int) (kBaseW * 0.6), (int) (kBaseH * 0.6),
                               (int) (kBaseW * 1.6), (int) (kBaseH * 1.6));
    setConstrainer (&constrainer);
    setSize (kBaseW, kBaseH);
}

CohesionEQCompAudioProcessorEditor::~CohesionEQCompAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CohesionEQCompAudioProcessorEditor::timerCallback()
{
    const auto id = onTargetId();
    if (id.isNotEmpty())
        onBtn.setToggleState (proc.apvts.getRawParameterValue (id)->load() > 0.5f, juce::dontSendNotification);

    chainBtn.setButtonText (proc.apvts.getRawParameterValue ("chainOrder")->load() > 0.5f ? "COMP>EQ" : "EQ>COMP");

    const int active = (int) proc.apvts.getRawParameterValue ("mbBands")->load();
    for (int i = 0; i < 4; ++i)
    {
        const bool on = (i == active);
        bandsBtn[i].setColour (juce::TextButton::buttonColourId, on ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        bandsBtn[i].setColour (juce::TextButton::textColourOffId, on ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        bandsBtn[i].repaint();
    }
}

void CohesionEQCompAudioProcessorEditor::showTab (int t)
{
    tab = t;
    eqPanel.setVisible (t == 0);
    mbPanel.setVisible (t == 1);
    asPanel.setVisible (t == 2);

    const bool eqT = (t == 0), mbT = (t == 1), asT = (t == 2);
    inS.setVisible (eqT);  inL.setVisible (eqT);
    outS.setVisible (eqT); outL.setVisible (eqT);
    bandsL.setVisible (mbT);
    for (auto& b : bandsBtn) b.setVisible (mbT);
    onBtn.setVisible (! asT);

    juce::TextButton* tabs[3] = { &eqTab, &mbTab, &asTab };
    for (int i = 0; i < 3; ++i)
    {
        const bool on = (i == t);
        tabs[i]->setColour (juce::TextButton::buttonColourId, on ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        tabs[i]->setColour (juce::TextButton::textColourOffId, on ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        tabs[i]->repaint();
    }
    layoutContent();
}

void CohesionEQCompAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff2b2f36), 0, 0, juce::Colour (0xff15171b), 0, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (6.0f), 8.0f, 1.5f);
}

void CohesionEQCompAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseW;
    content.setTransform ({});
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (juce::AffineTransform::scale (s));
    if (help) help->setBounds (getLocalBounds());
    layoutContent();
}

void CohesionEQCompAudioProcessorEditor::layoutContent()
{
    auto b = juce::Rectangle<int> (0, 0, kBaseW, kBaseH).reduced (12);
    const int centerX = kBaseW / 2;

    auto header = b.removeFromTop (58);
    // title / subtitle: same placement as the GLUE tool (absolute 14,22 / 14,43)
    title.setBounds    (14, 22, 280, 26);
    subtitle.setBounds (14, 43, 180, 16);
    updateBadge.placeAfter (subtitle);

    const int tw = 100, th = 30, gap = 6;
    int tx = centerX - (3 * tw + 2 * gap) / 2;
    const int ty = header.getY() + 12;
    // visual order: ASSIST · MULTIBAND · EQ (onClick still maps to the right panel)
    asTab.setBounds (tx, ty, tw, th); tx += tw + gap;
    mbTab.setBounds (tx, ty, tw, th); tx += tw + gap;
    eqTab.setBounds (tx, ty, tw, th);

    // right cluster, laid out right-to-left
    auto rc = header.removeFromRight (360);
    const int cy = rc.getCentreY();
    onBtn.setBounds    (rc.getRight() - 54, cy - 13, 54, 26);
    chainBtn.setBounds (rc.getRight() - 54 - 6 - 80, cy - 13, 80, 26);   // routing: always visible
    int rightEdge = rc.getRight() - 54 - 6 - 80 - 12;

    if (tab == 1)   // BANDS 2/3/4/5
    {
        int x = rightEdge - 4 * 28;
        bandsL.setBounds (x - 54, cy - 8, 50, 16);
        for (int i = 0; i < 4; ++i) bandsBtn[i].setBounds (x + i * 28, cy - 13, 26, 26);
    }
    else if (tab == 0)   // IN / OUT trims
    {
        auto outCell = juce::Rectangle<int> (rightEdge - 50, header.getY() + 2, 50, 52);
        outL.setBounds (outCell.getX(), outCell.getY(), 50, 12);
        outS.setBounds (outCell.getX() + 1, outCell.getY() + 12, 48, 42);
        auto inCell = juce::Rectangle<int> (rightEdge - 50 - 54, header.getY() + 2, 50, 52);
        inL.setBounds (inCell.getX(), inCell.getY(), 50, 12);
        inS.setBounds (inCell.getX() + 1, inCell.getY() + 12, 48, 42);
    }

    b.removeFromTop (6);
    eqPanel.setBounds (b);
    mbPanel.setBounds (b);
    asPanel.setBounds (b);

    helpBtn.setBounds (kBaseW - 42, kBaseH - 42, 30, 30);
}
