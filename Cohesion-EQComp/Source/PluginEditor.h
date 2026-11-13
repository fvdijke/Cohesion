#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <memory>
#include "PluginProcessor.h"
#include "CohesionUpdateCheck.h"
#include "LookAndFeel.h"
#include "EqCurve.h"
#include "MbCrossover.h"
#include "AssistEngine.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

// Shared band colours (same as the multiband comp) so the ASSIST cards and the
// graph nodes line up visually.
static inline juce::Colour assistCardColour (int i)
{
    const juce::Colour c[5] = { juce::Colour (0xffc98a5a), juce::Colour (0xffcabf63),
                                juce::Colour (0xff6fc88a), juce::Colour (0xff5aa6d8), juce::Colour (0xffb08fd8) };
    return c[juce::jlimit (0, 4, i)];
}

// ===========================================================================
//  EQ panel: interactive curve + band toggles + selected-band controls.
// ===========================================================================
class EqPanel : public juce::Component, private juce::Timer
{
public:
    explicit EqPanel (CohesionEQCompAudioProcessor& p) : proc (p), curve (p)
    {
        addAndMakeVisible (curve);
        curve.onSelect = [this] (int s) { setTarget (s); };

        auto addToggle = [this] (juce::ToggleButton& b, const juce::String& id, const juce::String& txt, int sel)
        {
            b.setButtonText (txt);
            addAndMakeVisible (b);
            atts.push_back (std::make_unique<ButtonAtt> (proc.apvts, id, b));
            b.onClick = [this, sel] { setTarget (sel); };
        };
        addToggle (hpBtn, "hpOn", "HPF", 100);
        for (int i = 0; i < 6; ++i) addToggle (bandBtn[i], bid (i, "on"), juce::String (i + 1), i);
        addToggle (lpBtn, "lpOn", "LPF", 101);

        selLabel.setJustificationType (juce::Justification::centredLeft);
        selLabel.setFont (juce::Font ("Segoe UI", 10.5f, juce::Font::bold).withExtraKerningFactor (0.1f));
        selLabel.setColour (juce::Label::textColourId, juce::Colour (0xff5dcaa5));
        addAndMakeVisible (selLabel);

        setupRowSlider (freqS, freqL, "Freq", " Hz");
        setupRowSlider (gainS, gainL, "Gain", " dB");
        setupRowSlider (qS,    qL,    "Q",    "");
        typeL.setText ("Type", juce::dontSendNotification);
        typeL.setFont (11.0f); typeL.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
        addAndMakeVisible (typeL);
        typeC.addItemList ({ "Bell", "Low Shelf", "High Shelf" }, 1);
        addAndMakeVisible (typeC);
        typeC.onChange = [this] { if (target >= 0 && target < 6) setChoice (bid (target, "type"), typeC.getSelectedId() - 1); };

        freqS.onValueChange = [this] { writeFreq(); };
        gainS.onValueChange = [this] { if (target >= 0 && target < 6) setP (bid (target, "gain"), (float) gainS.getValue()); };
        qS.onValueChange    = [this] { if (target >= 0 && target < 6) setP (bid (target, "q"),    (float) qS.getValue()); };

        setTarget (0);
        startTimerHz (20);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        auto bottom = r.removeFromBottom (74);
        curve.setBounds (r.reduced (0, 2));

        auto toggles = bottom.removeFromTop (26);
        const int n = 8;
        const int w = juce::jmin (54, toggles.getWidth() / n - 4);
        hpBtn.setBounds (toggles.removeFromLeft (w)); toggles.removeFromLeft (4);
        for (int i = 0; i < 6; ++i) { bandBtn[i].setBounds (toggles.removeFromLeft (w)); toggles.removeFromLeft (4); }
        lpBtn.setBounds (toggles.removeFromLeft (w));

        bottom.removeFromTop (4);
        selLabel.setBounds (bottom.removeFromLeft (90));
        auto cell = [&bottom] (int wd) { auto c = bottom.removeFromLeft (wd); bottom.removeFromLeft (10); return c; };
        auto place = [] (juce::Rectangle<int> c, juce::Label& l, juce::Component& ctl)
        { l.setBounds (c.removeFromTop (14)); ctl.setBounds (c); };
        place (cell (150), freqL, freqS);
        place (cell (150), gainL, gainS);
        place (cell (120), qL,    qS);
        place (cell (120), typeL, typeC);
    }

private:
    static juce::String bid (int b, const char* f) { return "b" + juce::String (b) + "_" + f; }
    float get (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void  setP (const juce::String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    void  setChoice (const juce::String& id, int idx) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 ((float) idx)); }

    void setupRowSlider (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& suffix)
    {
        addAndMakeVisible (s);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 16);
        s.setTextValueSuffix (suffix);
        s.setColour (juce::Slider::trackColourId, juce::Colour (0xff39ffb0));
        s.setLookAndFeel (&sliderLnf);
        l.setText (name, juce::dontSendNotification);
        l.setFont (11.0f); l.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
        addAndMakeVisible (l);
    }

    void writeFreq()
    {
        if (target == 100) setP ("hpFreq", (float) freqS.getValue());
        else if (target == 101) setP ("lpFreq", (float) freqS.getValue());
        else if (target >= 0 && target < 6) setP (bid (target, "freq"), (float) freqS.getValue());
    }

    void setTarget (int t)
    {
        target = t;
        curve.setSelected (t);
        const bool band = (t >= 0 && t < 6);
        gainS.setEnabled (band); qS.setEnabled (band); typeC.setEnabled (band);

        if (band)       { selLabel.setText ("BAND " + juce::String (t + 1), juce::dontSendNotification);
                          freqS.setRange (20.0, 20000.0, 1.0); freqS.setSkewFactorFromMidPoint (1000.0);
                          gainS.setRange (-18.0, 18.0, 0.1); qS.setRange (0.1, 10.0, 0.01); qS.setSkewFactorFromMidPoint (1.0); }
        else if (t == 100) { selLabel.setText ("HPF", juce::dontSendNotification); freqS.setRange (20.0, 1000.0, 1.0); freqS.setSkewFactorFromMidPoint (150.0); }
        else if (t == 101) { selLabel.setText ("LPF", juce::dontSendNotification); freqS.setRange (1000.0, 20000.0, 1.0); freqS.setSkewFactorFromMidPoint (6000.0); }
        else selLabel.setText ("-", juce::dontSendNotification);
        refresh();
    }

    void timerCallback() override { refresh(); }

    void refresh()
    {
        if (target == 100)      freqS.setValue (get ("hpFreq"), juce::dontSendNotification);
        else if (target == 101) freqS.setValue (get ("lpFreq"), juce::dontSendNotification);
        else if (target >= 0 && target < 6)
        {
            freqS.setValue (get (bid (target, "freq")), juce::dontSendNotification);
            gainS.setValue (get (bid (target, "gain")), juce::dontSendNotification);
            qS.setValue    (get (bid (target, "q")),    juce::dontSendNotification);
            typeC.setSelectedId ((int) get (bid (target, "type")) + 1, juce::dontSendNotification);
        }
    }

    BandSliderLookAndFeel sliderLnf;
    CohesionEQCompAudioProcessor& proc;
    EqCurve curve;
    juce::ToggleButton hpBtn, lpBtn, bandBtn[6];
    juce::Label selLabel, freqL, gainL, qL, typeL;
    juce::Slider freqS, gainS, qS;
    juce::ComboBox typeC;
    int target = -1;
    std::vector<std::unique_ptr<ButtonAtt>> atts;
};

// ===========================================================================
//  Multiband panel: crossover display + per-band GATE/EXP/COMP columns.
// ===========================================================================
class MultibandPanel : public juce::Component, private juce::Timer
{
public:
    explicit MultibandPanel (CohesionEQCompAudioProcessor& p) : proc (p), crossover (p)
    {
        addAndMakeVisible (crossover);

        const char* modeTxt[3] = { "GATE", "EXP", "COMP" };   // display order; param: Comp0 Exp1 Gate2
        const int   modeParam[3] = { 2, 1, 0 };
        for (int b = 0; b < 5; ++b)
        {
            auto& col = columns[b];
            col.num.setText (juce::String (b + 1), juce::dontSendNotification);
            col.num.setFont (juce::Font ("Segoe UI", 15.0f, juce::Font::bold));
            col.num.setColour (juce::Label::textColourId, bandColour (b));
            addAndMakeVisible (col.num);

            for (int m = 0; m < 3; ++m)
            {
                auto& mb = col.mode[m];
                mb.setButtonText (modeTxt[m]);
                styleBtn (mb);
                const int pv = modeParam[m];
                mb.onClick = [this, b, pv] { setChoice (mbId (b, "mode"), pv); };
                addAndMakeVisible (mb);
            }

            auto addS = [&] (juce::Slider& s, juce::Label& l, const char* nm, const char* id, const char* sfx)
            {
                l.setText (nm, juce::dontSendNotification); l.setFont (10.0f);
                l.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0aa));
                addAndMakeVisible (l);
                s.setSliderStyle (juce::Slider::LinearHorizontal);
                s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
                s.setTextValueSuffix (sfx);
                s.setColour (juce::Slider::trackColourId, bandColour (b));
                s.setLookAndFeel (&bandLnf);
                addAndMakeVisible (s);
                sAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, mbId (b, id), s));
            };
            addS (col.thr,   col.thrL,   "Thr",   "thr",   " dB");
            addS (col.ratio, col.ratioL, "Ratio", "ratio", "");
            addS (col.atk,   col.atkL,   "Att",   "atk",   " ms");
            addS (col.rel,   col.relL,   "Rel",   "rel",   " ms");
            addS (col.width, col.widthL, "Width", "width", "");
            addS (col.gain,  col.gainL,  "Gain",  "gain",  " dB");

            col.solo.setButtonText ("Solo"); col.mute.setButtonText ("Mute"); col.byp.setButtonText ("Byp");
            for (auto* t : { &col.solo, &col.mute, &col.byp }) addAndMakeVisible (*t);
            bAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, mbId (b, "solo"), col.solo));
            bAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, mbId (b, "mute"), col.mute));
            bAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, mbId (b, "byp"),  col.byp));
        }
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const int nb = numBands();
        for (int b = 0; b < nb; ++b)
        {
            g.setColour (juce::Colour (0xff0e1013));
            g.fillRoundedRectangle (colBox[b].toFloat(), 8.0f);
            g.setColour (bandColour (b).withAlpha (0.25f));
            g.drawRoundedRectangle (colBox[b].toFloat(), 8.0f, 1.0f);

            // GR meter box + bars
            auto mb = meterBox[b].toFloat();
            g.setColour (juce::Colour (0xff0d0f12));
            g.fillRoundedRectangle (mb, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawRoundedRectangle (mb, 4.0f, 1.0f);
            const float gr = proc.getBandGainReductionDb (b);
            const float norm = juce::jlimit (0.0f, 1.0f, gr / 18.0f);
            auto inner = mb.reduced (10.0f, 8.0f);
            const float barH = inner.getHeight() * norm;
            g.setColour (bandColour (b));
            g.fillRect (inner.getX(),        inner.getBottom() - barH, 14.0f, barH);
            g.fillRect (inner.getX() + 20.0f, inner.getBottom() - barH, 14.0f, barH);
            g.setColour (juce::Colour (0xff9fb0aa));
            g.setFont (9.5f);
            g.drawText ("GR " + juce::String (gr, 1) + " dB", mb.reduced (6.0f), juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);
        crossover.setBounds (area.removeFromTop (164));
        area.removeFromTop (6);

        const int nb = numBands();
        const int cw = area.getWidth() / nb;
        for (int b = 0; b < 5; ++b)
        {
            auto& col = columns[b];
            const bool show = (b < nb);
            col.setAllVisible (show);
            if (! show) continue;

            colBox[b] = juce::Rectangle<int> (area.getX() + b * cw, area.getY(), cw - 6, area.getHeight());
            auto c = colBox[b].reduced (8, 8);

            auto topRow = c.removeFromTop (22);
            col.num.setBounds (topRow.removeFromLeft (24));
            const int mw = topRow.getWidth() / 3 - 2;
            for (int m = 0; m < 3; ++m) { col.mode[m].setBounds (topRow.removeFromLeft (mw)); topRow.removeFromLeft (3); }
            c.removeFromTop (4);

            auto row = [&c] (juce::Label& l, juce::Slider& s) { auto rr = c.removeFromTop (18); l.setBounds (rr.removeFromLeft (40)); s.setBounds (rr); c.removeFromTop (3); };
            row (col.thrL, col.thr); row (col.ratioL, col.ratio); row (col.atkL, col.atk);
            row (col.relL, col.rel); row (col.widthL, col.width); row (col.gainL, col.gain);
            c.removeFromTop (4);

            meterBox[b] = c.removeFromTop (juce::jmax (60, c.getHeight() - 30));
            c.removeFromTop (4);
            auto btns = c.removeFromTop (22);
            const int bw = btns.getWidth() / 3 - 3;
            col.solo.setBounds (btns.removeFromLeft (bw)); btns.removeFromLeft (4);
            col.mute.setBounds (btns.removeFromLeft (bw)); btns.removeFromLeft (4);
            col.byp.setBounds  (btns.removeFromLeft (bw));
        }
    }

private:
    struct Column : juce::Component
    {
        juce::Label num, thrL, ratioL, atkL, relL, widthL, gainL;
        juce::TextButton mode[3];
        juce::Slider thr, ratio, atk, rel, width, gain;
        juce::ToggleButton solo, mute, byp;
        void setAllVisible (bool v)
        {
            for (auto* c : std::initializer_list<juce::Component*> {
                 &num,&thrL,&ratioL,&atkL,&relL,&widthL,&gainL,
                 &mode[0],&mode[1],&mode[2],&thr,&ratio,&atk,&rel,&width,&gain,&solo,&mute,&byp })
                c->setVisible (v);
        }
    };

    void timerCallback() override
    {
        const int nb = numBands();
        for (int b = 0; b < nb; ++b)
        {
            const int mode = (int) proc.apvts.getRawParameterValue (mbId (b, "mode"))->load(); // 0 comp,1 exp,2 gate
            highlight (columns[b].mode[0], mode == 2);   // GATE
            highlight (columns[b].mode[1], mode == 1);   // EXP
            highlight (columns[b].mode[2], mode == 0);   // COMP
        }
        repaint();
    }

    static void styleBtn (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fb0aa));
    }
    static void highlight (juce::TextButton& b, bool on)
    {
        b.setColour (juce::TextButton::buttonColourId, on ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        b.setColour (juce::TextButton::textColourOffId, on ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        b.repaint();
    }

    static juce::String mbId (int b, const char* f) { return "mb" + juce::String (b) + "_" + f; }
    int numBands() const { return 2 + (int) proc.apvts.getRawParameterValue ("mbBands")->load(); }
    void setChoice (const juce::String& id, int idx) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 ((float) idx)); }
    static juce::Colour bandColour (int b)
    {
        const juce::Colour c[5] = { juce::Colour (0xffc98a5a), juce::Colour (0xffcabf63),
                                    juce::Colour (0xff6fc88a), juce::Colour (0xff5aa6d8), juce::Colour (0xffb08fd8) };
        return c[juce::jlimit (0, 4, b)];
    }

    BandSliderLookAndFeel bandLnf;
    CohesionEQCompAudioProcessor& proc;
    MbCrossover crossover;
    Column columns[5];
    juce::Rectangle<int> colBox[5], meterBox[5];
    std::vector<std::unique_ptr<SliderAtt>> sAtts;
    std::vector<std::unique_ptr<ButtonAtt>> bAtts;
};

// ===========================================================================
//  Assist panel (layout matches the proposal; analysis engine comes later).
// ===========================================================================
class AssistCenter : public juce::Component, private juce::Timer
{
public:
    explicit AssistCenter (CohesionEQCompAudioProcessor& p)
        : proc (p), fft (CohesionEQCompAudioProcessor::fftOrder),
          window ((size_t) CohesionEQCompAudioProcessor::fftSize, juce::dsp::WindowingFunction<float>::hann)
    { for (auto& v : scopeDb) v = -120.0f; for (auto& v : scopePost) v = -120.0f; startTimerHz (30); }

    std::function<int()>          getInstrument;
    std::function<int()>          getStyle;
    std::function<float()>        getStrength;
    std::function<juce::String()> getInstrName;
    std::function<void()>         onProposal;

    void startListen() { for (auto& v : avgMag) v = 0.0f; frames = 0; listening = true; }
    bool  isListening() const { return listening; }
    float progress() const { return juce::jlimit (0.0f, 1.0f, (float) frames / (float) kTargetFrames); }
    const assist::Proposal& proposal() const { return prop; }

    // Recompute the proposal from the last measurement (e.g. when style/instrument changes),
    // so the target/proposal curves update without listening again.
    void reanalyze()
    {
        if (! hasMeas) return;
        prop = assist::analyze (lastMeas, getInstrument ? getInstrument() : 5,
                                getStyle ? getStyle() : 0, getStrength ? getStrength() * 0.01f : 0.7f);
        if (onProposal) onProposal();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (area, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (area, 6.0f, 1.0f);
        auto p = area.reduced (12.0f);
        p.removeFromBottom (14.0f);   // room for the frequency axis

        auto xF = [&] (float f) { return p.getX() + std::log (f / 20.0f) / std::log (1000.0f) * p.getWidth(); };
        auto yF = [&] (float db) { return p.getCentreY() - juce::jlimit (-10.0f, 10.0f, db) / 10.0f * (p.getHeight() * 0.5f); };

        // grid + frequency axis
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (p.getY() + p.getHeight() * i / 4.0f), p.getX(), p.getRight());
        const float gf[] = { 20.0f, 200.0f, 1000.0f, 5000.0f, 20000.0f };
        g.setFont (9.0f);
        for (float f : gf)
        {
            const float x = xF (f);
            g.setColour (juce::Colours::white.withAlpha (0.05f)); g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
            g.setColour (juce::Colour (0xff6c7a74));
            g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                        x - 16.0f, p.getBottom() + 2.0f, 32.0f, 12.0f, juce::Justification::centred);
        }

        // dynamic before/after spectra (always animating)
        {
            const int bins = CohesionEQCompAudioProcessor::fftSize / 2;
            const double fs = proc.getSampleRateHz();
            auto buildSpec = [&] (const float* sdb)
            {
                juce::Path spec; bool started = false;
                for (float px = p.getX(); px <= p.getRight(); px += 3.0f)
                {
                    const float f = 20.0f * std::pow (1000.0f, (px - p.getX()) / p.getWidth());
                    const float bf = juce::jlimit (1.0f, (float) bins - 2.0f, f / ((float) fs * 0.5f) * (float) bins);
                    const int b0 = (int) bf; const float fr = bf - (float) b0;
                    const float db = sdb[b0] * (1.0f - fr) + sdb[b0 + 1] * fr;
                    const float y = juce::jmap (juce::jlimit (-100.0f, 0.0f, db), -100.0f, 0.0f, p.getBottom(), p.getY());
                    if (! started) { spec.startNewSubPath (px, p.getBottom()); spec.lineTo (px, y); started = true; }
                    else spec.lineTo (px, y);
                }
                spec.lineTo (p.getRight(), p.getBottom()); spec.closeSubPath();
                return spec;
            };
            auto before = buildSpec (scopeDb);
            g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillPath (before);
            g.setColour (juce::Colours::white.withAlpha (0.18f)); g.strokePath (before, juce::PathStrokeType (1.0f));
            auto after = buildSpec (scopePost);
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.14f)); g.fillPath (after);
        }

        // legend: voor / na / doel / voorstel
        {
            auto lg = juce::Rectangle<float> (p.getX() + 6, p.getY() + 6, 176, 74);
            g.setColour (juce::Colour (0xff15181c)); g.fillRoundedRectangle (lg, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (lg, 5.0f, 1.0f);
            g.setFont (9.5f);
            const float tx = lg.getX() + 36.0f;
            g.setColour (juce::Colours::white.withAlpha (0.4f)); g.fillRect (lg.getX() + 10, lg.getY() + 13, 20.0f, 2.0f);
            g.setColour (juce::Colour (0xff9fb0aa)); g.drawText ("voor (ingang)", tx, lg.getY() + 6, 134, 14, juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.6f)); g.fillRect (lg.getX() + 10, lg.getY() + 29, 20.0f, 2.0f);
            g.setColour (juce::Colour (0xff9fb0aa)); g.drawText ("na (uitgang)", tx, lg.getY() + 22, 134, 14, juce::Justification::centredLeft);
            { juce::Line<float> ln (lg.getX() + 10.0f, lg.getY() + 47.0f, lg.getX() + 30.0f, lg.getY() + 47.0f);
              const float d[] = { 4.0f, 3.0f }; g.setColour (juce::Colour (0xff9fe1cb)); g.drawDashedLine (ln, d, 2, 1.6f); }
            const juce::String dn = juce::String ("doel") + (getInstrName ? juce::String (" (") + getInstrName() + ")" : juce::String());
            g.setColour (juce::Colour (0xff9fb0aa)); g.drawText (dn, tx, lg.getY() + 40, 134, 14, juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xff39ffb0)); g.fillRect (lg.getX() + 10, lg.getY() + 63, 20.0f, 2.0f);
            g.setColour (juce::Colour (0xff9fb0aa)); g.drawText ("voorstel", tx, lg.getY() + 56, 134, 14, juce::Justification::centredLeft);
        }

        if (! prop.valid || listening)   // while listening show only the moving spectrum, so a re-analysis is visible
        {
            g.setColour (juce::Colour (0xff6c7a74)); g.setFont (12.0f);
            g.drawText (listening ? juce::String ("Luisteren naar het signaal...")
                                  : juce::String ("Druk LISTEN om te analyseren"),
                        p.withTrimmedTop (p.getHeight() - 22.0f), juce::Justification::centred);
            return;
        }

        for (int i = 0; i < (int) prop.eq.size(); ++i)
        { g.setColour (assistCardColour (i).withAlpha (0.10f));
          g.fillRect (xF (prop.eq[(size_t) i].freq) - 18.0f, p.getY(), 36.0f, p.getHeight()); }

        float result[assist::kBands];
        for (int i = 0; i < assist::kBands; ++i)
        { result[i] = prop.measured[i];
          for (auto& m : prop.eq) if (juce::approximatelyEqual (m.freq, assist::kFreq[i])) result[i] += m.gain; }

        auto drawCurve = [&] (const float* v, juce::Colour c, bool dashed, float th)
        {
            float X[assist::kBands], Y[assist::kBands];
            for (int i = 0; i < assist::kBands; ++i) { X[i] = xF (assist::kFreq[i]); Y[i] = yF (v[i]); }
            juce::Path pth; pth.startNewSubPath (X[0], Y[0]);
            for (int i = 0; i < assist::kBands - 1; ++i)   // Catmull-Rom -> smooth, glooiend
            {
                const int im1 = i > 0 ? i - 1 : 0;
                const int ip2 = i < assist::kBands - 2 ? i + 2 : i + 1;
                const float c1x = X[i] + (X[i + 1] - X[im1]) / 6.0f, c1y = Y[i] + (Y[i + 1] - Y[im1]) / 6.0f;
                const float c2x = X[i + 1] - (X[ip2] - X[i]) / 6.0f, c2y = Y[i + 1] - (Y[ip2] - Y[i]) / 6.0f;
                pth.cubicTo (c1x, c1y, c2x, c2y, X[i + 1], Y[i + 1]);
            }
            g.setColour (c);
            if (dashed) { const float d[] = { 5.0f, 4.0f }; juce::Path dp; juce::PathStrokeType (th).createDashedStroke (dp, pth, d, 2); g.fillPath (dp); }
            else g.strokePath (pth, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        drawCurve (prop.measured, juce::Colours::white.withAlpha (0.25f), false, 1.4f);
        drawCurve (prop.target,   juce::Colour (0xff9fe1cb).withAlpha (0.6f), true, 1.6f);
        drawCurve (result,        juce::Colour (0xff39ffb0), false, 2.4f);

        // nodes on the proposal curve, coloured to match the VOORSTEL cards
        for (int k = 0; k < (int) prop.eq.size(); ++k)
        {
            const auto& m = prop.eq[(size_t) k];
            const float x = xF (m.freq);
            float yv = result[0];
            for (int i = 0; i < assist::kBands; ++i) if (juce::approximatelyEqual (m.freq, assist::kFreq[i])) yv = result[i];
            const float y = yF (yv);
            g.setColour (juce::Colour (0xff0d0f12)); g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);
            g.setColour (assistCardColour (k));
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
            g.drawEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f, 2.0f);
        }

        // comp threshold + limiter ceiling as labelled reference lines (0 dBFS at top .. -40 at bottom)
        auto yLevel = [&] (float dbfs) { return p.getY() + juce::jmap (juce::jlimit (-40.0f, 0.0f, dbfs), 0.0f, -40.0f, 0.0f, p.getHeight()); };
        const float dash[] = { 4.0f, 3.0f };
        {
            const float y = yLevel (prop.thr);
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.5f));
            g.drawDashedLine (juce::Line<float> (p.getX(), y, p.getRight(), y), dash, 2, 1.2f);
            g.setColour (juce::Colour (0xff9fe1cb)); g.setFont (9.0f);
            g.drawText ("Comp thr " + juce::String ((int) prop.thr) + " dB", p.getRight() - 128.0f, y - 12.0f, 124.0f, 12.0f, juce::Justification::centredRight);
        }
        {
            const float y = yLevel (prop.limCeil);
            g.setColour (juce::Colour (0xffe0a24e).withAlpha (0.65f));
            g.drawDashedLine (juce::Line<float> (p.getX(), y, p.getRight(), y), dash, 2, 1.2f);
            g.setColour (juce::Colour (0xffe0a24e)); g.setFont (9.0f);
            g.drawText ("Limiter " + juce::String (prop.limCeil, 1) + " dB", p.getRight() - 128.0f, y + 1.0f, 124.0f, 12.0f, juce::Justification::centredRight);
        }
    }

private:
    void timerCallback() override
    {
        if (! isShowing()) return;
        const int bins = CohesionEQCompAudioProcessor::fftSize / 2;

        // "before" = input (pre-EQ); also drives the analysis so each run is consistent
        if (proc.isFftBlockReadyPre())
        {
            proc.readFftBlockPre (fftData);
            window.multiplyWithWindowingTable (fftData, (size_t) CohesionEQCompAudioProcessor::fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData);
            for (int i = 0; i < bins; ++i)
            {
                const float db = juce::Decibels::gainToDecibels (fftData[i] / (float) bins, -120.0f) + 12.0f;
                scopeDb[i] = juce::jmax (db, scopeDb[i] - 2.5f);
                if (listening) avgMag[i] += fftData[i];
            }
            if (listening)
            {
                if (frames >= kTargetFrames) finish();   // finish only after a full-progress frame was painted
                else ++frames;
            }
        }

        // "after" = output (post all processing)
        if (proc.isFftBlockReady())
        {
            proc.readFftBlock (fftData);
            window.multiplyWithWindowingTable (fftData, (size_t) CohesionEQCompAudioProcessor::fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData);
            for (int i = 0; i < bins; ++i)
            {
                const float db = juce::Decibels::gainToDecibels (fftData[i] / (float) bins, -120.0f) + 12.0f;
                scopePost[i] = juce::jmax (db, scopePost[i] - 2.5f);
            }
        }
        repaint();   // always dynamic (before/after animate continuously)
    }

    void finish()
    {
        listening = false;
        const int bins = CohesionEQCompAudioProcessor::fftSize / 2;
        const double fs = proc.getSampleRateHz();
        float meas[assist::kBands];
        for (int b = 0; b < assist::kBands; ++b)
        {
            const float f = assist::kFreq[b], lo = f / 1.3f, hi = f * 1.3f;
            double sum = 0.0; int n = 0;
            for (int i = 1; i < bins; ++i)
            { const double bf = i * (fs * 0.5) / bins; if (bf >= lo && bf <= hi) { sum += avgMag[i] / (double) frames; ++n; } }
            const float mag = (n > 0) ? (float) (sum / n) : 1.0e-6f;
            meas[b] = juce::Decibels::gainToDecibels (mag / (float) bins, -100.0f);
        }
        for (int b = 0; b < assist::kBands; ++b) lastMeas[b] = meas[b];
        hasMeas = true;
        reanalyze();
    }

    CohesionEQCompAudioProcessor& proc;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * CohesionEQCompAudioProcessor::fftSize] { 0.0f };
    float avgMag[CohesionEQCompAudioProcessor::fftSize / 2] { 0.0f };
    float scopeDb[CohesionEQCompAudioProcessor::fftSize / 2];    // "before" (input)
    float scopePost[CohesionEQCompAudioProcessor::fftSize / 2];  // "after" (output)
    float lastMeas[assist::kBands] { 0 };
    bool  hasMeas = false;
    int  frames = 0;
    bool listening = false;
    static constexpr int kTargetFrames = 128;   // ~12 s for a more reliable analysis
    assist::Proposal prop;
};

// Circular LISTEN button with a progress ring and "analysing %" sub-label.
class ListenButton : public juce::Button
{
public:
    ListenButton() : juce::Button ("LISTEN") {}
    std::function<bool()>  listening;
    std::function<float()> progress;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat();
        auto ctr = b.getCentre();
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 3.0f;
        const bool lst = listening && listening();
        g.setColour (juce::Colour (0xff0f241d)); g.fillEllipse (ctr.x - r, ctr.y - r, 2 * r, 2 * r);
        g.setColour (juce::Colour (0xff0f6e56)); g.drawEllipse (ctr.x - r, ctr.y - r, 2 * r, 2 * r, 2.0f);
        if (lst && progress)
        {
            juce::Path arc;
            arc.addCentredArc (ctr.x, ctr.y, r, r, 0.0f, 0.0f,
                               juce::MathConstants<float>::twoPi * juce::jlimit (0.0f, 1.0f, progress()), true);
            g.setColour (juce::Colour (0xff39ffb0));
            g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (over ? juce::Colour (0xff39ffb0) : juce::Colour (0xffeafff6));
        g.setFont (juce::Font ("Segoe UI", 15.0f, juce::Font::bold).withExtraKerningFactor (0.08f));
        g.drawText ("LISTEN", b.translated (0.0f, lst ? -6.0f : 0.0f), juce::Justification::centred);
        if (lst)
        {
            g.setColour (juce::Colour (0xff9fe1cb)); g.setFont (9.5f);
            g.drawText ("analysing " + juce::String ((int) (progress() * 100.0f)) + "%",
                        b.translated (0.0f, 12.0f), juce::Justification::centred);
        }
    }
};

// VOORSTEL list: one toggleable card per proposed move + a COMP card.
class ProposalView : public juce::Component
{
public:
    std::function<void()> onToggle;

    void clearProposal() { items.clear(); have = false; repaint(); }

    void setProposal (const assist::Proposal& pr, int instrument)
    {
        items.clear();
        for (int i = 0; i < (int) pr.eq.size(); ++i)
        {
            const auto& m = pr.eq[(size_t) i];
            Item it; it.kind = 0; it.index = i; it.enabled = true; it.colour = assistCardColour (i);
            it.title  = juce::String::fromUTF8 ("EQ \xe2\x80\x93 ") + moveTitle (m);
            it.detail = juce::String (m.gain >= 0 ? "+" : "") + juce::String (m.gain, 1) + " dB @ "
                      + freqStr (m.freq) + juce::String::fromUTF8 ("  \xc2\xb7  Q ") + juce::String (m.q, 1);
            items.push_back (it);
        }
        { Item it; it.kind = 1; it.enabled = pr.hpfOn; it.colour = juce::Colour (0xff9fe1cb);
          it.title = juce::String::fromUTF8 ("EQ \xe2\x80\x93 HPF"); it.detail = pr.hpfText; items.push_back (it); }
        { Item it; it.kind = 2; it.enabled = true; it.colour = juce::Colour (0xff39ffb0); it.title = compName (instrument);
          it.detail = pr.compText.replaceCharacter ('\n', ' '); items.push_back (it); }
        { Item it; it.kind = 3; it.enabled = pr.limOn; it.colour = juce::Colour (0xff39ffb0); it.title = "Limiter";
          it.detail = pr.limText; items.push_back (it); }
        have = true; resized(); repaint();
    }

    bool enabledFor (int kind, int index) const
    {
        for (auto& it : items) if (it.kind == kind && (kind != 0 || it.index == index)) return it.enabled;
        return false;
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (area, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (area, 6.0f, 1.0f);
        g.setColour (juce::Colour (0xff5dcaa5));
        g.setFont (juce::Font ("Segoe UI", 10.5f, juce::Font::bold).withExtraKerningFactor (0.14f));
        g.drawText ("VOORSTEL", 14, 10, (int) area.getWidth() - 28, 16, juce::Justification::centredLeft);

        if (! have)
        {
            g.setColour (juce::Colour (0xff6c7a74)); g.setFont (11.0f);
            g.drawText ("Druk LISTEN om een voorstel te maken.", area.reduced (14.0f).withTrimmedTop (20.0f),
                        juce::Justification::centredTop);
            return;
        }

        for (auto& it : items)
        {
            auto r = it.bounds.toFloat();
            if (it.kind == 2)   // COMP sub-header above the comp card
            {
                g.setColour (juce::Colour (0xff5dcaa5));
                g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::bold).withExtraKerningFactor (0.12f));
                g.drawText ("COMP", (int) r.getX(), (int) r.getY() - 20, 80, 14, juce::Justification::centredLeft);
            }
            g.setColour (it.enabled ? juce::Colour (0xff12201b) : juce::Colour (0xff101317));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (it.enabled ? it.colour.withAlpha (0.85f) : juce::Colour (0xff23272c));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
            g.setColour (it.enabled ? it.colour : juce::Colour (0xff3a4149));
            g.fillEllipse (r.getX() + 10.0f, r.getY() + 13.0f, 8.0f, 8.0f);
            g.setColour (it.enabled ? juce::Colour (0xffeafff6) : juce::Colour (0xff6c7a74));
            g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
            g.drawText (it.title + (it.enabled ? juce::String() : juce::String ("  (uit)")),
                        (int) r.getX() + 26, (int) r.getY() + 6, (int) r.getWidth() - 34, 16, juce::Justification::centredLeft);
            g.setColour (it.enabled ? juce::Colour (0xff9fb0aa) : juce::Colour (0xff5a6460));
            g.setFont (10.0f);
            g.drawFittedText (it.detail, (int) r.getX() + 26, (int) r.getY() + 22, (int) r.getWidth() - 34,
                              (int) r.getHeight() - 26, juce::Justification::topLeft, 2);
        }
    }

    void resized() override
    {
        int y = 34;
        for (auto& it : items)
        {
            if (it.kind == 2) y += 22;                 // space for COMP header
            const int h = (it.kind == 2) ? 56 : 44;
            it.bounds = { 12, y, getWidth() - 24, h };
            y += h + 8;
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& it : items)
            if (it.bounds.contains (e.getPosition())) { it.enabled = ! it.enabled; repaint(); if (onToggle) onToggle(); return; }
    }

private:
    struct Item { int kind = 0; int index = 0; bool enabled = true; juce::String title, detail; juce::Colour colour { 0xff39ffb0 }; juce::Rectangle<int> bounds; };
    std::vector<Item> items;
    bool have = false;

    static juce::String freqStr (float f)
    { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) f) + " Hz"; }
    static juce::String moveTitle (const assist::EqMove& m)
    {
        const juce::String band = m.freq < 120.0f ? "Low" : m.freq < 350.0f ? "Low-mid"
                                : m.freq < 1500.0f ? "Mid" : m.freq < 5000.0f ? "High-mid" : "High";
        return band + (m.gain < 0.0f ? " dip" : " lift");
    }
    static juce::String compName (int instrument)
    {
        static const char* n[] = { "Bass-glue", "Vocal-glue", "Drum-glue", "Guitar-glue", "Synth-glue", "Master-glue" };
        return n[juce::jlimit (0, 5, instrument)];
    }
};

class AssistPanel : public juce::Component, private juce::Timer
{
public:
    explicit AssistPanel (CohesionEQCompAudioProcessor& p) : proc (p), center (p)
    {
        auto lab = [this] (juce::Label& l, const juce::String& t, float sz, juce::Colour c)
        { l.setText (t, juce::dontSendNotification); l.setFont (juce::Font ("Segoe UI", sz, juce::Font::plain)); l.setColour (juce::Label::textColourId, c); addAndMakeVisible (l); };

        lab (targetL, "TARGET", 10.5f, juce::Colour (0xff5dcaa5));
        lab (instrL,  "Instrument", 11.0f, juce::Colour (0xff9fb0aa));
        instr.addItemList ({ "Bass (electric)", "Vocal", "Drums", "Guitar", "Synth", "Master" }, 1);
        instr.setSelectedId (6, juce::dontSendNotification); addAndMakeVisible (instr);   // default: Master
        lab (styleL, "Style", 11.0f, juce::Colour (0xff9fb0aa));
        style.addItemList ({ "Modern / tight", "Vintage warm", "Natural", "Aggressive",
                             "Smooth / dark", "Bright / air", "Lo-fi", "Open / airy", "Punchy", "Hi-fi master" }, 1);
        style.setSelectedId (1, juce::dontSendNotification); addAndMakeVisible (style);
        instr.onChange = [this] { center.reanalyze(); };   // update doel/voorstel-curve on change
        style.onChange = [this] { center.reanalyze(); };

        addAndMakeVisible (listen);
        listen.onClick   = [this] { center.startListen(); };
        listen.listening = [this] { return center.isListening(); };
        listen.progress  = [this] { return center.progress(); };

        lab (strengthL, "Strength", 11.0f, juce::Colour (0xff9fb0aa));
        strength.setSliderStyle (juce::Slider::LinearHorizontal);
        strength.setRange (0.0, 100.0, 1.0); strength.setValue (70.0);
        strength.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
        strength.setTextValueSuffix (" %"); addAndMakeVisible (strength);

        lab (applyL, "Toepassen op", 11.0f, juce::Colour (0xff9fb0aa));
        eqOn.setButtonText ("EQ"); eqOn.setToggleState (true, juce::dontSendNotification);
        compOn.setButtonText ("Comp"); compOn.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (eqOn); addAndMakeVisible (compOn);

        applyBtn.setButtonText ("APPLY");
        applyBtn.setEnabled (false);
        applyBtn.onClick = [this] { applyProposal(); applied = true;
                                    previewBtn.setToggleState (false, juce::dontSendNotification); updateButtons(); };
        addAndMakeVisible (applyBtn);
        previewBtn.setButtonText ("Preview");
        previewBtn.setClickingTogglesState (true);
        previewBtn.onClick = [this]
        {
            if (previewBtn.getToggleState()) applyProposal();
            else if (baseline.isValid()) proc.apvts.replaceState (baseline.createCopy());
            updateButtons();
        };
        addAndMakeVisible (previewBtn);
        revertBtn.setButtonText ("Revert");
        revertBtn.onClick = [this] { if (baseline.isValid()) proc.apvts.replaceState (baseline.createCopy());
                                     applied = false; previewBtn.setToggleState (false, juce::dontSendNotification); updateButtons(); };
        addAndMakeVisible (revertBtn);
        updateButtons();

        center.getInstrument = [this] { return instr.getSelectedId() - 1; };
        center.getStyle      = [this] { return style.getSelectedId() - 1; };
        center.getStrength   = [this] { return (float) strength.getValue(); };
        center.getInstrName  = [this] { return instr.getText(); };
        center.onProposal    = [this]
        {
            const bool live = previewBtn.getToggleState() || applied;   // already sounding -> refresh live
            if (! live) { baseline = proc.apvts.copyState(); applied = false; }
            propView.setProposal (center.proposal(), instr.getSelectedId() - 1);
            applyBtn.setEnabled (true);
            updateButtons();
            if (live)
            {
                if (baseline.isValid()) proc.apvts.replaceState (baseline.createCopy());
                applyProposal();
            }
        };
        addAndMakeVisible (center);
        addAndMakeVisible (propView);
        // toggling a card updates the sound live while previewing
        propView.onToggle = [this]
        {
            if (previewBtn.getToggleState() || applied)   // live update when applied or previewing
            {
                if (baseline.isValid()) proc.apvts.replaceState (baseline.createCopy());
                applyProposal();
            }
        };

        note.setJustificationType (juce::Justification::topLeft);
        note.setColour (juce::Label::textColourId, juce::Colour (0xff6c7a74));
        note.setFont (10.0f);
        note.setText (juce::String::fromUTF8 ("APPLY zet het voorstel op de EQ- en MULTIBAND-schermen. "
                                              "Preview schakelt het tijdelijk aan/uit om te vergelijken. Revert zet terug."),
                      juce::dontSendNotification);
        addAndMakeVisible (note);
        startTimerHz (12);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (leftPanel.toFloat(), 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (leftPanel.toFloat(), 6.0f, 1.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        leftPanel = r.removeFromLeft (250);
        r.removeFromLeft (8);
        auto rightCol = r.removeFromRight (230);
        r.removeFromRight (8);
        center.setBounds (r);

        auto L = leftPanel.reduced (14);
        targetL.setBounds (L.removeFromTop (18)); L.removeFromTop (8);
        instrL.setBounds (L.removeFromTop (14)); instr.setBounds (L.removeFromTop (28)); L.removeFromTop (10);
        styleL.setBounds (L.removeFromTop (14)); style.setBounds (L.removeFromTop (28)); L.removeFromTop (16);
        listen.setBounds (L.removeFromTop (104).withSizeKeepingCentre (104, 104)); L.removeFromTop (14);
        { auto sr = L.removeFromTop (16); strengthL.setBounds (sr); } L.removeFromTop (2);
        strength.setBounds (L.removeFromTop (22)); L.removeFromTop (14);
        applyL.setBounds (L.removeFromTop (14)); L.removeFromTop (2);
        { auto tog = L.removeFromTop (28);
          eqOn.setBounds (tog.removeFromLeft (tog.getWidth() / 2 - 4)); tog.removeFromLeft (8); compOn.setBounds (tog); }
        L.removeFromTop (12);
        applyBtn.setBounds (L.removeFromTop (34)); L.removeFromTop (8);
        { auto pr = L.removeFromTop (28);
          previewBtn.setBounds (pr.removeFromLeft (pr.getWidth() / 2 - 4)); pr.removeFromLeft (8); revertBtn.setBounds (pr); }

        propView.setBounds (rightCol);
        note.setBounds (rightCol.reduced (12).removeFromBottom (54));
    }

private:
    void timerCallback() override
    {
        if (center.isListening()) listen.repaint();
    }

    void updateButtons()
    {
        applyBtn.setColour (juce::TextButton::buttonColourId,  applied ? juce::Colour (0xff39ffb0) : juce::Colour (0xff0e1013));
        applyBtn.setColour (juce::TextButton::textColourOffId, applied ? juce::Colour (0xff07130f) : juce::Colour (0xff9fe1cb));
        const bool prev = previewBtn.getToggleState();
        previewBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff0e1013));
        previewBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff39ffb0));
        previewBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff9fe1cb));
        previewBtn.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff07130f));
        juce::ignoreUnused (prev);
        applyBtn.repaint(); previewBtn.repaint();
    }

    void setP (const juce::String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); }

    // Deterministic: writes the full EQ/comp/limiter state from the enabled cards
    // (it both enables AND disables), so toggling any card gives a clear difference.
    void applyProposal()
    {
        const auto& pr = center.proposal();
        if (! pr.valid) return;

        // ---- EQ section ----
        const bool eqMaster = eqOn.getToggleState();
        setP ("eqOn", eqMaster ? 1.0f : 0.0f);
        if (eqMaster)
        {
            const bool hpf = pr.hpfOn && propView.enabledFor (1, 0);
            setP ("hpOn", hpf ? 1.0f : 0.0f);
            if (hpf) setP ("hpFreq", pr.hpf);

            int slot = 0;
            for (int i = 0; i < (int) pr.eq.size() && slot < 6; ++i, ++slot)
            {
                const juce::String b = "b" + juce::String (slot) + "_";
                if (propView.enabledFor (0, i))
                {
                    const auto& m = pr.eq[(size_t) i];
                    setP (b + "on", 1.0f); setP (b + "freq", m.freq); setP (b + "gain", m.gain);
                    setP (b + "q", m.q);   setP (b + "type", 0.0f);
                }
                else setP (b + "on", 0.0f);
            }
            for (int b = slot; b < 6; ++b) setP ("b" + juce::String (b) + "_on", 0.0f);   // clear unused bands
        }

        // ---- COMP section (multiband) ---- per-band settings from the measured spectrum
        const bool compMaster = compOn.getToggleState();
        if (compMaster && propView.enabledFor (2, 0))
        {
            setP ("mbOn", 1.0f);
            const int nb = 2 + (int) proc.apvts.getRawParameterValue ("mbBands")->load();
            const float s = (float) strength.getValue() * 0.01f;
            const float xf[4] = { proc.apvts.getRawParameterValue ("x1")->load(),
                                  proc.apvts.getRawParameterValue ("x2")->load(),
                                  proc.apvts.getRawParameterValue ("x3")->load(),
                                  proc.apvts.getRawParameterValue ("x4")->load() };
            float L = 0.0f; for (int i = 0; i < assist::kBands; ++i) L += pr.measAbs[i]; L /= (float) assist::kBands;

            for (int b = 0; b < nb; ++b)
            {
                const float fLo = (b == 0)      ? 20.0f    : xf[b - 1];
                const float fHi = (b == nb - 1) ? 20000.0f : xf[b];
                float lvl = 0.0f; int cnt = 0;
                for (int i = 0; i < assist::kBands; ++i)
                    if (assist::kFreq[i] >= fLo && assist::kFreq[i] <= fHi) { lvl += pr.measAbs[i]; ++cnt; }
                if (cnt == 0)   // no 8-band point inside this range -> nearest to band centre
                {
                    const float fc = std::sqrt (fLo * fHi); int best = 0; float bd = 1.0e9f;
                    for (int i = 0; i < assist::kBands; ++i) { const float d = std::abs (assist::kFreq[i] - fc); if (d < bd) { bd = d; best = i; } }
                    lvl = pr.measAbs[best]; cnt = 1;
                }
                lvl /= (float) cnt;
                const float rel    = juce::jlimit (-12.0f, 12.0f, lvl - L);                 // louder/quieter than average
                const float thr    = juce::jlimit (-42.0f, -6.0f, -16.0f - rel * 0.8f - s * 8.0f);
                const float ratio  = juce::jlimit (1.2f, 8.0f, pr.ratio + juce::jmax (0.0f, rel) * 0.15f + s * 1.5f);
                const float makeup = juce::jlimit (0.0f, 9.0f, (-thr - 10.0f) * 0.35f);

                const juce::String m = "mb" + juce::String (b) + "_";
                setP (m + "solo", 0.0f); setP (m + "byp", 0.0f); setP (m + "mute", 0.0f);
                setP (m + "mode", 0.0f); setP (m + "thr", thr); setP (m + "ratio", ratio);
                setP (m + "atk", pr.atk);  setP (m + "rel", pr.rel);
                setP (m + "gain", makeup); setP (m + "width", 6.0f);
            }
        }
        else setP ("mbOn", 0.0f);

        // ---- limiter (push drives loudness into the ceiling) ----
        if (compMaster && propView.enabledFor (3, 0))
        {
            setP ("limOn", 1.0f); setP ("limCeil", pr.limCeil); setP ("limPush", pr.limPush); setP ("limRel", 60.0f);
        }
        else setP ("limOn", 0.0f);
    }

    CohesionEQCompAudioProcessor& proc;
    juce::Label targetL, instrL, styleL, strengthL, applyL, note;
    juce::ComboBox instr, style;
    ListenButton listen;
    juce::TextButton applyBtn, previewBtn, revertBtn;
    juce::Slider strength;
    juce::ToggleButton eqOn, compOn;
    AssistCenter center;
    ProposalView propView;
    juce::ValueTree baseline;
    juce::Rectangle<int> leftPanel;
    bool applied = false;
};

// Round "?" button, bottom-right of the editor.
class RoundHelpButton : public juce::Button
{
public:
    RoundHelpButton() : juce::Button ("help") {}
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff15181c)); g.fillEllipse (b);
        g.setColour (over ? juce::Colour (0xff39ffb0) : juce::Colour (0xff0f6e56)); g.drawEllipse (b, 1.6f);
        g.setColour (over ? juce::Colour (0xff39ffb0) : juce::Colour (0xff9fe1cb));
        g.setFont (juce::Font ("Segoe UI", 15.0f, juce::Font::bold));
        g.drawText ("?", b, juce::Justification::centred);
    }
};

// One help window with a column of help per tab.
class HelpOverlay : public juce::Component
{
public:
    HelpOverlay()
    {
        eqHelp = juce::String::fromUTF8 (
            "Parametrische EQ met 6 banden plus HPF en LPF.\n\n"
            "\xe2\x80\xa2 Sleep een punt in de grafiek: links/rechts = frequentie, omhoog/omlaag = versterking.\n"
            "\xe2\x80\xa2 Scrollwiel op een punt wijzigt de Q (bandbreedte).\n"
            "\xe2\x80\xa2 Dubbelklik zet een band aan/uit.\n"
            "\xe2\x80\xa2 Per band: Bell, Low Shelf of High Shelf.\n"
            "\xe2\x80\xa2 HPF/LPF ruimen laag/hoog op.\n\n"
            "Het grijze vlak is het ingangssignaal (voor), het teal vlak het resultaat (na). "
            "De filters werken zero-delay-feedback voor een schone, stabiele toon.");
        mbHelp = juce::String::fromUTF8 (
            "Multiband-dynamiek splitst het signaal in 2\xe2\x80\x93" "5 banden met Linkwitz-Riley-crossovers.\n\n"
            "\xe2\x80\xa2 Stel het aantal banden in met BANDS in de header.\n"
            "\xe2\x80\xa2 Sleep de verticale lijnen in de scoop om de crossover-frequenties te zetten.\n"
            "\xe2\x80\xa2 Per band: COMP, EXP of GATE met Threshold, Ratio, Attack, Release, Width en Gain.\n"
            "\xe2\x80\xa2 De horizontale lijn toont de threshold; Width = knie (COMP) of range (EXP/GATE).\n"
            "\xe2\x80\xa2 Solo, Mute en Bypass per band; de meter toont de gain reduction.\n\n"
            "Bandkleuren komen overeen met de sliders en meters.");
        asHelp = juce::String::fromUTF8 (
            "ASSIST luistert naar je signaal en stelt EQ- en comp-zetten voor per instrument.\n\n"
            "\xe2\x80\xa2 Kies Instrument en Style, druk LISTEN \xe2\x80\x94 de grafiek beweegt mee tijdens het luisteren.\n"
            "\xe2\x80\xa2 Daarna zie je huidig signaal, doelcurve en voorstel, met probleemzones in rood.\n"
            "\xe2\x80\xa2 Strength bepaalt hoe sterk er wordt gecorrigeerd.\n"
            "\xe2\x80\xa2 In VOORSTEL klik je elke zet los aan/uit.\n"
            "\xe2\x80\xa2 Preview schakelt het voorstel tijdelijk aan/uit om te vergelijken.\n"
            "\xe2\x80\xa2 APPLY zet het voorstel op de EQ- en MULTIBAND-schermen; Revert zet terug.");

        closeBtn.setButtonText (juce::String::fromUTF8 ("Sluiten"));
        closeBtn.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeBtn);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.62f));
        auto card = getLocalBounds().reduced (46, 40).toFloat();
        g.setColour (juce::Colour (0xff15181c)); g.fillRoundedRectangle (card, 10.0f);
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.45f)); g.drawRoundedRectangle (card, 10.0f, 1.5f);
        g.setColour (juce::Colour (0xff39ffb0));
        g.setFont (juce::Font ("Segoe UI", 18.0f, juce::Font::bold).withExtraKerningFactor (0.12f));
        g.drawText (juce::String::fromUTF8 ("COHESION - EQ/COMP  \xc2\xb7  HELP"), (int) card.getX() + 22, (int) card.getY() + 16,
                    (int) card.getWidth() - 44, 24, juce::Justification::centredLeft);

        auto body = card.reduced (22.0f); body.removeFromTop (48.0f); body.removeFromBottom (40.0f);
        const int gap = 18;
        const int cw = (int) ((body.getWidth() - 2 * gap) / 3.0f);
        drawCol (g, "EQ",        eqHelp, (int) body.getX(),                   (int) body.getY(), cw, (int) body.getHeight());
        drawCol (g, "MULTIBAND", mbHelp, (int) body.getX() + cw + gap,        (int) body.getY(), cw, (int) body.getHeight());
        drawCol (g, "ASSIST",    asHelp, (int) body.getX() + 2 * (cw + gap),  (int) body.getY(), cw, (int) body.getHeight());
    }

    void resized() override
    {
        auto card = getLocalBounds().reduced (46, 40);
        closeBtn.setBounds (card.getRight() - 96, card.getBottom() - 34, 80, 24);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().reduced (46, 40).contains (e.getPosition())) setVisible (false);
    }

private:
    void drawCol (juce::Graphics& g, const juce::String& title, const juce::String& text, int x, int y, int w, int h)
    {
        g.setColour (juce::Colour (0xff5dcaa5));
        g.setFont (juce::Font ("Segoe UI", 14.0f, juce::Font::bold).withExtraKerningFactor (0.12f));
        g.drawText (title, x, y, w, 20, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff2a3036)); g.fillRect (x, y + 24, w, 1);
        g.setColour (juce::Colour (0xffcdd6d1)); g.setFont (juce::Font ("Segoe UI", 13.0f, juce::Font::plain));
        g.drawFittedText (text, x, y + 32, w, h - 32, juce::Justification::topLeft, 40);
    }

    juce::TextButton closeBtn;
    juce::String eqHelp, mbHelp, asHelp;
};

// ===========================================================================
class CohesionEQCompAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit CohesionEQCompAudioProcessorEditor (CohesionEQCompAudioProcessor&);
    ~CohesionEQCompAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 980;
    static constexpr int kBaseH = 640;

private:
    void showTab (int t);
    void layoutContent();
    void timerCallback() override;
    juce::String onTargetId() const { return tab == 0 ? "eqOn" : tab == 1 ? "mbOn" : juce::String(); }
    void setParam (const juce::String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); }

    CohesionEQCompAudioProcessor& proc;
    CohesionLookAndFeel lnf;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::TooltipWindow> tooltip;
    juce::Component content;

    juce::Label title, subtitle;
    cohesion::UpdateBadge updateBadge;   // UPDATE AVAILABLE (start-up version check)
    juce::TextButton eqTab { "EQ" }, mbTab { "MULTIBAND" }, asTab { "ASSIST" };
    juce::Label bandsL;
    juce::TextButton bandsBtn[4];
    juce::ToggleButton onBtn { "ON" };
    juce::TextButton chainBtn;
    RoundHelpButton helpBtn;
    std::unique_ptr<HelpOverlay> help;
    juce::Slider inS, outS;
    juce::Label inL, outL;

    EqPanel eqPanel { proc };
    MultibandPanel mbPanel { proc };
    AssistPanel asPanel { proc };
    int tab = 0;

    std::vector<std::unique_ptr<SliderAtt>> sAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionEQCompAudioProcessorEditor)
};
