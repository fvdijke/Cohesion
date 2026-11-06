#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include "PluginProcessor.h"

// Multiband crossover display: live spectrum with draggable split lines and
// per-band numbers / crossover-frequency labels. Matches the mockup.
class MbCrossover : public juce::Component, private juce::Timer
{
public:
    explicit MbCrossover (CohesionEQCompAudioProcessor& p)
        : proc (p), fft (CohesionEQCompAudioProcessor::fftOrder),
          window ((size_t) CohesionEQCompAudioProcessor::fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        for (auto& v : scopeDb)    v = -120.0f;
        for (auto& v : scopePreDb) v = -120.0f;
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (area, 6.0f, 1.0f);
        plot = area.reduced (10.0f, 8.0f);

        // grid
        g.setFont (9.0f);
        const float gf[] = { 100, 1000, 10000 };
        for (float f : gf)
        {
            const float x = xFor (f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        }

        const int nb = numBands();

        // band regions: per-band colour matching the sliders / meters
        {
            float lx = plot.getX();
            for (int b = 0; b < nb; ++b)
            {
                const float rx = (b < nb - 1) ? xFor (get ("x" + juce::String (b + 1))) : plot.getRight();
                g.setColour (bandColour (b).withAlpha (0.08f));
                g.fillRect (juce::Rectangle<float> (lx, plot.getY(), rx - lx, plot.getHeight()));
                lx = rx;
            }
        }

        const int bins = CohesionEQCompAudioProcessor::fftSize / 2;
        const double fs = proc.getSampleRateHz();

        auto buildSpec = [&] (const float* sdb)
        {
            juce::Path spec;
            bool started = false;
            for (float px = plot.getX(); px <= plot.getRight(); px += 2.0f)
            {
                const float bf = juce::jlimit (1.0f, (float) bins - 2.0f, freqAt (px) / ((float) fs * 0.5f) * (float) bins);
                const int b0 = (int) bf; const float fr = bf - (float) b0;
                const float db = sdb[b0] * (1.0f - fr) + sdb[b0 + 1] * fr;
                const float y = juce::jmap (juce::jlimit (-100.0f, 0.0f, db), -100.0f, 0.0f, plot.getBottom(), plot.getY());
                if (! started) { spec.startNewSubPath (px, plot.getBottom()); spec.lineTo (px, y); started = true; }
                else spec.lineTo (px, y);
            }
            spec.lineTo (plot.getRight(), plot.getBottom());
            spec.closeSubPath();
            return spec;
        };

        // before (input): dim grey
        auto specPre = buildSpec (scopePreDb);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillPath (specPre);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (specPre, juce::PathStrokeType (1.0f));

        // after (output): teal
        auto spec = buildSpec (scopeDb);
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.14f));
        g.fillPath (spec);

        // crossover lines + freq labels
        for (int i = 0; i < nb - 1; ++i)
        {
            const float x = xFor (get ("x" + juce::String (i + 1)));
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.55f));
            const float dash[] = { 5.0f, 4.0f };
            g.drawDashedLine (juce::Line<float> (x, plot.getY(), x, plot.getBottom()), dash, 2, 1.2f);
            g.setColour (juce::Colour (0xff15181c));
            auto lab = juce::Rectangle<float> (x - 34.0f, plot.getBottom() - 16.0f, 68.0f, 14.0f);
            g.fillRoundedRectangle (lab, 3.0f);
            g.setColour (juce::Colour (0xff9fe1cb));
            g.drawText (freqText (get ("x" + juce::String (i + 1))), lab, juce::Justification::centred);
        }

        // band numbers centered in each region
        float leftX = plot.getX();
        for (int b = 0; b < nb; ++b)
        {
            const float rightX = (b < nb - 1) ? xFor (get ("x" + juce::String (b + 1))) : plot.getRight();
            g.setColour (bandColour (b));
            g.setFont (juce::Font ("Segoe UI", 14.0f, juce::Font::bold));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (leftX, plot.getY() + 4.0f, rightX - leftX, 18.0f),
                        juce::Justification::centred);
            leftX = rightX;
        }

        // per-band threshold lines
        {
            float lx = plot.getX();
            for (int b = 0; b < nb; ++b)
            {
                const float rx = (b < nb - 1) ? xFor (get ("x" + juce::String (b + 1))) : plot.getRight();
                const float thr = get ("mb" + juce::String (b) + "_thr");
                const float y = juce::jmap (juce::jlimit (-100.0f, 0.0f, thr), -100.0f, 0.0f, plot.getBottom(), plot.getY());
                g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.75f));
                g.drawLine (lx + 3.0f, y, rx - 3.0f, y, 1.4f);
                lx = rx;
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { dragLine = lineAt (e.position.x); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragLine < 0) return;
        float f = juce::jlimit (20.0f, 20000.0f, freqAt (e.position.x));
        // keep ascending between neighbours
        if (dragLine > 0)              f = juce::jmax (f, get ("x" + juce::String (dragLine))     + 1.0f);
        if (dragLine < numBands() - 2) f = juce::jmin (f, get ("x" + juce::String (dragLine + 2)) - 1.0f);
        setP ("x" + juce::String (dragLine + 1), f);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { dragLine = -1; }

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
            const float db = juce::Decibels::gainToDecibels (fftData[i] / (float) bins, -120.0f) + 12.0f;
            scopeDb[i] = juce::jmax (db, scopeDb[i] - 2.0f);
        }
        repaint();
    }

    float xFor (float f) const { return plot.getX() + std::log (f / 20.0f) / std::log (1000.0f) * plot.getWidth(); }
    float freqAt (float x) const { return 20.0f * std::pow (1000.0f, (x - plot.getX()) / plot.getWidth()); }

    int lineAt (float x)
    {
        for (int i = 0; i < numBands() - 1; ++i)
            if (std::abs (x - xFor (get ("x" + juce::String (i + 1)))) < 8.0f) return i;
        return -1;
    }

    static juce::String freqText (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) f) + " Hz";
    }
    static juce::Colour bandColour (int b)
    {
        const juce::Colour c[5] = { juce::Colour (0xffc98a5a), juce::Colour (0xffcabf63),
                                    juce::Colour (0xff6fc88a), juce::Colour (0xff5aa6d8), juce::Colour (0xffb08fd8) };
        return c[juce::jlimit (0, 4, b)];
    }
    int numBands() const { return 2 + (int) proc.apvts.getRawParameterValue ("mbBands")->load(); }
    float get (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void  setP (const juce::String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); }

    CohesionEQCompAudioProcessor& proc;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float fftData[2 * CohesionEQCompAudioProcessor::fftSize] { 0.0f };
    float scopeDb[CohesionEQCompAudioProcessor::fftSize / 2];
    float scopePreDb[CohesionEQCompAudioProcessor::fftSize / 2];
    juce::Rectangle<float> plot;
    int dragLine = -1;
};
