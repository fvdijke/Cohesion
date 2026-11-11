#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"
#include "La32.h"

// Relies on LabeledKnob + the attachment typedefs from PluginEditor.h (included first).

namespace laui
{
    static const juce::Colour teal { 0xff39ffb0 }, tealDark { 0xff0f6e56 }, txt { 0xff9fb0aa }, dim { 0xff6c7a74 }, light { 0xffeafff6 }, blue { 0xff5aa6d8 };
    inline void card (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    }
}

// ---------------------------------------------------------------------------
//  Structure diagram: two partial pairs, synth (S) / PCM (P), mix or ring.
// ---------------------------------------------------------------------------
struct PairDiagram : juce::Component
{
    la32::Timbre t;
    void paint (juce::Graphics& g) override
    {
        auto a = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a0c0f)); g.fillRoundedRectangle (a, 5.0f);
        static const char* mixName[] = { "MIX", "1 + RING", "RING", "2 + RING" };
        const float colW = a.getWidth() / 2.0f;
        for (int pair = 0; pair < 2; ++pair)
        {
            bool p1, p2; int mix; la32::structure (t.common[10 + pair], p1, p2, mix);
            const float cx = a.getX() + colW * (pair + 0.5f);
            const float y = a.getY() + 30;
            for (int k = 0; k < 2; ++k)
            {
                const int idx = pair * 2 + k;
                const bool on = t.partialOn (idx), pcm = k == 0 ? p1 : p2;
                juce::Rectangle<float> b (cx - 46 + k * 52, y, 40, 30);
                g.setColour (on ? (pcm ? laui::blue.withAlpha (0.35f) : laui::tealDark) : juce::Colour (0xff1a1e23)); g.fillRoundedRectangle (b, 4.0f);
                g.setColour (on ? (pcm ? laui::blue : laui::teal) : juce::Colour (0xff3a4149)); g.drawRoundedRectangle (b, 4.0f, 1.0f);
                g.setColour (on ? laui::light : laui::dim); g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
                g.drawText ((pcm ? "P" : "S") + juce::String (idx + 1), b, juce::Justification::centred);
                g.setColour (laui::txt.withAlpha (0.7f)); g.drawLine (b.getCentreX(), b.getBottom(), cx, y + 62, 1.3f);
            }
            juce::Rectangle<float> m (cx - 34, y + 62, 68, 24);
            g.setColour (mix == 0 ? juce::Colour (0xff1a1e23) : laui::tealDark.withAlpha (0.6f)); g.fillRoundedRectangle (m, 4.0f);
            g.setColour (laui::light); g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::bold));
            g.drawText (mixName[mix], m, juce::Justification::centred);
            g.setColour (laui::teal.withAlpha (0.6f)); g.drawLine (cx, m.getBottom(), cx, a.getBottom() - 16, 1.4f);
            g.setColour (laui::dim); g.setFont (juce::Font ("Segoe UI", 9.5f, juce::Font::plain));
            g.drawText ("STRUCTURE " + juce::String (t.common[10 + pair] + 1), (int) (cx - 60), (int) a.getY() + 8, 120, 14, juce::Justification::centred);
        }
        g.setColour (laui::teal.withAlpha (0.6f));
        g.drawLine (a.getX() + colW * 0.5f, a.getBottom() - 16, a.getX() + colW * 1.5f, a.getBottom() - 16, 2.0f);
        g.setColour (laui::dim); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText ("OUT", a.withTop (a.getBottom() - 14), juce::Justification::centred);
    }
};

// ---------------------------------------------------------------------------
//  One partial card.
// ---------------------------------------------------------------------------
struct PartialCard : juce::Component
{
    int index = 0; bool on = false, pcm = false; const uint8_t* p = nullptr; uint8_t data[58] {};

    void set (const la32::Timbre& t, int idx)
    {
        index = idx; on = t.partialOn (idx);
        bool p1, p2; int mix; la32::structure (t.common[10 + idx / 2], p1, p2, mix);
        pcm = (idx % 2 == 0) ? p1 : p2;
        std::memcpy (data, t.partial[idx], 58); p = data;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        laui::card (g, r);
        const float a = on ? 1.0f : 0.35f;
        const auto accent = pcm ? laui::blue : laui::teal;
        g.setColour (accent.withAlpha (a)); g.fillEllipse (12.0f, 15.0f, 9.0f, 9.0f);
        g.setColour (juce::Colour (0xffe8eef0).withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 13.0f, juce::Font::bold));
        g.drawText ("PARTIAL " + juce::String (index + 1), 27, 10, 140, 20, juce::Justification::centredLeft);
        g.setColour (accent.withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 9.5f, juce::Font::bold));
        g.drawText (! on ? juce::String ("OFF") : pcm ? juce::String ("PCM SAMPLE") : juce::String ("SYNTH"), 12, 30, 150, 14, juce::Justification::centredLeft);
        if (p == nullptr) return;

        g.setColour (laui::light.withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 19.0f, juce::Font::plain));
        const juce::String wave = pcm ? "PCM #" + juce::String ((int) p[5] + 128 * ((p[4] >> 1) & 1) + 1)
                                      : juce::String ((p[4] & 1) ? "Sawtooth" : "Square") + ((p[4] & 1) ? juce::String() : "  " + juce::String ((int) p[6]) + "%");
        g.drawText (wave, 10, 48, (int) r.getWidth() - 20, 26, juce::Justification::centredLeft);

        g.setFont (juce::Font ("Segoe UI", 10.5f, juce::Font::plain));
        auto line = [&] (int y, const juce::String& k, const juce::String& v)
        {
            g.setColour (laui::dim.withAlpha (a)); g.drawText (k, 10, y, 90, 14, juce::Justification::centredLeft);
            g.setColour (laui::light.withAlpha (a)); g.drawText (v, 10, y, (int) r.getWidth() - 20, 14, juce::Justification::centredRight);
        };
        const int semis = (int) p[0] - 36;
        line (80, "PITCH", (semis >= 0 ? "+" : "") + juce::String (semis) + " st  " + (p[1] >= 50 ? "+" : "") + juce::String ((int) p[1] - 50) + " ct");
        line (96, "KEY FOLLOW", juce::String (la32::keyfollow (p[2]), 2));
        if (pcm) line (112, "FILTER", "none (PCM)");
        else     line (112, "FILTER", juce::String ((int) p[0x17]) + "  res " + juce::String ((int) p[0x18]));
        line (128, "LEVEL", juce::String ((int) p[0x29]));
        line (144, "VELOCITY", juce::String ((int) p[0x2a] - 50));

        // TVA envelope sketch (T1..T5, L1..L3, sustain)
        juce::Rectangle<float> e (10, 166, r.getWidth() - 20, 78);
        g.setColour (juce::Colour (0xff0a0c0f)); g.fillRoundedRectangle (e, 4.0f);
        const float lv[6] = { 0, (float) p[0x36], (float) p[0x37], (float) p[0x38], (float) p[0x39], 0 };
        float t[5]; float tot = 0;
        for (int k = 0; k < 5; ++k) { t[k] = 0.15f + std::sqrt (la32::envMs (p[0x31 + k]) / 1000.0f); tot += t[k]; }
        tot += 0.6f;
        juce::Path pth; float x = e.getX() + 4; const float w = e.getWidth() - 8, yb = e.getBottom() - 6, h = e.getHeight() - 16;
        pth.startNewSubPath (x, yb);
        for (int k = 0; k < 4; ++k) pth.lineTo (x += t[k] / tot * w, yb - lv[k + 1] / 100.0f * h);
        pth.lineTo (x += 0.6f / tot * w, yb - lv[4] / 100.0f * h);
        pth.lineTo (x += t[4] / tot * w, yb);
        g.setColour (accent.withAlpha (a)); g.strokePath (pth, juce::PathStrokeType (1.6f));
        g.setColour (laui::dim); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText ("AMP ENVELOPE (TVA)", e.reduced (4, 2), juce::Justification::topLeft);
    }
};

// ---------------------------------------------------------------------------
//  The LA tab.
// ---------------------------------------------------------------------------
struct LaTab : juce::Component, private juce::Timer
{
    CohesionSynthAudioProcessor& proc;
    std::function<void()> onOpenLibrary;
    juce::ToggleButton onBtn;
    std::unique_ptr<ButtonAtt> onAtt;
    juce::TextButton libBtn { "LOAD / IMPORT ROMS..." };
    juce::OwnedArray<LabeledKnob> knobs;
    PairDiagram diagram;
    PartialCard parts[4];
    la32::Timbre timbre; std::shared_ptr<const la32::Rom> rom;
    int shownVersion = -1;
    juce::Rectangle<int> left, macros;

    explicit LaTab (CohesionSynthAudioProcessor& p) : proc (p)
    {
        onBtn.setButtonText ("ON"); addAndMakeVisible (onBtn);
        onAtt = std::make_unique<ButtonAtt> (proc.apvts, pid::laOn, onBtn);
        libBtn.setColour (juce::TextButton::buttonColourId, laui::tealDark);
        libBtn.setColour (juce::TextButton::textColourOffId, laui::light);
        libBtn.onClick = [this] { if (onOpenLibrary) onOpenLibrary(); };
        addAndMakeVisible (libBtn);
        addAndMakeVisible (diagram);
        for (auto& c : parts) addAndMakeVisible (c);
        auto add = [&] (const char* id, const char* nm) { auto* k = new LabeledKnob (proc.apvts, id, nm); knobs.add (k); addAndMakeVisible (k); };
        add (pid::laLevel, "LEVEL"); add (pid::laCutoff, "CUTOFF"); add (pid::laTime, "TIME");
        refresh();
        startTimerHz (20);
    }

    void refresh()
    {
        if (proc.getLaVersion() != shownVersion)
        {
            shownVersion = proc.getLaVersion();
            auto cur = proc.getLaTimbre(); rom = cur.first; timbre = cur.second;
            diagram.t = timbre; diagram.repaint();
            for (int i = 0; i < 4; ++i) parts[i].set (timbre, i);
            repaint();
        }
        const bool on = proc.apvts.getRawParameterValue (pid::laOn)->load() > 0.5f;
        const float a = on ? 1.0f : 0.4f;
        for (juce::Component* c : { (juce::Component*) &diagram, (juce::Component*) &parts[0], (juce::Component*) &parts[1],
                                     (juce::Component*) &parts[2], (juce::Component*) &parts[3] })
            if (std::abs (c->getAlpha() - a) > 0.01f) c->setAlpha (a);
        for (auto* k : knobs) if (std::abs (k->getAlpha() - a) > 0.01f) k->setAlpha (a);
    }

    void timerCallback() override { if (isShowing()) refresh(); }

    void paint (juce::Graphics& g) override
    {
        laui::card (g, left.toFloat());
        laui::card (g, macros.toFloat());
        const int lx = left.getX() + 16;
        g.setColour (juce::Colour (0xff5dcaa5)); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("LA ENGINE  \xc2\xb7  MT-32 / CM-32L"), lx, left.getY() + 12, 240, 18, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white); g.setFont (juce::Font ("Segoe UI", 22.0f, juce::Font::bold));
        const bool none = proc.numRomSets() == 0;
        g.drawText (none ? juce::String ("No ROMs loaded") : timbre.getName().isEmpty() ? juce::String ("(no timbre)") : timbre.getName(),
                    lx, left.getY() + 52, left.getWidth() - 32, 30, juce::Justification::centredLeft);
        g.setColour (laui::txt); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        g.drawFittedText (none ? juce::String ("Import your own MT-32 / CM-32L control + PCM ROM dumps (folders and .zip files are scanned).")
                               : proc.getLaBank(), lx, left.getY() + 84, left.getWidth() - 32, 30, juce::Justification::topLeft, 2);

        g.setColour (juce::Colour (0xff5dcaa5)); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        g.drawText ("MACROS", macros.getX() + 16, macros.getY() + 10, 120, 16, juce::Justification::centredLeft);
        const int sx = macros.getRight() - 290;
        g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        auto line = [&] (int y, const juce::String& k, const juce::String& v)
        {
            g.setColour (laui::dim); g.drawText (k, sx, y, 110, 16, juce::Justification::centredLeft);
            g.setColour (laui::light); g.drawText (v, sx + 110, y, 170, 16, juce::Justification::centredLeft);
        };
        const int y0 = macros.getY() + 30;
        int active = 0; for (int k = 0; k < 4; ++k) if (timbre.partialOn (k)) ++active;
        line (y0,      "ROM SET", rom != nullptr ? rom->model + " " + rom->version : juce::String ("-"));
        line (y0 + 20, "PARTIALS", juce::String (active) + " of 4");
        line (y0 + 40, "ENV MODE", timbre.common[13] ? "no sustain" : "normal");
        line (y0 + 60, "STRUCTURES", juce::String (timbre.common[10] + 1) + " / " + juce::String (timbre.common[11] + 1));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        left = r.removeFromLeft (300);
        r.removeFromLeft (10);
        auto right = r;
        auto row = right.removeFromTop (300);
        right.removeFromTop (10);
        macros = right;

        onBtn.setBounds (left.getRight() - 16 - 46, left.getY() + 12, 46, 22);
        diagram.setBounds (left.getX() + 14, left.getY() + 130, left.getWidth() - 28, 190);
        libBtn.setBounds (left.getX() + 14, left.getBottom() - 48, left.getWidth() - 28, 34);

        const int gap = 8, cw = (row.getWidth() - 3 * gap) / 4;
        for (int i = 0; i < 4; ++i) parts[i].setBounds (row.getX() + i * (cw + gap), row.getY(), cw, row.getHeight());

        auto kr = macros.reduced (14).withTrimmedTop (18);
        kr.setWidth (kr.getWidth() - 300);
        const int kw = kr.getWidth() / juce::jmax (1, knobs.size());
        for (int i = 0; i < knobs.size(); ++i) knobs[i]->setBounds (kr.getX() + i * kw, kr.getY(), kw, kr.getHeight());
    }
};
