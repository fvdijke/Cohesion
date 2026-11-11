#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"
#include "Dx7.h"

// Relies on LabeledKnob + the attachment typedefs from PluginEditor.h (included first).

namespace fmui
{
    static const juce::Colour teal { 0xff39ffb0 }, tealDark { 0xff0f6e56 }, txt { 0xff9fb0aa }, dim { 0xff6c7a74 }, light { 0xffeafff6 };

    inline void card (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    }
    inline juce::String ratioText (const dx7::Op& o)
    {
        if (o.mode == 1)
        {
            const double hz = std::pow (10.0, (o.coarse & 3) + o.fine / 100.0);
            return hz >= 1000.0 ? juce::String (hz / 1000.0, 2) + " kHz" : juce::String (hz, hz < 10.0 ? 2 : 1) + " Hz";
        }
        const double r = (o.coarse == 0 ? 0.5 : (double) o.coarse) * (1.0 + o.fine / 100.0);
        return juce::String::fromUTF8 ("\xc3\x97") + juce::String (r, 2);
    }
}

// ---------------------------------------------------------------------------
//  Algorithm diagram (laid out automatically from the connection table).
// ---------------------------------------------------------------------------
struct AlgoDiagram : juce::Component
{
    int alg = 0;

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a0c0f)); g.fillRoundedRectangle (area, 5.0f);
        const auto& A = dx7::algorithm (alg);

        int level[6] {}, parent[6];
        for (int j = 0; j < 6; ++j)
        {
            parent[j] = -1;
            for (int i = 0; i < j; ++i)
                if (A.mods[i] & (1 << j)) { level[j] = juce::jmax (level[j], level[i] + 1); if (parent[j] < 0) parent[j] = i; }
        }
        float width[6];
        for (int j = 5; j >= 0; --j)
        {
            float w = 0; for (int c = j + 1; c < 6; ++c) if (parent[c] == j) w += width[c];
            width[j] = juce::jmax (1.0f, w);
        }
        float total = 0; int maxLevel = 0;
        for (int j = 0; j < 6; ++j) { if (parent[j] < 0) total += width[j]; maxLevel = juce::jmax (maxLevel, level[j]); }

        const float boxW = 34, boxH = 24;
        auto inner = area.reduced (12, 14).withTrimmedBottom (22);
        const float colW = inner.getWidth() / juce::jmax (1.0f, total);
        const float rowH = juce::jmin (54.0f, inner.getHeight() / (float) (maxLevel + 1));
        // centre the graph vertically in the available space
        const float used = (float) maxLevel * rowH + boxH;
        inner = inner.withTrimmedBottom (juce::jmax (0.0f, (inner.getHeight() - used) * 0.5f));
        float cx[6], cy[6];
        std::function<void (int, float)> place = [&] (int op, float x0)
        {
            cx[op] = inner.getX() + (x0 + width[op] * 0.5f) * colW;
            cy[op] = inner.getBottom() - boxH * 0.5f - level[op] * rowH;
            float x = x0;
            for (int c = op + 1; c < 6; ++c) if (parent[c] == op) { place (c, x); x += width[c]; }
        };
        float x = 0;
        for (int j = 0; j < 6; ++j) if (parent[j] < 0) { place (j, x); x += width[j]; }

        // connections
        g.setColour (fmui::txt.withAlpha (0.7f));
        for (int i = 0; i < 6; ++i)
            for (int m = 0; m < 6; ++m)
                if (A.mods[i] & (1 << m))
                    g.drawLine (cx[m], cy[m] + boxH * 0.5f, cx[i], cy[i] - boxH * 0.5f, 1.4f);
        // carriers to the output bus
        const float busY = area.getBottom() - 14.0f;
        float minX = 1e9f, maxX = -1e9f;
        for (int i = 0; i < 6; ++i)
            if (A.carriers & (1 << i))
            {
                g.setColour (fmui::teal.withAlpha (0.6f)); g.drawLine (cx[i], cy[i] + boxH * 0.5f, cx[i], busY, 1.4f);
                minX = juce::jmin (minX, cx[i]); maxX = juce::jmax (maxX, cx[i]);
            }
        g.setColour (fmui::teal.withAlpha (0.6f)); g.drawLine (minX, busY, maxX, busY, 2.0f);
        // feedback
        const int fo = A.fbOp - 1, fs = A.fbSrc - 1;
        g.setColour (fmui::teal.withAlpha (0.85f));
        if (fo == fs)
        {
            juce::Path p; const float bx = cx[fo] + boxW * 0.5f, by = cy[fo];
            p.startNewSubPath (bx, by + 5); p.lineTo (bx + 9, by + 5); p.lineTo (bx + 9, by - boxH * 0.5f - 7);
            p.lineTo (cx[fo], by - boxH * 0.5f - 7); p.lineTo (cx[fo], by - boxH * 0.5f);
            g.strokePath (p, juce::PathStrokeType (1.3f));
        }
        else
        {
            juce::Path p; p.startNewSubPath (cx[fs] + boxW * 0.5f, cy[fs]);
            p.cubicTo (cx[fs] + boxW + 12, cy[fs], cx[fo] + boxW + 12, cy[fo], cx[fo] + boxW * 0.5f, cy[fo]);
            g.strokePath (p, juce::PathStrokeType (1.3f));
        }
        // boxes
        for (int i = 0; i < 6; ++i)
        {
            juce::Rectangle<float> b (cx[i] - boxW * 0.5f, cy[i] - boxH * 0.5f, boxW, boxH);
            const bool car = (A.carriers & (1 << i)) != 0;
            g.setColour (car ? fmui::tealDark : juce::Colour (0xff1a1e23)); g.fillRoundedRectangle (b, 4.0f);
            g.setColour (car ? fmui::teal : juce::Colour (0xff3a4149)); g.drawRoundedRectangle (b, 4.0f, 1.0f);
            g.setColour (car ? fmui::light : fmui::txt);
            g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
            g.drawText (juce::String (i + 1), b, juce::Justification::centred);
        }
        g.setColour (fmui::dim); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText ("OUT", area.withTop (busY + 1), juce::Justification::centred);
    }
};

// ---------------------------------------------------------------------------
//  One operator card.
// ---------------------------------------------------------------------------
struct OpCard : juce::Component
{
    int index = 0; dx7::Op op; bool carrier = false;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        fmui::card (g, r);
        const bool silent = op.ol == 0;
        const float a = silent ? 0.4f : 1.0f;
        g.setColour ((carrier ? fmui::teal : fmui::txt).withAlpha (a));
        g.fillEllipse (12.0f, 15.0f, 9.0f, 9.0f);
        g.setColour (juce::Colour (0xffe8eef0).withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 13.0f, juce::Font::bold));
        g.drawText ("OP " + juce::String (index + 1), 27, 10, 80, 20, juce::Justification::centredLeft);
        g.setColour ((carrier ? fmui::teal : fmui::dim).withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 9.5f, juce::Font::bold));
        g.drawText (carrier ? "CARRIER" : "MODULATOR", 12, 30, (int) r.getWidth() - 20, 14, juce::Justification::centredLeft);

        // frequency
        g.setColour (fmui::light.withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 20.0f, juce::Font::plain));
        g.drawText (fmui::ratioText (op), 10, 50, (int) r.getWidth() - 20, 28, juce::Justification::centredLeft);
        g.setColour (fmui::dim.withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 9.5f, juce::Font::plain));
        g.drawText (op.mode ? "FIXED" : "RATIO", 10, 78, 60, 14, juce::Justification::centredLeft);

        // output level bar
        g.drawText ("LEVEL", 10, 100, 60, 14, juce::Justification::centredLeft);
        g.setColour (fmui::light.withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        g.drawText (juce::String ((int) op.ol), 10, 100, (int) r.getWidth() - 20, 14, juce::Justification::centredRight);
        juce::Rectangle<float> bar (10, 117, r.getWidth() - 20, 8);
        g.setColour (juce::Colour (0xff13181d)); g.fillRoundedRectangle (bar, 4.0f);
        g.setColour ((carrier ? fmui::teal : fmui::txt).withAlpha (0.8f * a));
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (6.0f, bar.getWidth() * op.ol / 99.0f)), 4.0f);

        // envelope sketch
        juce::Rectangle<float> e (10, 140, r.getWidth() - 20, 74);
        g.setColour (juce::Colour (0xff0a0c0f)); g.fillRoundedRectangle (e, 4.0f);
        float seg[5]; float lv[5] = { (float) op.l[3], (float) op.l[0], (float) op.l[1], (float) op.l[2], (float) op.l[3] };
        auto segLen = [] (float from, float to, int rate) { return (0.2f + std::abs (to - from) / 99.0f) * std::sqrt ((100.0f - rate) / 100.0f + 0.02f); };
        seg[0] = segLen (lv[0], lv[1], op.r[0]); seg[1] = segLen (lv[1], lv[2], op.r[1]); seg[2] = segLen (lv[2], lv[3], op.r[2]);
        seg[3] = 0.5f; seg[4] = segLen (lv[3], lv[4], op.r[3]);
        float tot = 0; for (float s : seg) tot += s;
        juce::Path p; float x = e.getX() + 4; const float w = e.getWidth() - 8, yb = e.getBottom() - 6, h = e.getHeight() - 14;
        p.startNewSubPath (x, yb - lv[0] / 99.0f * h);
        float yPrev = lv[1];
        p.lineTo (x += seg[0] / tot * w, yb - lv[1] / 99.0f * h);
        p.lineTo (x += seg[1] / tot * w, yb - lv[2] / 99.0f * h);
        p.lineTo (x += seg[2] / tot * w, yb - lv[3] / 99.0f * h);
        p.lineTo (x += seg[3] / tot * w, yb - lv[3] / 99.0f * h);
        p.lineTo (x += seg[4] / tot * w, yb - lv[4] / 99.0f * h);
        juce::ignoreUnused (yPrev);
        g.setColour ((carrier ? fmui::teal : fmui::txt).withAlpha (a)); g.strokePath (p, juce::PathStrokeType (1.6f));
        g.setColour (fmui::dim); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText ("ENVELOPE", e.reduced (4, 2), juce::Justification::topLeft);

        // details
        g.setFont (juce::Font ("Segoe UI", 10.5f, juce::Font::plain));
        auto line = [&] (int y, const juce::String& k, const juce::String& v)
        {
            g.setColour (fmui::dim.withAlpha (a)); g.drawText (k, 10, y, 70, 14, juce::Justification::centredLeft);
            g.setColour (fmui::light.withAlpha (a)); g.drawText (v, 10, y, (int) r.getWidth() - 20, 14, juce::Justification::centredRight);
        };
        line (224, "DETUNE", juce::String ((int) op.det - 7));
        line (241, "VELOCITY", juce::String ((int) op.kvs));
        line (258, "RATE SCALE", juce::String ((int) op.rs));
        line (275, "AMP MOD", juce::String ((int) op.ams));
    }
};

// ---------------------------------------------------------------------------
//  The FM tab.
// ---------------------------------------------------------------------------
struct FmTab : juce::Component, private juce::Timer
{
    CohesionSynthAudioProcessor& proc;
    std::function<void()> onOpenLibrary;
    juce::ToggleButton onBtn;
    std::unique_ptr<ButtonAtt> onAtt;
    juce::TextButton libBtn { "LOAD / IMPORT SYSEX..." };
    juce::OwnedArray<LabeledKnob> knobs;
    AlgoDiagram diagram;
    OpCard ops[6];
    dx7::Patch patch;
    int shownVersion = -1;
    juce::Rectangle<int> left, macros;

    explicit FmTab (CohesionSynthAudioProcessor& p) : proc (p)
    {
        onBtn.setButtonText ("ON"); addAndMakeVisible (onBtn);
        onAtt = std::make_unique<ButtonAtt> (proc.apvts, pid::dxOn, onBtn);
        libBtn.setColour (juce::TextButton::buttonColourId, fmui::tealDark);
        libBtn.setColour (juce::TextButton::textColourOffId, fmui::light);
        libBtn.onClick = [this] { if (onOpenLibrary) onOpenLibrary(); };
        addAndMakeVisible (libBtn);
        addAndMakeVisible (diagram);
        for (int i = 0; i < 6; ++i) { ops[i].index = i; addAndMakeVisible (ops[i]); }
        auto add = [&] (const char* id, const char* nm) { auto* k = new LabeledKnob (proc.apvts, id, nm); knobs.add (k); addAndMakeVisible (k); };
        add (pid::dxLevel, "LEVEL"); add (pid::dxBright, "BRIGHT"); add (pid::dxTime, "TIME"); add (pid::dxVel, "VELOCITY");
        refresh();
        startTimerHz (20);
    }

    void refresh()
    {
        if (proc.getFmVersion() != shownVersion)
        {
            shownVersion = proc.getFmVersion();
            patch = proc.getFmPatch();
            diagram.alg = patch.alg;
            const auto& A = dx7::algorithm (patch.alg);
            for (int i = 0; i < 6; ++i) { ops[i].op = patch.op[(size_t) i]; ops[i].carrier = (A.carriers & (1 << i)) != 0; ops[i].repaint(); }
            diagram.repaint();
            repaint();
        }
        const bool on = proc.apvts.getRawParameterValue (pid::dxOn)->load() > 0.5f;
        const float a = on ? 1.0f : 0.4f;
        for (juce::Component* c : { (juce::Component*) &diagram, (juce::Component*) &ops[0], (juce::Component*) &ops[1], (juce::Component*) &ops[2],
                                     (juce::Component*) &ops[3], (juce::Component*) &ops[4], (juce::Component*) &ops[5] })
            if (std::abs (c->getAlpha() - a) > 0.01f) c->setAlpha (a);
        for (auto* k : knobs) if (std::abs (k->getAlpha() - a) > 0.01f) k->setAlpha (a);
    }

    void timerCallback() override { if (isShowing()) refresh(); }

    void paint (juce::Graphics& g) override
    {
        fmui::card (g, left.toFloat());
        fmui::card (g, macros.toFloat());
        const int lx = left.getX() + 16;
        g.setColour (juce::Colour (0xff5dcaa5)); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("FM ENGINE  \xc2\xb7  DX7"), lx, left.getY() + 12, 200, 18, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white); g.setFont (juce::Font ("Segoe UI", 22.0f, juce::Font::bold));
        g.drawText (patch.getName().isEmpty() ? juce::String ("(no voice)") : patch.getName(), lx, left.getY() + 52, left.getWidth() - 32, 30, juce::Justification::centredLeft);
        g.setColour (fmui::txt); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        const auto bank = proc.getFmBank();
        g.drawText (bank.isEmpty() ? juce::String ("Init") : bank, lx, left.getY() + 84, left.getWidth() - 32, 16, juce::Justification::centredLeft);
        g.setColour (fmui::light); g.setFont (juce::Font ("Segoe UI", 12.5f, juce::Font::plain));
        g.drawText ("ALGORITHM " + juce::String (patch.alg + 1) + "    FEEDBACK " + juce::String ((int) patch.fb),
                    lx, left.getY() + 112, left.getWidth() - 32, 18, juce::Justification::centredLeft);

        // macros card: titles + a summary of the voice-wide settings
        g.setColour (juce::Colour (0xff5dcaa5)); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        g.drawText ("MACROS", macros.getX() + 16, macros.getY() + 10, 120, 16, juce::Justification::centredLeft);
        static const char* waves[] = { "Triangle", "Saw down", "Saw up", "Square", "Sine", "S&H" };
        bool pegFlat = true; for (int k = 0; k < 4; ++k) if (patch.pl[k] != 50) pegFlat = false;
        const int sx = macros.getRight() - 290;
        g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        auto line = [&] (int y, const juce::String& k, const juce::String& v)
        {
            g.setColour (fmui::dim); g.drawText (k, sx, y, 100, 16, juce::Justification::centredLeft);
            g.setColour (fmui::light); g.drawText (v, sx + 100, y, 180, 16, juce::Justification::centredLeft);
        };
        const int y0 = macros.getY() + 30;
        line (y0,      "LFO", juce::String (waves[juce::jlimit (0, 5, (int) patch.lfoWave)]) + ", speed " + juce::String ((int) patch.lfoSpeed));
        line (y0 + 20, "LFO DEPTH", "pitch " + juce::String ((int) patch.lfoPmd) + " / amp " + juce::String ((int) patch.lfoAmd));
        line (y0 + 40, "LFO DELAY", juce::String ((int) patch.lfoDelay));
        line (y0 + 60, "PITCH EG", pegFlat ? juce::String ("flat") : juce::String ("active"));
        line (y0 + 80, "TRANSPOSE", juce::String ((int) patch.transpose - 24) + " st");
        line (y0 + 100, "KEY SYNC", patch.oks ? "on" : "off");
    }

    void resized() override
    {
        auto r = getLocalBounds();
        left = r.removeFromLeft (300);
        r.removeFromLeft (10);
        auto right = r;
        auto opsRow = right.removeFromTop (300);
        right.removeFromTop (10);
        macros = right;

        onBtn.setBounds (left.getRight() - 16 - 46, left.getY() + 12, 46, 22);
        diagram.setBounds (left.getX() + 14, left.getY() + 140, left.getWidth() - 28, left.getHeight() - 140 - 62);
        libBtn.setBounds (left.getX() + 14, left.getBottom() - 48, left.getWidth() - 28, 34);

        const int gap = 7, ow = (opsRow.getWidth() - 5 * gap) / 6;
        for (int i = 0; i < 6; ++i) ops[i].setBounds (opsRow.getX() + i * (ow + gap), opsRow.getY(), ow, opsRow.getHeight());

        auto kr = macros.reduced (14).withTrimmedTop (18);
        kr.setWidth (kr.getWidth() - 300);
        const int kw = kr.getWidth() / juce::jmax (1, knobs.size());
        for (int i = 0; i < knobs.size(); ++i) knobs[i]->setBounds (kr.getX() + i * kw, kr.getY(), kw, kr.getHeight());
    }
};
