#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>
#include "PluginProcessor.h"

#define COHESION_VERSION "v1.0"

// ===========================================================================
//  Look & feel: dark pro-audio styling with a teal accent.
// ===========================================================================
class CohesionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CohesionLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,    juce::Colour (0xffdfe6e2));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId,   juce::Colour (0xff0e1013));
        setColour (juce::ComboBox::textColourId,         juce::Colour (0xffdfe6e2));
        setColour (juce::ComboBox::outlineColourId,      juce::Colour (0xff39ffb0).withAlpha (0.40f));
        setColour (juce::ComboBox::arrowColourId,        juce::Colour (0xff9fe1cb));
        setColour (juce::PopupMenu::backgroundColourId,  juce::Colour (0xff15181c));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff0f6e56));
        setColour (juce::PopupMenu::textColourId,        juce::Colour (0xffdfe6e2));
        setColour (juce::TextButton::buttonColourId,     juce::Colour (0xff0e1013));
        setColour (juce::TextButton::textColourOffId,    juce::Colour (0xff9fe1cb));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float, float,
                           const juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float trackY = y + h * 0.5f;
        juce::Rectangle<float> track ((float) x, trackY - 2.0f, (float) w, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (juce::Colour (0xff39ffb0));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) x, trackY - 2.0f,
                                                        juce::jmax (0.0f, sliderPos - (float) x), 4.0f), 2.0f);
        g.setColour (juce::Colour (0xffeef4f1));
        g.fillEllipse (sliderPos - 7.0f, trackY - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawEllipse (sliderPos - 7.0f, trackY - 7.0f, 14.0f, 14.0f, 1.0f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float rotStart, float rotEnd, juce::Slider&) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float cx = r.getCentreX(), cy = r.getCentreY();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const float ang = rotStart + sliderPos * (rotEnd - rotStart);

        g.setColour (juce::Colour (0xff101317));
        g.fillEllipse (r);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (r, 1.0f);

        juce::Path track;
        track.addCentredArc (cx, cy, rad - 2.5f, rad - 2.5f, 0.0f, rotStart, rotEnd, true);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.strokePath (track, juce::PathStrokeType (3.0f));

        juce::Path arc;
        arc.addCentredArc (cx, cy, rad - 2.5f, rad - 2.5f, 0.0f, rotStart, ang, true);
        g.setColour (juce::Colour (0xff39ffb0));
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float pr = rad - 7.0f;
        const float px = cx + pr * std::sin (ang);
        const float py = cy - pr * std::cos (ang);
        g.setColour (juce::Colour (0xffdfe6e2));
        g.drawLine (cx, cy, px, py, 2.5f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        g.setColour (on ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? juce::Colour (0xff39ffb0) : juce::Colour (0xff39ffb0).withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.6f), 6.0f, 1.0f);

        auto led = r.removeFromLeft (r.getHeight());
        g.setColour (on ? juce::Colour (0xff39ffb0) : juce::Colours::white.withAlpha (0.20f));
        g.fillEllipse (led.withSizeKeepingCentre (8.0f, 8.0f));

        g.setColour (on ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::plain));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (2.0f), juce::Justification::centredLeft);
    }

    // slider value text: no box, small font (matches the control labels)
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::backgroundColourId,        juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId,           juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff0e1013));
        l->setColour (juce::Label::outlineWhenEditingColourId, juce::Colour (0xff39ffb0));
        l->setColour (juce::Label::textColourId,              juce::Colour (0xffdfe6e2));
        l->setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        return l;
    }
};

// ===========================================================================
//  Hero "MAGIC" engage button (power glyph + glow).
// ===========================================================================
class MagicButton : public juce::Button
{
public:
    MagicButton() : juce::Button ("MAGIC") { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const bool on = getToggleState();

        g.setColour (on ? juce::Colour (0xff0f6e56) : juce::Colour (0xff14181d));
        g.fillRoundedRectangle (r, 11.0f);
        if (on)
        {
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.10f));
            g.fillRoundedRectangle (r.reduced (3.0f), 9.0f);
        }
        g.setColour (on ? juce::Colour (0xff39ffb0) : juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (r.reduced (0.8f), 11.0f, on ? 1.8f : 1.0f);

        // power glyph on the left
        auto ic = r.removeFromLeft (r.getHeight());
        const float gcx = ic.getCentreX(), gcy = ic.getCentreY();
        const float rr = ic.getHeight() * 0.24f;
        g.setColour (on ? juce::Colour (0xff39ffb0) : juce::Colours::white.withAlpha (0.55f));
        juce::Path ring;
        ring.addCentredArc (gcx, gcy, rr, rr, 0.0f, 0.6f,
                            juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath (ring, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (gcx, gcy - rr - 2.5f, gcx, gcy + 1.0f, 2.2f);

        g.setColour (on ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        g.setFont (juce::Font ("Segoe UI", 15.0f, juce::Font::bold).withExtraKerningFactor (0.12f));
        g.drawText ("MAGIC", r, juce::Justification::centred);

        if (over)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 11.0f);
        }
    }
};

// ===========================================================================
//  Big numeric readout (INPUT / GAIN REDUCTION / OUTPUT).
// ===========================================================================
class Readout : public juce::Component, private juce::Timer
{
public:
    Readout (juce::String captionText, std::function<float()> fetchFn,
             juce::Colour colour, bool reduction = false)
        : caption (std::move (captionText)), fetch (std::move (fetchFn)),
          col (colour), isReduction (reduction)
    {
        value = reduction ? 0.0f : -100.0f;
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (juce::Colour (0xff9fb0aa));
        g.setFont (11.0f);
        g.drawText (caption, r.removeFromTop (16), juce::Justification::topLeft);

        juce::String t;
        if (isReduction)
            t = juce::String (-value, 1);
        else
            t = (value <= -99.0f) ? juce::String::fromUTF8 ("-\xe2\x88\x9e") : juce::String (value, 1);

        g.setColour (col);
        g.setFont (juce::Font ("Segoe UI", 32.0f, juce::Font::plain));
        g.drawText (t + "  dB", r, juce::Justification::topLeft);
    }

private:
    void timerCallback() override
    {
        const float v = fetch ? fetch() : 0.0f;
        if (! juce::approximatelyEqual (v, value)) { value = v; repaint(); }
    }

    juce::String caption;
    std::function<float()> fetch;
    juce::Colour col;
    bool  isReduction;
    float value = -100.0f;
};

// ===========================================================================
//  Vertical level meter (dBFS).
// ===========================================================================
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter (juce::String labelText, std::function<float()> fetchFn)
        : label (std::move (labelText)), fetch (std::move (fetchFn))
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (16.0f);

        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        const float minDb = -36.0f;
        const float norm = juce::jlimit (0.0f, 1.0f, (value - minDb) / (0.0f - minDb));
        auto fill = r.reduced (3.0f);
        fill = fill.withTop (fill.getBottom() - fill.getHeight() * norm);
        g.setColour (juce::Colour (0xff39ffb0));
        g.fillRoundedRectangle (fill, 2.0f);

        g.setColour (juce::Colour (0xff9fb0aa));
        g.setFont (10.0f);
        g.drawText (label, lab, juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        float v = fetch ? fetch() : -100.0f;
        v = juce::jlimit (-60.0f, 0.0f, v);
        if (std::abs (v - value) > 0.1f) { value = v; repaint(); }
    }

    juce::String label;
    std::function<float()> fetch;
    float value = -60.0f;
};

// ===========================================================================
//  Live display: scrolling level history BEHIND a single green transfer curve.
// ===========================================================================
class TransferCurve : public juce::Component, private juce::Timer
{
public:
    explicit TransferCurve (CohesionAudioProcessor& p) : proc (p)
    {
        for (int i = 0; i < kHist; ++i) { histIn[i] = -100.0f; histOut[i] = -100.0f; }
        startTimerHz (30);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (avgBadge.contains (e.position))
            if (auto* pr = proc.apvts.getParameter ("avgView"))
                pr->setValueNotifyingHost (pr->getValue() > 0.5f ? 0.0f : 1.0f);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (4.0f);
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (area, 4.0f, 1.0f);

        auto plot = area.reduced (10.0f, 10.0f);
        plot.removeFromRight (26.0f);                 // room for the dB scale

        const float minDb = -36.0f, maxDb = 0.0f, rng = maxDb - minDb;
        auto mapYv  = [&] (float db) { return plot.getBottom() - (db - minDb) / rng * plot.getHeight(); };
        auto mapXin = [&] (float db) { return plot.getX() + (db - minDb) / rng * plot.getWidth(); };
        auto mapXt  = [&] (int k)    { return plot.getX() + (float) k / (float) (kHist - 1) * plot.getWidth(); };

        const float thr     = juce::jlimit (minDb, maxDb, proc.apvts.getRawParameterValue ("threshold")->load());
        const int   ratioIx = (int) proc.apvts.getRawParameterValue ("ratio")->load();
        const float ratio   = (ratioIx == 0 ? 2.0f : ratioIx == 1 ? 4.0f : 10.0f);
        const int   kneeIx   = (int) proc.apvts.getRawParameterValue ("knee")->load();
        const float kneeDb   = (kneeIx == 1 ? 0.0f : 6.0f);
        const float ceiling  = juce::jlimit (minDb, maxDb, proc.apvts.getRawParameterValue ("ceiling")->load());
        const bool  avg      = proc.apvts.getRawParameterValue ("avgView")->load() > 0.5f;
        const float gr       = proc.getCompGainReductionDb();
        const bool  compActive  = gr > 0.1f;
        const bool  limitActive = proc.getLimitGainReductionDb() > 0.1f;

        // --- dB grid + scale labels ---
        g.setFont (9.5f);
        for (int db = 0; db >= -36; db -= 6)
        {
            const float yy = mapYv ((float) db);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawHorizontalLine ((int) yy, plot.getX(), plot.getRight());
            g.setColour (juce::Colour (0xff6c7a74));
            g.drawText (juce::String (db), plot.getRight() + 2.0f, yy - 7.0f, 24.0f, 14.0f,
                        juce::Justification::centredLeft);
        }

        // --- scrolling history (ordered oldest..newest, newest at right) ---
        float inOrd[kHist], outOrd[kHist];
        for (int k = 0; k < kHist; ++k)
        {
            const int idx = (histPos + k) % kHist;
            inOrd[k]  = juce::jlimit (minDb, maxDb, histIn[idx]);
            outOrd[k] = juce::jlimit (minDb, maxDb, histOut[idx]);
        }
        if (avg)
        {
            float si[kHist], so[kHist];
            boxcar (inOrd, si);  boxcar (outOrd, so);
            for (int k = 0; k < kHist; ++k) { inOrd[k] = si[k]; outOrd[k] = so[k]; }
        }

        juce::Path pin, pout;
        for (int k = 0; k < kHist; ++k)
        {
            const float x = mapXt (k);
            if (k == 0) { pin.startNewSubPath (x, mapYv (inOrd[0])); pout.startNewSubPath (x, mapYv (outOrd[0])); }
            else        { pin.lineTo (x, mapYv (inOrd[k]));          pout.lineTo (x, mapYv (outOrd[k])); }
        }
        g.setColour (juce::Colours::white.withAlpha (0.28f));          // output (grey)
        g.strokePath (pout, juce::PathStrokeType (1.3f));
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.90f));     // input (green)
        g.strokePath (pin,  juce::PathStrokeType (1.6f));

        // --- 1:1 reference ---
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        drawDashed (g, mapXin (minDb), mapYv (minDb), mapXin (maxDb), mapYv (maxDb));

        // --- single green transfer curve (+ soft fill underneath) ---
        juce::Path curvePath;
        bool started = false;
        for (float db = minDb; db <= maxDb + 0.001f; db += 1.0f)
        {
            const float out = juce::jlimit (minDb, maxDb, db - grAt (db, thr, ratio, kneeDb));
            const float x = mapXin (db), y = mapYv (out);
            if (! started) { curvePath.startNewSubPath (x, y); started = true; }
            else           curvePath.lineTo (x, y);
        }
        juce::Path fillPath (curvePath);
        fillPath.lineTo (mapXin (maxDb), plot.getBottom());
        fillPath.lineTo (mapXin (minDb), plot.getBottom());
        fillPath.closeSubPath();
        juce::ColourGradient grad (juce::Colour (0x881d9e75), 0.0f, plot.getY(),
                                   juce::Colour (0x081d9e75), 0.0f, plot.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (fillPath);
        g.setColour (juce::Colour (0xff39ffb0));
        g.strokePath (curvePath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // --- ceiling: thin continuous teal dashed line + value ---
        const float cy = mapYv (ceiling);
        g.setColour (juce::Colour (0xff9fe1cb).withAlpha (0.35f));
        drawDashed (g, plot.getX(), cy, plot.getRight(), cy);
        g.setColour (juce::Colour (0xff9fe1cb).withAlpha (0.70f));
        g.setFont (9.5f);
        g.drawText ("Ceiling " + juce::String (ceiling, 1), plot.getX() + 4.0f, cy - 13.0f, 90.0f, 12.0f,
                    juce::Justification::centredLeft);

        // --- threshold marker + KNEE label ---
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        drawDashed (g, mapXin (thr), plot.getY(), mapXin (thr), plot.getBottom());
        g.setColour (juce::Colour (0xff9fe1cb).withAlpha (0.7f));
        g.setFont (10.0f);
        g.drawText ("KNEE", mapXin (thr) + 4.0f, mapYv (thr) - 20.0f, 44.0f, 14.0f,
                    juce::Justification::centredLeft);

        // --- AVG toggle badge (left edge, halfway down; style of COMP/LIMIT) ---
        avgBadge = juce::Rectangle<float> (area.getX() + 6.0f, area.getCentreY() - 8.0f, 48.0f, 16.0f);
        drawBadge (g, avgBadge.getX(), avgBadge.getY(), "AVG", avg, juce::Colour (0xff39b58a));

        // --- status badges + GR readout ---
        drawBadge (g, area.getX() + 6.0f,  area.getBottom() - 20.0f, "COMP",  compActive,  juce::Colour (0xff39b58a));
        drawBadge (g, area.getX() + 60.0f, area.getBottom() - 20.0f, "LIMIT", limitActive, juce::Colour (0xffe24b4a));

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (11.0f);
        g.drawText ("GR " + juce::String (gr, 1) + " dB", area.reduced (30, 2), juce::Justification::topRight);
    }

private:
    void timerCallback() override
    {
        histIn[histPos]  = proc.getInputLevelDb();
        histOut[histPos] = proc.getOutputLevelDb();
        histPos = (histPos + 1) % kHist;
        repaint();
    }

    static void boxcar (const float* src, float* dst)
    {
        const int W = 14;
        for (int i = 0; i < kHist; ++i)
        {
            float s = 0.0f; int c = 0;
            for (int j = i - W; j <= i + W; ++j)
                if (j >= 0 && j < kHist) { s += src[j]; ++c; }
            dst[i] = s / (float) c;
        }
    }

    static float grAt (float inDb, float thr, float ratio, float kneeDb)
    {
        const float slope = 1.0f - 1.0f / ratio;
        const float over  = inDb - thr;
        if (kneeDb <= 0.0f)
            return over <= 0.0f ? 0.0f : slope * over;
        const float half = kneeDb * 0.5f;
        if (over <= -half) return 0.0f;
        if (over >=  half) return slope * over;
        const float x = over + half;
        return slope * (x * x) / (2.0f * kneeDb);
    }

    void drawDashed (juce::Graphics& g, float x1, float y1, float x2, float y2)
    {
        const float dashes[] = { 4.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (x1, y1, x2, y2), dashes, 2, 1.0f);
    }

    void drawBadge (juce::Graphics& g, float x, float y, const juce::String& t, bool on, juce::Colour c)
    {
        auto r = juce::Rectangle<float> (x, y, 48.0f, 16.0f);
        g.setColour (on ? c : juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? juce::Colours::white : juce::Colours::white.withAlpha (0.45f));
        g.setFont (11.0f);
        g.drawText (t, r, juce::Justification::centred);
    }

    CohesionAudioProcessor& proc;
    juce::Rectangle<float> avgBadge;

    static constexpr int kHist = 240;
    float histIn[kHist];
    float histOut[kHist];
    int   histPos = 0;
};

// ===========================================================================
//  Help / tips overlay (opened by the "?" button).
// ===========================================================================
class HelpOverlay : public juce::Component
{
public:
    HelpOverlay()
    {
        close.setButtonText ("Sluiten");
        addAndMakeVisible (close);
        close.onClick = [this] { setVisible (false); };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.62f));

        auto panel = getLocalBounds().reduced (54, 34).toFloat();
        g.setColour (juce::Colour (0xff1b1f24));
        g.fillRoundedRectangle (panel, 10.0f);
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.40f));
        g.drawRoundedRectangle (panel, 10.0f, 1.2f);

        auto inner = panel.reduced (24.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font ("Segoe UI", 18.0f, juce::Font::bold));
        g.drawText ("Hoe werkt Cohesion?", inner.removeFromTop (28), juce::Justification::topLeft);

        g.setColour (juce::Colour (0xffcdd6d1));
        g.setFont (juce::Font ("Segoe UI", 13.0f, juce::Font::plain));
        g.drawMultiLineText (bodyText, (int) inner.getX(), (int) inner.getY() + 20,
                             (int) inner.getWidth(), juce::Justification::left, 1.15f);
    }

    void resized() override
    {
        auto panel = getLocalBounds().reduced (54, 34);
        close.setBounds (panel.getRight() - 24 - 90, panel.getBottom() - 24 - 30, 90, 30);
    }

    juce::TextButton close;

    juce::String bodyText =
        juce::String::fromUTF8 (
        "MAGIC schakelt de hele keten in: EQ-shelves  ->  bus-compressor  ->  limiter.\n"
        "Amount is een macro die alles tegelijk 'dikker' maakt (meer compressie, makeup, low en air).\n\n"
        "De grafiek toont live je ingangsniveau (groen) en uitgangsniveau (grijs) die achter de "
        "transfer-curve doorlopen. De curve met knie laat zien vanaf welk niveau (threshold) en hoe "
        "hard er wordt gecomprimeerd. De dunne teal streeplijn is de limiter-ceiling.\n"
        "Zet de grafiek op AVG voor een gemiddeld beeld i.p.v. super-dynamisch.\n\n"
        "TIPS\n"
        "-  Begin met een preset en draai daarna pas aan Amount.\n"
        "-  Soft knee = zachtere, muzikalere inzet; Hard knee = directer/punchy.\n"
        "-  Zet SC HPF aan zodat de bas de compressie niet laat pompen.\n"
        "-  Gebruik SC LISTEN om te horen wat de compressor 'ziet'.\n"
        "-  Mix lager = parallel-/NY-compressie: punch behouden, lijm toevoegen.\n"
        "-  Houd Ceiling net onder 0 dB om de laatste pieken te vangen.");
};

// ===========================================================================
class CohesionAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit CohesionAudioProcessorEditor (CohesionAudioProcessor&);
    ~CohesionAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 920;
    static constexpr int kBaseH = 640;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void setupFader (juce::Slider& s, juce::Label& l, const juce::String& id,
                     const juce::String& text, const juce::String& suffix, const juce::String& tip);
    void setupCombo (juce::ComboBox& c, juce::Label& l, const juce::String& id,
                     const juce::StringArray& items, const juce::String& text, const juce::String& tip);
    void setupTrim  (juce::Slider& s, juce::Label& l, const juce::String& id,
                     const juce::String& text, const juce::String& tip);
    void layoutContent();
    void selectProgram (int idx);
    void refreshUserPresets();
    juce::File userPresetDir() const;

    CohesionAudioProcessor& proc;
    CohesionLookAndFeel lnf;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::TooltipWindow> tooltip;   // created only when tips are on
    juce::ToggleButton tipsToggle { "Tips" };

    juce::Component content;   // base-size canvas, scaled to fit (resizable)

    // header
    juce::Label title, subtitle;
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, abBtn { "A" }, saveBtn { "Save" };
    MagicButton magic;
    juce::Slider amount;
    juce::Label  amountLabel;

    // readouts + input/output trims
    Readout inReadout  { "INPUT",          [this] { return proc.getInputLevelDb();        }, juce::Colour (0xff5dcaa5) };
    Readout grReadout  { "GAIN REDUCTION", [this] { return proc.getCompGainReductionDb(); }, juce::Colour (0xffef9f27), true };
    Readout outReadout { "OUTPUT",         [this] { return proc.getOutputLevelDb();       }, juce::Colour (0xff39ffb0) };
    juce::Slider inTrim, outTrim;
    juce::Label  inTrimL, outTrimL;

    // display + meters
    TransferCurve curve { proc };
    LevelMeter inMeter  { "IN",  [this] { return proc.getInputLevelDb();  } };
    LevelMeter outMeter { "OUT", [this] { return proc.getOutputLevelDb(); } };

    // group headers
    juce::Label compHeader, eqHeader, outHeader, scHeader;

    // compressor controls
    juce::Slider threshold, makeup;
    juce::Label  thresholdL, makeupL;
    juce::ComboBox ratio, attack, release, knee;
    juce::Label  ratioL, attackL, releaseL, kneeL;

    // EQ controls
    juce::Slider hpf, lpf, low, air;
    juce::Label  hpfL, lpfL, lowL, airL;

    // output controls
    juce::Slider mix, ceiling;
    juce::Label  mixL, ceilingL;

    // sidechain
    juce::ToggleButton scOn { "SC HPF" };
    juce::ToggleButton scListen { "SC LISTEN" };
    juce::Slider scFreq;
    juce::Label  scFreqL;

    // help
    juce::TextButton helpButton { "?" };
    HelpOverlay helpView;

    // A/B compare + user presets
    juce::ValueTree stateA, stateB;
    bool onB = false;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::File> userFiles;

    std::vector<std::unique_ptr<SliderAtt>> sliderAtts;
    std::vector<std::unique_ptr<ComboAtt>>  comboAtts;
    std::vector<std::unique_ptr<ButtonAtt>> buttonAtts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionAudioProcessorEditor)
};
