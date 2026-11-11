#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"

// These rely on the typedefs + LabeledKnob defined in PluginEditor.h (included first).

// ---------------------------------------------------------------------------
//  Small titled card base.
// ---------------------------------------------------------------------------
struct Card : juce::Component
{
    juce::String title;
    juce::Colour titleCol { 0xffe8eef0 };
    explicit Card (juce::String t) : title (std::move (t)) {}
    void paintCard (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (titleCol);
        g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        g.drawText (title, 14, 10, getWidth() - 28, 16, juce::Justification::centredLeft);
    }
    void paint (juce::Graphics& g) override { paintCard (g); }
};

// ---------------------------------------------------------------------------
//  Filter panel (ladder or SVF).
// ---------------------------------------------------------------------------
struct FilterPanel : Card
{
    juce::ComboBox typeBox, envSrcBox, slopeBox;
    juce::Label envSrcLbl, slopeLbl;
    std::unique_ptr<ComboAtt> typeAtt, envSrcAtt, slopeAtt;
    juce::OwnedArray<LabeledKnob> knobs;
    bool hasSlope;

    FilterPanel (juce::AudioProcessorValueTreeState& apvts, const juce::String& t,
                 const juce::StringArray& typeItems,
                 const juce::String& typeId, const juce::String& cut, const juce::String& res,
                 const juce::String& drv, const juce::String& kt, const juce::String& envamt,
                 const juce::String& envsrc, const juce::String& slopeId)
        : Card (t), hasSlope (slopeId.isNotEmpty())
    {
        typeBox.addItemList (typeItems, 1); addAndMakeVisible (typeBox);
        typeAtt = std::make_unique<ComboAtt> (apvts, typeId, typeBox);

        auto add = [&] (const juce::String& id, const juce::String& nm)
        { auto* k = new LabeledKnob (apvts, id, nm); knobs.add (k); addAndMakeVisible (k); };
        add (cut, "CUTOFF"); add (res, "RES"); add (drv, "DRIVE"); add (kt, "KEY TRK"); add (envamt, "ENV AMT");

        envSrcLbl.setText ("ENV SRC", juce::dontSendNotification);
        envSrcLbl.setFont (juce::Font ("Segoe UI", 8.5f, juce::Font::plain));
        envSrcLbl.setColour (juce::Label::textColourId, juce::Colour (0xff6c7a74));
        addAndMakeVisible (envSrcLbl);
        envSrcBox.addItemList ({ "ENV 1", "ENV 2", "ENV 3", "ENV 4" }, 1); addAndMakeVisible (envSrcBox);
        envSrcAtt = std::make_unique<ComboAtt> (apvts, envsrc, envSrcBox);

        if (hasSlope)
        {
            slopeLbl.setText ("SLOPE", juce::dontSendNotification);
            slopeLbl.setFont (juce::Font ("Segoe UI", 8.5f, juce::Font::plain));
            slopeLbl.setColour (juce::Label::textColourId, juce::Colour (0xff6c7a74));
            addAndMakeVisible (slopeLbl);
            slopeBox.addItemList ({ "12 dB", "24 dB" }, 1); addAndMakeVisible (slopeBox);
            slopeAtt = std::make_unique<ComboAtt> (apvts, slopeId, slopeBox);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (22);
        typeBox.setBounds (r.removeFromTop (24));
        r.removeFromTop (8);
        auto krow = r.removeFromTop (72);
        const int cw = krow.getWidth() / 3;
        for (int i = 0; i < 3; ++i) knobs[i]->setBounds (krow.getX() + i * cw, krow.getY(), cw, krow.getHeight());
        r.removeFromTop (4);
        auto krow2 = r.removeFromTop (72);
        knobs[3]->setBounds (krow2.removeFromLeft (cw));
        knobs[4]->setBounds (krow2.removeFromLeft (cw));
        auto cm = krow2;
        envSrcLbl.setBounds (cm.getX(), cm.getY() + 6, cm.getWidth(), 14);
        envSrcBox.setBounds (cm.getX(), cm.getY() + 22, cm.getWidth() - 4, 22);
        if (hasSlope)
        {
            slopeLbl.setBounds (cm.getX(), cm.getY() + 46, cm.getWidth(), 14);
            slopeBox.setBounds (cm.getX(), cm.getY() + 62, cm.getWidth() - 4, 22);
        }
    }
};

// ---------------------------------------------------------------------------
//  Routing panel (serial / parallel + balance).
// ---------------------------------------------------------------------------
struct RoutingPanel : Card
{
    juce::ComboBox routeBox;
    std::unique_ptr<ComboAtt> routeAtt;
    std::unique_ptr<LabeledKnob> balance;

    RoutingPanel (juce::AudioProcessorValueTreeState& apvts) : Card ("ROUTING")
    {
        titleCol = juce::Colour (0xff5dcaa5);
        routeBox.addItemList ({ "Serial  F1 > F2", "Parallel" }, 1); addAndMakeVisible (routeBox);
        routeAtt = std::make_unique<ComboAtt> (apvts, pid::fltRouting, routeBox);
        balance = std::make_unique<LabeledKnob> (apvts, pid::fltBalance, "BALANCE  F1>F2"); addAndMakeVisible (*balance);
    }

    void paint (juce::Graphics& g) override
    {
        paintCard (g);
        auto gr = juce::Rectangle<float> (14, 34, (float) getWidth() - 28, 96.0f);
        g.setColour (juce::Colour (0xff0a0c0f)); g.fillRoundedRectangle (gr, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) gr.getCentreY(), gr.getX(), gr.getRight());
        // illustrative dual filter curves
        juce::Path p1, p2;
        const float y0 = gr.getCentreY();
        p1.startNewSubPath (gr.getX(), y0 - 6);
        p1.cubicTo (gr.getX() + gr.getWidth() * 0.5f, y0 - 24, gr.getX() + gr.getWidth() * 0.62f, y0 + 10, gr.getRight(), gr.getBottom() - 6);
        g.setColour (juce::Colour (0xff39ffb0)); g.strokePath (p1, juce::PathStrokeType (2.0f));
        p2.startNewSubPath (gr.getX(), gr.getBottom() - 8);
        p2.cubicTo (gr.getX() + gr.getWidth() * 0.35f, y0 - 4, gr.getX() + gr.getWidth() * 0.5f, y0 - 22, gr.getRight(), y0 + 4);
        g.setColour (juce::Colour (0xff5aa6d8)); g.strokePath (p2, juce::PathStrokeType (1.6f));
        g.setColour (juce::Colour (0xff39ffb0)); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain)); g.drawText ("F1", (int) gr.getX() + 6, (int) gr.getY() + 4, 24, 12, juce::Justification::left);
        g.setColour (juce::Colour (0xff5aa6d8)); g.drawText ("F2", (int) gr.getX() + 28, (int) gr.getY() + 4, 24, 12, juce::Justification::left);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (22 + 96 + 8);
        routeBox.setBounds (r.removeFromLeft (200).removeFromTop (26));
        balance->setBounds (r.removeFromRight (130).withHeight (80));
    }
};

// ---------------------------------------------------------------------------
//  Envelope panel: 4 selectable ADSR envelopes (ENV 1 = AMP).
// ---------------------------------------------------------------------------
struct EnvPanel : Card
{
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton sel[4];
    juce::OwnedArray<LabeledKnob> knobs;
    int cur = 0;

    EnvPanel (juce::AudioProcessorValueTreeState& a) : Card ("ENVELOPES"), apvts (a)
    {
        titleCol = juce::Colour (0xff5dcaa5);
        const char* names[] = { "AMP", "ENV 2", "ENV 3", "ENV 4" };
        for (int i = 0; i < 4; ++i)
        {
            sel[i].setButtonText (names[i]);
            sel[i].onClick = [this, i] { build (i); };
            addAndMakeVisible (sel[i]);
        }
        build (0);
    }

    void idsFor (int n, juce::String& a, juce::String& d, juce::String& s, juce::String& r)
    {
        if (n == 0) { a = pid::ampA; d = pid::ampD; s = pid::ampS; r = pid::ampR; }
        else        { a = pid::envA (n + 1); d = pid::envD (n + 1); s = pid::envS (n + 1); r = pid::envR (n + 1); }
    }

    void build (int n)
    {
        cur = n;
        knobs.clear (true);
        juce::String a, d, s, r; idsFor (n, a, d, s, r);
        auto add = [&] (const juce::String& id, const juce::String& nm) { auto* k = new LabeledKnob (apvts, id, nm); knobs.add (k); addAndMakeVisible (k); };
        add (a, "ATK"); add (d, "DEC"); add (s, "SUS"); add (r, "REL");
        for (int i = 0; i < 4; ++i)
        {
            sel[i].setColour (juce::TextButton::buttonColourId, i == n ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
            sel[i].setColour (juce::TextButton::textColourOffId, i == n ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        }
        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (22);
        auto selRow = r.removeFromTop (26);
        const int bw = selRow.getWidth() / 4;
        for (int i = 0; i < 4; ++i) sel[i].setBounds (selRow.getX() + i * bw + 2, selRow.getY(), bw - 4, 26);
        r.removeFromTop (10);
        const int cw = r.getWidth() / juce::jmax (1, knobs.size());
        for (int i = 0; i < knobs.size(); ++i) knobs[i]->setBounds (r.getX() + i * cw, r.getY(), cw, r.getHeight());
    }
};

// ---------------------------------------------------------------------------
//  LFO panel: 4 selectable LFOs.
// ---------------------------------------------------------------------------
struct LfoPanel : Card
{
    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton sel[4];
    juce::ComboBox waveBox;
    juce::ToggleButton syncBtn, monoBtn;
    std::unique_ptr<ComboAtt> waveAtt;
    std::unique_ptr<ButtonAtt> syncAtt, monoAtt;
    juce::OwnedArray<LabeledKnob> knobs;
    int cur = 0;

    LfoPanel (juce::AudioProcessorValueTreeState& a) : Card ("LFO"), apvts (a)
    {
        titleCol = juce::Colour (0xff5dcaa5);
        for (int i = 0; i < 4; ++i)
        {
            sel[i].setButtonText ("LFO " + juce::String (i + 1));
            sel[i].onClick = [this, i] { build (i); };
            addAndMakeVisible (sel[i]);
        }
        addAndMakeVisible (waveBox);
        syncBtn.setButtonText ("SYNC"); addAndMakeVisible (syncBtn);
        monoBtn.setButtonText ("MONO"); addAndMakeVisible (monoBtn);
        build (0);
    }

    void build (int n)
    {
        cur = n;
        const int lfo = n + 1;
        waveBox.clear (juce::dontSendNotification);
        waveBox.addItemList ({ "Sine", "Triangle", "Saw", "Square", "S&H" }, 1);
        waveAtt = std::make_unique<ComboAtt> (apvts, pid::lfoWave (lfo), waveBox);
        syncAtt = std::make_unique<ButtonAtt> (apvts, pid::lfoSync (lfo), syncBtn);
        monoAtt = std::make_unique<ButtonAtt> (apvts, pid::lfoMono (lfo), monoBtn);

        knobs.clear (true);
        auto add = [&] (const juce::String& id, const juce::String& nm) { auto* k = new LabeledKnob (apvts, id, nm); knobs.add (k); addAndMakeVisible (k); };
        add (pid::lfoRate (lfo), "RATE"); add (pid::lfoFade (lfo), "FADE IN"); add (pid::lfoPhase (lfo), "PHASE"); add (pid::lfoSmooth (lfo), "SMOOTH");

        for (int i = 0; i < 4; ++i)
        {
            sel[i].setColour (juce::TextButton::buttonColourId, i == n ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
            sel[i].setColour (juce::TextButton::textColourOffId, i == n ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        }
        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (22);
        auto selRow = r.removeFromTop (26);
        const int bw = selRow.getWidth() / 4;
        for (int i = 0; i < 4; ++i) sel[i].setBounds (selRow.getX() + i * bw + 2, selRow.getY(), bw - 4, 26);
        r.removeFromTop (8);
        auto wrow = r.removeFromTop (26);
        waveBox.setBounds (wrow.removeFromLeft (wrow.getWidth() - 120));
        wrow.removeFromLeft (6);
        syncBtn.setBounds (wrow.removeFromLeft (56));
        wrow.removeFromLeft (4);
        monoBtn.setBounds (wrow);
        r.removeFromTop (8);
        const int cw = r.getWidth() / juce::jmax (1, knobs.size());
        for (int i = 0; i < knobs.size(); ++i) knobs[i]->setBounds (r.getX() + i * cw, r.getY(), cw, r.getHeight());
    }
};

// ---------------------------------------------------------------------------
//  Mod matrix: 8 rows of source -> dest : amount.
// ---------------------------------------------------------------------------
struct ModMatrixPanel : Card
{
    struct Row
    {
        juce::ComboBox src, dst;
        juce::Slider amt;
        std::unique_ptr<ComboAtt> srcAtt, dstAtt;
        std::unique_ptr<SliderAtt> amtAtt;
    };
    juce::OwnedArray<Row> rows;

    ModMatrixPanel (juce::AudioProcessorValueTreeState& apvts) : Card ("MOD MATRIX")
    {
        titleCol = juce::Colour (0xff5dcaa5);
        for (int i = 0; i < 8; ++i)
        {
            auto* row = new Row();
            row->src.addItemList (pid::modSources, 1); addAndMakeVisible (row->src);
            row->dst.addItemList (pid::modDests, 1);   addAndMakeVisible (row->dst);
            row->amt.setSliderStyle (juce::Slider::LinearHorizontal);
            row->amt.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible (row->amt);
            row->srcAtt = std::make_unique<ComboAtt>  (apvts, pid::modSrc (i), row->src);
            row->dstAtt = std::make_unique<ComboAtt>  (apvts, pid::modDst (i), row->dst);
            row->amtAtt = std::make_unique<SliderAtt> (apvts, pid::modAmt (i), row->amt);
            rows.add (row);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (24);
        const int rh = r.getHeight() / 8;
        for (int i = 0; i < rows.size(); ++i)
        {
            auto row = r.removeFromTop (rh).reduced (0, 2);
            auto* R = rows[i];
            R->src.setBounds (row.removeFromLeft (120));
            row.removeFromLeft (6);
            R->dst.setBounds (row.removeFromLeft (140));
            row.removeFromLeft (8);
            R->amt.setBounds (row);
        }
    }
};

// ---------------------------------------------------------------------------
//  The whole FILTER / MOD tab.
// ---------------------------------------------------------------------------
struct FilterModTab : juce::Component
{
    std::unique_ptr<FilterPanel>    f1, f2;
    std::unique_ptr<RoutingPanel>   routing;
    std::unique_ptr<EnvPanel>       envs;
    std::unique_ptr<LfoPanel>       lfos;
    std::unique_ptr<ModMatrixPanel> matrix;

    explicit FilterModTab (juce::AudioProcessorValueTreeState& apvts)
    {
        f1 = std::make_unique<FilterPanel> (apvts, juce::String::fromUTF8 ("FILTER 1  \xc2\xb7  LADDER"), juce::StringArray { "LP 24", "LP 12", "HP 24" },
                                            pid::fltType, pid::fltCutoff, pid::fltRes, pid::fltDrive, pid::fltKeyTrk, pid::fltEnvAmt, pid::flt1EnvSrc, juce::String());
        f2 = std::make_unique<FilterPanel> (apvts, juce::String::fromUTF8 ("FILTER 2  \xc2\xb7  SVF"), juce::StringArray { "LP", "HP", "BP", "Notch" },
                                            pid::flt2Type, pid::flt2Cutoff, pid::flt2Res, pid::flt2Drive, pid::flt2KeyTrk, pid::flt2EnvAmt, pid::flt2EnvSrc, pid::flt2Slope);
        routing = std::make_unique<RoutingPanel> (apvts);
        envs    = std::make_unique<EnvPanel> (apvts);
        lfos    = std::make_unique<LfoPanel> (apvts);
        matrix  = std::make_unique<ModMatrixPanel> (apvts);
        addAndMakeVisible (*f1); addAndMakeVisible (*f2); addAndMakeVisible (*routing);
        addAndMakeVisible (*envs); addAndMakeVisible (*lfos); addAndMakeVisible (*matrix);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int gap = 10;
        auto top = r.removeFromTop (248);
        f1->setBounds (top.removeFromLeft (300)); top.removeFromLeft (gap);
        f2->setBounds (top.removeFromLeft (300)); top.removeFromLeft (gap);
        routing->setBounds (top);
        r.removeFromTop (gap);
        auto bot = r;
        envs->setBounds (bot.removeFromLeft (330)); bot.removeFromLeft (gap);
        lfos->setBounds (bot.removeFromLeft (300)); bot.removeFromLeft (gap);
        matrix->setBounds (bot);
    }
};
