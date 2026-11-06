#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <functional>
#include <cmath>
#include <complex>
#include "PluginProcessor.h"

// Interactive EQ display: live spectrum behind, analytic EQ response in front,
// draggable band / HPF / LPF nodes. Matches the Cohesion house style.
class EqCurve : public juce::Component, private juce::Timer
{
public:
    static constexpr int kBands = CohesionEQCompAudioProcessor::kNumBands;

    std::function<void(int)> onSelect;   // selected node (0..kBands-1 bands, 100 HPF, 101 LPF, -1 none)

    explicit EqCurve (CohesionEQCompAudioProcessor& p)
        : proc (p),
          fft (CohesionEQCompAudioProcessor::fftOrder),
          window ((size_t) CohesionEQCompAudioProcessor::fftSize,
                  juce::dsp::WindowingFunction<float>::hann)
    {
        for (auto& v : scopeDb)    v = -120.0f;
        for (auto& v : scopePreDb) v = -120.0f;
        startTimerHz (30);
    }

    int  getSelected() const { return selected; }
    void setSelected (int s) { selected = s; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (area, 6.0f, 1.0f);

        plot = area.reduced (10.0f, 10.0f);
        plot.removeFromRight (18.0f);

        // grid
        g.setFont (9.0f);
        const float freqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        for (float f : freqs)
        {
            const float x = xFor (f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (juce::Colour (0xff6c7a74));
            g.drawText (f >= 1000 ? juce::String (f / 1000, 0) + "k" : juce::String ((int) f),
                        x - 14.0f, plot.getBottom() + 1.0f, 28.0f, 10.0f, juce::Justification::centred);
        }
        for (int db = -12; db <= 12; db += 6)
        {
            const float y = yFor ((float) db);
            g.setColour (db == 0 ? juce::Colours::white.withAlpha (0.12f) : juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (juce::Colour (0xff6c7a74));
            g.drawText (juce::String (db), plot.getRight() + 1.0f, y - 6.0f, 18.0f, 12.0f, juce::Justification::centredLeft);
        }

        // spectrum behind — "before" (grey) and "after" (teal)
        const int bins = CohesionEQCompAudioProcessor::fftSize / 2;
        const double fs = proc.getSampleRateHz();

        auto buildSpec = [&] (const float* sdb)
        {
            juce::Path spec;
            bool started = false;
            for (float px = plot.getX(); px <= plot.getRight(); px += 2.0f)
            {
                const float f = freqAt (px);
                const float bf = juce::jlimit (1.0f, (float) bins - 2.0f, f / ((float) fs * 0.5f) * (float) bins);
                const int b0 = (int) bf; const float fr = bf - (float) b0;
                const float db = sdb[b0] * (1.0f - fr) + sdb[b0 + 1] * fr;   // interpolate between bins
                const float y = juce::jmap (juce::jlimit (-100.0f, 0.0f, db), -100.0f, 0.0f, plot.getBottom(), plot.getY());
                if (! started) { spec.startNewSubPath (px, plot.getBottom()); spec.lineTo (px, y); started = true; }
                else spec.lineTo (px, y);
            }
            spec.lineTo (plot.getRight(), plot.getBottom());
            spec.closeSubPath();
            return spec;
        };

        // before (input): dim grey fill + faint outline
        auto specPre = buildSpec (scopePreDb);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillPath (specPre);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.strokePath (specPre, juce::PathStrokeType (1.0f));

        // after (output): teal fill
        auto spec = buildSpec (scopeDb);
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.12f));
        g.fillPath (spec);

        // EQ response curve
        juce::Path curve;
        for (float px = plot.getX(); px <= plot.getRight(); px += 1.0f)
        {
            const float y = yFor (juce::jlimit (-18.0f, 18.0f, (float) totalDb (freqAt (px))));
            if (px <= plot.getX()) curve.startNewSubPath (px, y);
            else curve.lineTo (px, y);
        }
        g.setColour (juce::Colour (0xff39ffb0));
        g.strokePath (curve, juce::PathStrokeType (2.2f));

        // nodes
        if (on ("hpOn")) drawNode (g, 100, xFor (get ("hpFreq")), yFor ((float) totalDb (get ("hpFreq"))), "H");
        if (on ("lpOn")) drawNode (g, 101, xFor (get ("lpFreq")), yFor ((float) totalDb (get ("lpFreq"))), "L");
        for (int b = 0; b < kBands; ++b)
            if (on (bid (b, "on")))
                drawNode (g, b, xFor (get (bid (b, "freq"))), yFor (get (bid (b, "gain"))), juce::String (b + 1));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        selected = nodeAt (e.position);
        if (onSelect) onSelect (selected);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (selected < 0) return;
        const float f = juce::jlimit (20.0f, 20000.0f, freqAt (e.position.x));
        if (selected == 100) setP ("hpFreq", f);
        else if (selected == 101) setP ("lpFreq", f);
        else
        {
            setP (bid (selected, "freq"), f);
            const float gain = juce::jlimit (-18.0f, 18.0f, dbAt (e.position.y));
            setP (bid (selected, "gain"), gain);
        }
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (selected < 0 || selected >= kBands) return;
        const float q = juce::jlimit (0.1f, 10.0f, get (bid (selected, "q")) * (1.0f + w.deltaY * 0.5f));
        setP (bid (selected, "q"), q);
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int n = nodeAt (e.position);
        if (n == 100)      setP ("hpOn", on ("hpOn") ? 0.0f : 1.0f);
        else if (n == 101) setP ("lpOn", on ("lpOn") ? 0.0f : 1.0f);
        else if (n >= 0)   setP (bid (n, "on"), on (bid (n, "on")) ? 0.0f : 1.0f);
        repaint();
    }

private:
    void timerCallback() override
    {
        if (! isShowing()) return;
        const int bins = CohesionEQCompAudioProcessor::fftSize / 2;

        if (proc.isFftBlockReadyPre())
        {
            proc.readFftBlockPre (fftData);
            window.multiplyWithWindowingTable (fftData, (size_t) CohesionEQCompAudioProcessor::fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData);
            for (int i = 0; i < bins; ++i)
            {
                const float db = juce::Decibels::gainToDecibels (fftData[i] / (float) bins, -120.0f) + 12.0f;
                scopePreDb[i] = juce::jmax (db, scopePreDb[i] - 2.0f);
            }
        }

        if (! proc.isFftBlockReady()) { repaint(); return; }
        proc.readFftBlock (fftData);
        window.multiplyWithWindowingTable (fftData, (size_t) CohesionEQCompAudioProcessor::fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData);
        for (int i = 0; i < bins; ++i)
        {
            const float mag = fftData[i] / (float) bins;
            const float db = juce::Decibels::gainToDecibels (mag, -120.0f) + 12.0f;
            scopeDb[i] = juce::jmax (db, scopeDb[i] - 2.0f);   // fast attack, slow decay
        }
        repaint();
    }

    // --- coordinate maps ---
    float xFor (float f) const
    {
        return plot.getX() + std::log (f / 20.0f) / std::log (20000.0f / 20.0f) * plot.getWidth();
    }
    float freqAt (float x) const
    {
        return 20.0f * std::pow (20000.0f / 20.0f, (x - plot.getX()) / plot.getWidth());
    }
    float yFor (float db) const { return plot.getCentreY() - (db / 18.0f) * (plot.getHeight() * 0.5f); }
    float dbAt (float y)  const { return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * 18.0f; }

    // --- param helpers ---
    static juce::String bid (int b, const char* f) { return "b" + juce::String (b) + "_" + f; }
    float get (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    bool  on  (const juce::String& id) const { return get (id) > 0.5f; }
    void  setP (const juce::String& id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    int nodeAt (juce::Point<float> pt)
    {
        auto near = [&] (float x, float y) { return pt.getDistanceFrom ({ x, y }) < 12.0f; };
        if (on ("hpOn") && near (xFor (get ("hpFreq")), yFor ((float) totalDb (get ("hpFreq"))))) return 100;
        if (on ("lpOn") && near (xFor (get ("lpFreq")), yFor ((float) totalDb (get ("lpFreq"))))) return 101;
        for (int b = 0; b < kBands; ++b)
            if (on (bid (b, "on")) && near (xFor (get (bid (b, "freq"))), yFor (get (bid (b, "gain")))))
                return b;
        return -1;
    }

    void drawNode (juce::Graphics& g, int idx, float x, float y, const juce::String& label)
    {
        const bool sel = (idx == selected);
        g.setColour (sel ? juce::Colour (0xff39ffb0) : juce::Colour (0xff0f6e56));
        g.fillEllipse (x - 10.0f, y - 10.0f, 20.0f, 20.0f);
        g.setColour (sel ? juce::Colours::white : juce::Colour (0xff9fe1cb));
        g.drawEllipse (x - 10.0f, y - 10.0f, 20.0f, 20.0f, sel ? 2.0f : 1.2f);
        g.setColour (sel ? juce::Colour (0xff07130f) : juce::Colour (0xffeafff6));
        g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        g.drawText (label, juce::Rectangle<float> (x - 10.0f, y - 10.0f, 20.0f, 20.0f), juce::Justification::centred);
    }

    // analytic RBJ magnitude (dB) of one filter at probe frequency
    double biqMagDb (int type, double f, double gainDb, double Q, double probe) const
    {
        const double fs = proc.getSampleRateHz();
        f = juce::jlimit (10.0, fs * 0.49, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double c = std::cos (w0), s = std::sin (w0);
        const double alpha = s / (2.0 * Q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
        switch (type)
        {
            case 0: b0 = 1 + alpha * A; b1 = -2 * c; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * c; a2 = 1 - alpha / A; break;
            case 1: { const double sa = 2 * std::sqrt (A) * alpha;
                      b0 = A * ((A + 1) - (A - 1) * c + sa); b1 = 2 * A * ((A - 1) - (A + 1) * c); b2 = A * ((A + 1) - (A - 1) * c - sa);
                      a0 = (A + 1) + (A - 1) * c + sa; a1 = -2 * ((A - 1) + (A + 1) * c); a2 = (A + 1) + (A - 1) * c - sa; } break;
            case 2: { const double sa = 2 * std::sqrt (A) * alpha;
                      b0 = A * ((A + 1) + (A - 1) * c + sa); b1 = -2 * A * ((A - 1) + (A + 1) * c); b2 = A * ((A + 1) + (A - 1) * c - sa);
                      a0 = (A + 1) - (A - 1) * c + sa; a1 = 2 * ((A - 1) - (A + 1) * c); a2 = (A + 1) - (A - 1) * c - sa; } break;
            case 3: b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
            case 4: b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
        }
        const double w = 2.0 * juce::MathConstants<double>::pi * probe / fs;
        const std::complex<double> z1 (std::cos (-w), std::sin (-w));
        const std::complex<double> z2 (std::cos (-2 * w), std::sin (-2 * w));
        const auto num = b0 + b1 * z1 + b2 * z2;
        const auto den = a0 + a1 * z1 + a2 * z2;
        return 20.0 * std::log10 (std::abs (num / den) + 1.0e-9);
    }

    double totalDb (double probe) const
    {
        double sum = 0.0;
        if (on ("hpOn")) sum += biqMagDb (3, get ("hpFreq"), 0.0, 0.70710678, probe);
        if (on ("lpOn")) sum += biqMagDb (4, get ("lpFreq"), 0.0, 0.70710678, probe);
        for (int b = 0; b < kBands; ++b)
            if (on (bid (b, "on")))
            {
                const int t = (int) get (bid (b, "type"));   // 0 bell,1 ls,2 hs
                sum += biqMagDb (t, get (bid (b, "freq")), get (bid (b, "gain")), get (bid (b, "q")), probe);
            }
        return sum;
    }

    CohesionEQCompAudioProcessor& proc;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * CohesionEQCompAudioProcessor::fftSize] { 0.0f };
    float scopeDb[CohesionEQCompAudioProcessor::fftSize / 2];
    float scopePreDb[CohesionEQCompAudioProcessor::fftSize / 2];
    juce::Rectangle<float> plot;
    int selected = -1;
};
