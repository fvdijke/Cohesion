#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"

// Relies on LabeledKnob + the attachment typedefs from PluginEditor.h (included first).

namespace fxui
{
    static const char* names[fx::kNumFx] = { "COMP", "FLANGER", "PHASER", "ECHO", "REVERB" };
    inline const char* onId (int fxId)
    {
        switch (fxId) { case fx::kComp: return pid::compOn; case fx::kFlanger: return pid::flOn; case fx::kPhaser: return pid::phOn;
                        case fx::kEcho: return pid::ecOn; default: return pid::rvOn; }
    }
    inline bool isOn (juce::AudioProcessorValueTreeState& apvts, int fxId)
    {
        return apvts.getRawParameterValue (onId (fxId))->load() > 0.5f;
    }
    static const juce::Colour teal { 0xff39ffb0 }, tealDark { 0xff0f6e56 }, chipBg { 0xff0f2a22 }, txt { 0xff9fb0aa };
}

// ---------------------------------------------------------------------------
//  One effect panel: LED + title + ON, live display, 6 knobs + MIX.
// ---------------------------------------------------------------------------
struct FxPanel : juce::Component
{
    CohesionSynthAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    int fxId;
    juce::ToggleButton onBtn;
    std::unique_ptr<ButtonAtt> onAtt;
    juce::OwnedArray<LabeledKnob> knobs;
    double anim = 0.0;

    FxPanel (CohesionSynthAudioProcessor& p, int id) : proc (p), apvts (p.apvts), fxId (id)
    {
        onBtn.setButtonText ("ON");
        addAndMakeVisible (onBtn);
        onAtt = std::make_unique<ButtonAtt> (apvts, fxui::onId (id), onBtn);

        struct K { const char* id; const char* name; };
        std::vector<K> ks;
        switch (id)
        {
            case fx::kComp:    ks = { { pid::compThresh, "THRESH" }, { pid::compRatio, "RATIO" }, { pid::compAtk, "ATTACK" },
                                      { pid::compRel, "RELEASE" }, { pid::compMakeup, "MAKEUP" }, { pid::compKnee, "KNEE" }, { pid::compMix, "MIX" } }; break;
            case fx::kFlanger: ks = { { pid::flRate, "RATE" }, { pid::flDepth, "DEPTH" }, { pid::flFb, "FEEDBK" },
                                      { pid::flDelay, "DELAY" }, { pid::flWidth, "WIDTH" }, { pid::flSync, "SYNC" }, { pid::flMix, "MIX" } }; break;
            case fx::kPhaser:  ks = { { pid::phRate, "RATE" }, { pid::phDepth, "DEPTH" }, { pid::phFb, "FEEDBK" },
                                      { pid::phStages, "STAGES" }, { pid::phSpread, "SPREAD" }, { pid::phSync, "SYNC" }, { pid::phMix, "MIX" } }; break;
            case fx::kEcho:    ks = { { pid::ecTime, "TIME" }, { pid::ecSync, "SYNC" }, { pid::ecFb, "FEEDBK" },
                                      { pid::ecDamp, "DAMP" }, { pid::ecWidth, "WIDTH" }, { pid::ecPing, "PINGPONG" }, { pid::ecMix, "MIX" } }; break;
            default:           ks = { { pid::rvSize, "SIZE" }, { pid::rvDecay, "DECAY" }, { pid::rvPre, "PREDLY" },
                                      { pid::rvDamp, "DAMP" }, { pid::rvWidth, "WIDTH" }, { pid::rvLowCut, "LOW CUT" }, { pid::rvMix, "MIX" } }; break;
        }
        for (auto& k : ks) { auto* kn = new LabeledKnob (apvts, k.id, k.name); knobs.add (kn); addAndMakeVisible (kn); }
    }

    float v (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::Rectangle<float> displayArea() const { return { 14.0f, 44.0f, (float) getWidth() - 28.0f, 64.0f }; }

    // advance the display animation (called by the tab timer, ~30 Hz)
    void tick (double bpm)
    {
        double hz = 0.0;
        if (fxId == fx::kFlanger) hz = fx::modSyncHz ((int) v (pid::flSync), v (pid::flRate), bpm);
        if (fxId == fx::kPhaser)  hz = fx::modSyncHz ((int) v (pid::phSync), v (pid::phRate), bpm);
        anim += hz / 30.0;
        if (anim > 1000.0) anim -= 1000.0;
        repaint (displayArea().toNearestInt().expanded (2));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const bool on = fxui::isOn (apvts, fxId);
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (on ? fxui::teal : juce::Colour (0xff2b3138)); g.fillEllipse (13.0f, 19.0f, 10.0f, 10.0f);
        g.setColour (on ? juce::Colour (0xffe8eef0) : fxui::txt);
        g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
        g.drawText (fxui::names[fxId], 30, 14, 110, 20, juce::Justification::centredLeft);

        auto d = displayArea();
        g.setColour (juce::Colour (0xff0a0c0f)); g.fillRoundedRectangle (d, 5.0f);
        g.setColour (juce::Colour (0xff1c2127)); g.drawRoundedRectangle (d, 5.0f, 1.0f);
        g.saveState();
        g.reduceClipRegion (d.toNearestInt());
        const float a = on ? 1.0f : 0.35f;
        switch (fxId)
        {
            case fx::kComp:    drawComp (g, d, a);    break;
            case fx::kFlanger: drawFlanger (g, d, a); break;
            case fx::kPhaser:  drawPhaser (g, d, a);  break;
            case fx::kEcho:    drawEcho (g, d, a);    break;
            default:           drawReverb (g, d, a);  break;
        }
        g.restoreState();
    }

    void drawComp (juce::Graphics& g, juce::Rectangle<float> d, float a)
    {
        const float gr = juce::jlimit (0.0f, 24.0f, proc.getCompGainReductionDb());
        const float thr = (v (pid::compThresh) + 60.0f) / 60.0f;
        auto bar = [&] (float y, float frac, juce::Colour c, const char* lab)
        {
            g.setColour (juce::Colour (0xff6c7a74)); g.setFont (juce::Font ("Segoe UI", 8.0f, juce::Font::plain));
            g.drawText (lab, (int) d.getX() + 8, (int) y - 1, 24, 12, juce::Justification::centredLeft);
            juce::Rectangle<float> t (d.getX() + 34, y, d.getWidth() - 44, 10);
            g.setColour (juce::Colour (0xff13181d)); g.fillRoundedRectangle (t, 5.0f);
            g.setColour (c.withAlpha (a)); g.fillRoundedRectangle (t.withWidth (juce::jmax (10.0f, t.getWidth() * frac)), 5.0f);
        };
        bar (d.getY() + 14, gr / 24.0f, fxui::teal, "GR");
        bar (d.getY() + 36, thr, juce::Colour (0xff5dcaa5).withAlpha (0.6f), "THR");
        g.setColour (juce::Colour (0xff9fe1cb).withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText ("-" + juce::String (gr, 1) + " dB", d.reduced (8, 4), juce::Justification::topRight);
    }

    void drawFlanger (juce::Graphics& g, juce::Rectangle<float> d, float a)
    {
        const float depth = v (pid::flDepth), width = v (pid::flWidth);
        for (int ch = 1; ch >= 0; --ch)
        {
            juce::Path p;
            const double off = ch == 1 ? width * 0.25 : 0.0;
            for (int i = 0; i <= 60; ++i)
            {
                const float x = (float) i / 60.0f;
                const float y = d.getCentreY() - (0.12f + 0.3f * depth) * d.getHeight()
                              * (float) std::sin (juce::MathConstants<double>::twoPi * (x * 2.0 + anim + off));
                if (i == 0) p.startNewSubPath (d.getX() + 6 + x * (d.getWidth() - 12), y); else p.lineTo (d.getX() + 6 + x * (d.getWidth() - 12), y);
            }
            g.setColour ((ch == 0 ? fxui::teal : juce::Colour (0xff5aa6d8)).withAlpha (a * (ch == 0 ? 1.0f : 0.6f)));
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
        }
    }

    void drawPhaser (juce::Graphics& g, juce::Rectangle<float> d, float a)
    {
        static const int st[] = { 2, 4, 6, 8, 12 };
        const int notches = juce::jmax (1, st[juce::jlimit (0, 4, (int) v (pid::phStages))] / 2);
        const float depth = v (pid::phDepth);
        const float sweep = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * anim);
        juce::Path p;
        for (int i = 0; i <= 80; ++i)
        {
            const float x = (float) i / 80.0f;
            const float ph = juce::MathConstants<float>::pi * ((float) notches * x * (0.6f + 0.4f * depth) + sweep * depth);
            const float mag = std::pow (std::abs (std::cos (ph)), 0.5f);
            const float y = d.getBottom() - 8 - mag * (d.getHeight() - 18);
            if (i == 0) p.startNewSubPath (d.getX() + 6 + x * (d.getWidth() - 12), y); else p.lineTo (d.getX() + 6 + x * (d.getWidth() - 12), y);
        }
        g.setColour (fxui::teal.withAlpha (a)); g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
    }

    void drawEcho (juce::Graphics& g, juce::Rectangle<float> d, float a)
    {
        const float ms = fx::echoSyncMs ((int) v (pid::ecSync), v (pid::ecTime), proc.getFxBpm());
        const float fb = v (pid::ecFb);
        const bool ping = v (pid::ecPing) > 0.5f;
        const float spacing = juce::jlimit (10.0f, 60.0f, ms / 2000.0f * 140.0f + 10.0f);
        float amp = 1.0f, x = d.getX() + 14;
        for (int k = 0; k < 12 && x < d.getRight() - 6 && amp > 0.03f; ++k)
        {
            const float h = amp * (d.getHeight() - 16);
            const bool right = ping && (k & 1);
            g.setColour ((right ? juce::Colour (0xff5aa6d8) : fxui::teal).withAlpha (a * (0.35f + 0.65f * amp)));
            g.fillRoundedRectangle (x - 1.5f, d.getBottom() - 6 - h, 3.0f, h, 1.5f);
            amp *= juce::jmax (0.05f, fb);
            x += spacing;
        }
        g.setColour (juce::Colour (0xff9fe1cb).withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText (juce::String ((int) std::round (ms)) + " ms", d.reduced (8, 4), juce::Justification::topRight);
    }

    void drawReverb (juce::Graphics& g, juce::Rectangle<float> d, float a)
    {
        const float pre = v (pid::rvPre) / 250.0f, decay = v (pid::rvDecay), size = v (pid::rvSize);
        const float x0 = d.getX() + 8 + pre * 0.25f * d.getWidth();
        const float top = d.getY() + 8 + (1.0f - size) * 18.0f;
        const float k = 1.5f + (1.0f - decay) * 9.0f;
        juce::Path p; p.startNewSubPath (x0, d.getBottom() - 4);
        for (int i = 0; i <= 60; ++i)
        {
            const float t = (float) i / 60.0f;
            const float x = x0 + t * (d.getRight() - 8 - x0);
            const float env = (t < 0.04f ? t / 0.04f : std::exp (-k * (t - 0.04f)));
            p.lineTo (x, d.getBottom() - 4 - env * (d.getBottom() - 4 - top));
        }
        p.lineTo (d.getRight() - 8, d.getBottom() - 4); p.closeSubPath();
        g.setColour (fxui::teal.withAlpha (0.14f * a)); g.fillPath (p);
        g.setColour (fxui::teal.withAlpha (a)); g.strokePath (p, juce::PathStrokeType (1.4f));
        const float rt = 0.2f * std::pow (60.0f, decay);
        g.setColour (juce::Colour (0xff9fe1cb).withAlpha (a)); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText (juce::String (rt, 1) + " s", d.reduced (8, 4), juce::Justification::topRight);
    }

    void resized() override
    {
        onBtn.setBounds (getWidth() - 14 - 46, 13, 46, 22);
        auto r = getLocalBounds().reduced (10);
        r.removeFromTop (108);
        const int rowH = juce::jmin (96, r.getHeight() / 4);
        const int cw = r.getWidth() / 2;
        for (int i = 0; i < 6 && i < knobs.size(); ++i)
            knobs[i]->setBounds (r.getX() + (i % 2) * cw, r.getY() + (i / 2) * rowH, cw, rowH);
        if (knobs.size() > 6)
            knobs[6]->setBounds (r.getX() + cw / 2, r.getY() + 3 * rowH, cw, rowH);
    }
};

// ---------------------------------------------------------------------------
//  Signal-chain bar: drag chips to reorder, click a chip to toggle the effect.
//  Master limiter on the right.
// ---------------------------------------------------------------------------
struct FxChainBar : juce::Component
{
    CohesionSynthAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    std::function<void()> onOrderChanged;

    juce::ToggleButton limBtn;
    juce::Slider ceilSl;
    std::unique_ptr<ButtonAtt> limAtt;
    std::unique_ptr<SliderAtt> ceilAtt;

    int downSlot = -1, insertAt = -1; bool dragging = false; float dragX = 0, downX = 0;
    static constexpr int x0 = 124, chipW = 116, arrowW = 22;

    explicit FxChainBar (CohesionSynthAudioProcessor& p) : proc (p), apvts (p.apvts)
    {
        limBtn.setButtonText ("LIMITER"); addAndMakeVisible (limBtn);
        limAtt = std::make_unique<ButtonAtt> (apvts, pid::limOn, limBtn);
        ceilSl.setSliderStyle (juce::Slider::LinearHorizontal);
        ceilSl.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
        ceilSl.setColour (juce::Slider::trackColourId, fxui::teal.withAlpha (0.8f));
        ceilSl.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff13181d));
        ceilSl.setColour (juce::Slider::thumbColourId, juce::Colour (0xffdfe6e2));
        ceilSl.setTooltip ("Limiter ceiling (dBFS)");
        addAndMakeVisible (ceilSl);
        ceilAtt = std::make_unique<SliderAtt> (apvts, pid::limCeil, ceilSl);
    }

    juce::Rectangle<int> slotRect (int k) const { return { x0 + k * (chipW + arrowW), 8, chipW, getHeight() - 16 }; }

    int slotAt (juce::Point<int> p) const
    {
        for (int k = 0; k < fx::kNumFx; ++k) if (slotRect (k).contains (p)) return k;
        return -1;
    }

    void previewOrder (int* out) const
    {
        int cur[fx::kNumFx]; proc.getFxOrder (cur);
        if (! dragging || downSlot < 0) { std::copy (cur, cur + fx::kNumFx, out); return; }
        int tmp[fx::kNumFx - 1]; int t = 0;
        for (int k = 0; k < fx::kNumFx; ++k) if (k != downSlot) tmp[t++] = cur[k];
        int o = 0;
        for (int k = 0; k < fx::kNumFx; ++k)
            out[k] = (k == insertAt) ? cur[downSlot] : tmp[o++];
    }

    void drawChip (juce::Graphics& g, juce::Rectangle<float> r, int fxId, float alpha)
    {
        const bool on = fxui::isOn (apvts, fxId);
        g.setColour ((on ? fxui::chipBg : juce::Colour (0xff0e1013)).withAlpha (alpha)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour ((on ? fxui::tealDark : juce::Colour (0xff2b3138)).withAlpha (alpha)); g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour ((on ? fxui::teal : juce::Colour (0xff3a4149)).withAlpha (alpha));
        g.fillEllipse (r.getX() + 10, r.getCentreY() - 4, 8, 8);
        g.setColour ((on ? juce::Colour (0xffeafff6) : fxui::txt).withAlpha (alpha));
        g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::plain));
        g.drawText (fxui::names[fxId], r.withTrimmedLeft (22).withTrimmedRight (18), juce::Justification::centred);
        g.setColour (juce::Colour (0xff3a4149).withAlpha (alpha));
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 2; ++j)   // grip dots
            g.fillEllipse (r.getRight() - 14 + j * 4, r.getCentreY() - 5 + i * 4, 2, 2);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (b, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (juce::Colour (0xff5dcaa5));
        g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::bold));
        g.drawText ("SIGNAL CHAIN", 16, 0, 110, getHeight(), juce::Justification::centredLeft);

        int order[fx::kNumFx]; previewOrder (order);
        g.setColour (juce::Colour (0xff6c7a74)); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        for (int k = 0; k < fx::kNumFx; ++k)
        {
            auto r = slotRect (k).toFloat();
            if (k < fx::kNumFx - 1)
                g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92"), (int) r.getRight(), (int) r.getY(), arrowW, (int) r.getHeight(), juce::Justification::centred);
            if (dragging && k == insertAt)
            {
                g.setColour (fxui::teal.withAlpha (0.25f)); g.drawRoundedRectangle (r, 5.0f, 1.5f);
                g.setColour (juce::Colour (0xff6c7a74));
                continue;   // the dragged chip floats under the mouse
            }
            drawChip (g, r, order[k], 1.0f);
            g.setColour (juce::Colour (0xff6c7a74));
        }
        if (dragging && downSlot >= 0)
        {
            int cur[fx::kNumFx]; proc.getFxOrder (cur);
            auto r = slotRect (0).toFloat().withX (juce::jlimit ((float) x0, (float) (x0 + 4 * (chipW + arrowW)), dragX - chipW * 0.5f));
            drawChip (g, r.translated (0, -2), cur[downSlot], 0.85f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        downSlot = slotAt (e.getPosition()); dragging = false; downX = dragX = (float) e.x; insertAt = downSlot;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (downSlot < 0) return;
        dragX = (float) e.x;
        if (! dragging && std::abs (dragX - downX) > 5.0f) dragging = true;
        if (dragging)
        {
            insertAt = juce::jlimit (0, fx::kNumFx - 1, (int) std::round ((dragX - x0 - chipW * 0.5f) / (float) (chipW + arrowW)));
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (downSlot >= 0)
        {
            int cur[fx::kNumFx]; proc.getFxOrder (cur);
            if (dragging)
            {
                int out[fx::kNumFx]; previewOrder (out);
                proc.setFxOrder (out);
                if (onOrderChanged) onOrderChanged();
            }
            else if (auto* prm = apvts.getParameter (fxui::onId (cur[downSlot])))   // click = toggle
                prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        }
        downSlot = insertAt = -1; dragging = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void resized() override
    {
        const int lx = x0 + fx::kNumFx * (chipW + arrowW) - arrowW + 16;
        limBtn.setBounds (lx, 10, 84, getHeight() - 20);
        ceilSl.setBounds (lx + 92, 10, getWidth() - lx - 92 - 12, getHeight() - 20);
    }
};

// ---------------------------------------------------------------------------
//  The whole FX tab.
// ---------------------------------------------------------------------------
struct FxTab : juce::Component, private juce::Timer
{
    CohesionSynthAudioProcessor& proc;
    FxChainBar bar;
    std::unique_ptr<FxPanel> panels[fx::kNumFx];

    explicit FxTab (CohesionSynthAudioProcessor& p) : proc (p), bar (p)
    {
        addAndMakeVisible (bar);
        for (int i = 0; i < fx::kNumFx; ++i) { panels[i] = std::make_unique<FxPanel> (p, i); addAndMakeVisible (*panels[i]); }
        bar.onOrderChanged = [this] { resized(); };
        startTimerHz (30);
    }

    int lastOrder[fx::kNumFx] { 0, 1, 2, 3, 4 };

    void resized() override
    {
        auto r = getLocalBounds();
        bar.setBounds (r.removeFromTop (48));
        r.removeFromTop (10);
        int order[fx::kNumFx]; proc.getFxOrder (order);
        std::copy (order, order + fx::kNumFx, lastOrder);
        const int gap = 10;
        const int pw = (r.getWidth() - gap * (fx::kNumFx - 1)) / fx::kNumFx;
        for (int k = 0; k < fx::kNumFx; ++k)
            panels[order[k]]->setBounds (r.getX() + k * (pw + gap), r.getY(), pw, r.getHeight());
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        // keep the panel layout in sync if the order changed elsewhere (state recall)
        int order[fx::kNumFx]; proc.getFxOrder (order);
        if (! std::equal (order, order + fx::kNumFx, lastOrder)) resized();
        for (auto& pnl : panels) pnl->tick (proc.getFxBpm());
        bar.repaint();
        for (auto& pnl : panels) pnl->repaint (0, 0, pnl->getWidth(), 40);   // LED / title
    }
};
