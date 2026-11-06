#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <memory>
#include <cmath>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

// ===========================================================================
//  Rounded double-tracking visualizer: original + delayed/wobbled double,
//  delay bracket and a stereo-width fan. Smooth filled waveforms.
// ===========================================================================
class AdtVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit AdtVisualizer (CohesionADTAudioProcessor& p) : proc (p)
    {
        for (auto& v : disp) v = 0.0f;
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (area, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (area, 7.0f, 1.0f);

        g.setColour (juce::Colour (0xff5dcaa5));
        g.setFont (juce::Font ("Segoe UI", 10.5f, juce::Font::bold).withExtraKerningFactor (0.14f));
        g.drawText ("DOUBLE TRACKING", 16, 10, 260, 16, juce::Justification::centredLeft);

        auto wave = area.reduced (16.0f);
        wave.removeFromTop (34.0f);
        const float waveRight = wave.getRight() - 170.0f;   // reserve right side for the width fan
        const float x0 = wave.getX() + 84.0f;               // onset of the original
        const float yc = wave.getCentreY() + 8.0f;          // shared centre line (waves overlaid)
        const float amp = juce::jmin (62.0f, wave.getHeight() * 0.40f);

        const float delayMs = get ("delayMs");
        const float widthN  = get ("width") * 0.01f;
        const float depthN  = get ("lfoDepth") * 0.01f;
        const float rateN   = juce::jmax (0.2f, get ("lfoRate"));
        const float xShift  = juce::jmap (delayMs, 5.0f, 40.0f, 10.0f, 40.0f);   // visual delay offset
        const float sepV    = juce::jlimit (0.0f, 1.0f, widthN);                  // width 100% -> fully separated
        const float yoff    = sepV * amp * 0.6f;                                  // WIDTH pulls the waves apart
        const float ampE    = amp * (1.0f - 0.4f * sepV);
        const float yO = yc - yoff;   // original / Left  (up)
        const float yD = yc + yoff;   // double   / Right (down)

        // centre reference
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) yc, wave.getX(), waveRight);

        // original (teal, L) + double (cyan, R). At 0% width they overlap; WIDTH pulls them apart.
        // The double is a continuously phase-shifted copy (delay); Rate = wobble density, Depth = amount.
        drawWave (g, x0, waveRight, yO, ampE, 0.0f, 0.0f, 0.0f, 0.0f, juce::Colour (0xff39ffb0));
        drawWave (g, x0, waveRight, yD, ampE * 0.96f, -(xShift * 0.085f), depthN * 2.2f, 0.004f + rateN * 0.02f, phase, juce::Colour (0xff5ad0ff));

        // legend
        g.setFont (juce::Font ("Segoe UI", 9.5f, juce::Font::plain));
        g.setColour (juce::Colour (0xff39ffb0)); g.fillRect (wave.getX(), wave.getY() + 3.0f, 16.0f, 2.0f);
        g.setColour (juce::Colour (0xff9fb0aa)); g.drawText ("origineel  L", (int) wave.getX() + 22, (int) wave.getY() - 3, 90, 12, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff5ad0ff)); g.fillRect (wave.getX() + 110.0f, wave.getY() + 3.0f, 16.0f, 2.0f);
        g.setColour (juce::Colour (0xff9fb0aa)); g.drawText ("dubbel  R (+" + juce::String ((int) delayMs) + " ms)", (int) wave.getX() + 132, (int) wave.getY() - 3, 170, 12, juce::Justification::centredLeft);

        // delay offset markers
        g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawVerticalLine ((int) x0, wave.getY() + 16.0f, wave.getBottom());
        g.setColour (juce::Colour (0xff5ad0ff).withAlpha (0.35f)); g.drawVerticalLine ((int) (x0 + xShift), wave.getY() + 16.0f, wave.getBottom());
        g.setColour (juce::Colour (0xff15181c)); g.fillRoundedRectangle (x0 + xShift * 0.5f - 22.0f, wave.getBottom() - 15.0f, 44.0f, 14.0f, 3.0f);
        g.setColour (juce::Colour (0xff9fe1cb)); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText (juce::String ((int) delayMs) + " ms", (int) (x0 + xShift * 0.5f - 22.0f), (int) (wave.getBottom() - 15.0f), 44, 14, juce::Justification::centred);

        // stereo width fan (right)
        {
            const float cx = area.getRight() - 92.0f, cy = area.getCentreY() + 28.0f;
            const float half = juce::jmap (juce::jlimit (0.0f, 2.0f, widthN), 0.0f, 2.0f, 4.0f, 62.0f) * juce::MathConstants<float>::pi / 180.0f;
            const float len = 92.0f;
            const float lx = cx - std::sin (half) * len, rx = cx + std::sin (half) * len, ty = cy - std::cos (half) * len;
            juce::Path fan; fan.startNewSubPath (cx, cy); fan.lineTo (lx, ty);
            fan.addArc (cx - len, cy - len, len * 2.0f, len * 2.0f, -half, half); fan.closeSubPath();
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.10f)); g.fillPath (fan);
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.5f));
            g.drawLine (cx, cy, lx, ty, 1.0f); g.drawLine (cx, cy, rx, ty, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawLine (cx, cy, cx, cy - len, 1.0f);
            g.setColour (juce::Colour (0xff5ad0ff)); g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::plain)); g.drawText ("L", (int) lx - 12, (int) ty - 6, 12, 12, juce::Justification::centred);
            g.setColour (juce::Colour (0xffb08fd8)); g.drawText ("R", (int) rx, (int) ty - 6, 12, 12, juce::Justification::centred);
            g.setColour (juce::Colour (0xff9fb0aa)); g.setFont (juce::Font ("Segoe UI", 9.5f, juce::Font::plain)); g.drawText ("WIDTH", (int) cx - 40, (int) cy - len - 16, 80, 12, juce::Justification::centred);
            g.setColour (juce::Colour (0xffeafff6)); g.setFont (juce::Font ("Segoe UI", 13.0f, juce::Font::bold));
            g.drawText (juce::String ((int) (widthN * 100.0f)) + "%", (int) cx - 40, (int) cy + 8, 80, 16, juce::Justification::centred);
        }
    }

private:
    void timerCallback() override
    {
        float s[CohesionADTAudioProcessor::kScope];
        proc.readScope (s);
        for (int i = 0; i < CohesionADTAudioProcessor::kScope; ++i)
            disp[i] = disp[i] * 0.6f + s[i] * 0.4f;     // temporal smoothing -> rounder
        phase += 0.12f; if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    // Continuous flowing waveform. phaseOff shifts it horizontally (the delay),
    // modDepth/modRateK add the LFO wobble (so Rate changes the density, Depth the amount).
    void drawWave (juce::Graphics& g, float xA, float xB, float yc, float amp,
                   float phaseOff, float modDepth, float modRateK, float anim, juce::Colour c)
    {
        const int N = CohesionADTAudioProcessor::kScope;
        const float span = juce::jmax (1.0f, xB - xA);
        const float carrierK = 0.085f;
        juce::Path wf; bool started = false;
        for (float x = xA; x <= xB; x += 3.0f)
        {
            const float t  = (x - xA) / span;
            const float fi = juce::jlimit (0.0f, (float) (N - 1), t * (float) (N - 1));
            const int i0 = (int) fi; const float fr = fi - (float) i0;
            const float a = disp[i0] * (1.0f - fr) + disp[juce::jmin (N - 1, i0 + 1)] * fr;
            const float e = 0.16f + std::sqrt (juce::jlimit (0.0f, 1.0f, a)) * 0.9f;   // live envelope + floor
            const float ph = x * carrierK + phaseOff + modDepth * std::sin (x * modRateK + anim);
            const float y = yc + e * amp * std::sin (ph);
            if (! started) { wf.startNewSubPath (x, y); started = true; } else wf.lineTo (x, y);
        }
        g.setColour (c.withAlpha (0.92f));
        g.strokePath (wf, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    float get (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    CohesionADTAudioProcessor& proc;
    float disp[CohesionADTAudioProcessor::kScope];
    float phase = 0.0f;
};

// ===========================================================================
class CohesionADTAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit CohesionADTAudioProcessorEditor (CohesionADTAudioProcessor&);
    ~CohesionADTAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void addKnob (juce::Slider& s, juce::Label& l, const juce::String& id, const juce::String& name, const juce::String& suffix);
    void addToggle (juce::ToggleButton& b, const juce::String& id, const juce::String& txt);

    CohesionADTAudioProcessor& proc;
    CohesionLookAndFeel lnf;

    juce::Label title, subtitle, subcap;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn;

    AdtVisualizer viz { proc };

    // knobs
    juce::Slider delayS, spreadS, detuneS, rateS, depthS, humanizeS, rotateS, widthS, lowMonoS, mixS, outS;
    juce::Label  delayL, spreadL, detuneL, rateL, depthL, humanizeL, rotateL, widthL, lowMonoL, mixL, outL;
    juce::ComboBox shapeC;
    juce::TextButton voicesBtn[2];
    juce::ToggleButton invertBtn, correctBtn, bassMonoBtn, vintageBtn, monoBtn;

    std::vector<std::unique_ptr<SliderAtt>> sAtts;
    std::vector<std::unique_ptr<ComboAtt>>  cAtts;
    std::vector<std::unique_ptr<ButtonAtt>> bAtts;

    static constexpr int kBaseW = 980, kBaseH = 560;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionADTAudioProcessorEditor)
};
