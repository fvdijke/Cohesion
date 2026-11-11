#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Dx7.h"

// Shared by the synth's FM tab and the COHESION - SYSEX bank builder.
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

