#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <memory>
#include <cmath>
#include "PluginProcessor.h"
#include "CohesionUpdateCheck.h"
#include "LookAndFeel.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

// ===========================================================================
//  Live transient display: input (grey) vs shaped (teal) peak envelopes.
// ===========================================================================
class TransientView : public juce::Component, private juce::Timer
{
public:
    explicit TransientView (CohesionTransientAudioProcessor& p) : proc (p)
    {
        for (auto& v : dIn) v = 0.0f;
        for (auto& v : dSh) v = 0.0f;
        avgBtn.setButtonText ("AVG");
        avgBtn.setClickingTogglesState (true);
        avgBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff0e1013));
        avgBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff0f6e56));
        avgBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff9fb0aa));
        avgBtn.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffeafff6));
        avgBtn.onClick = [this] { repaint(); };
        addAndMakeVisible (avgBtn);
        startTimerHz (30);
    }

    void resized() override { avgBtn.setBounds (16, getHeight() - 32, 48, 22); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (area, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (area, 7.0f, 1.0f);

        g.setColour (juce::Colour (0xff5dcaa5));
        g.setFont (juce::Font ("Segoe UI", 10.5f, juce::Font::bold).withExtraKerningFactor (0.14f));
        g.drawText ("TRANSIENT SHAPING", 16, 10, 300, 16, juce::Justification::centredLeft);

        // legend
        g.setFont (juce::Font ("Segoe UI", 9.5f, juce::Font::plain));
        g.setColour (juce::Colours::white.withAlpha (0.35f)); g.fillRect (area.getRight() - 150.0f, 18.0f, 16.0f, 2.0f);
        g.setColour (juce::Colour (0xff9fb0aa)); g.drawText ("input", (int) area.getRight() - 128, 12, 50, 14, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff39ffb0)); g.fillRect (area.getRight() - 84.0f, 18.0f, 16.0f, 2.0f);
        g.setColour (juce::Colour (0xff9fb0aa)); g.drawText ("shaped", (int) area.getRight() - 62, 12, 60, 14, juce::Justification::centredLeft);

        auto p = area.reduced (16.0f); p.removeFromTop (22.0f);
        const float base = p.getBottom() - 6.0f;
        const float amp  = p.getHeight() - 20.0f;

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) base, p.getX(), p.getRight());

        const int N = CohesionTransientAudioProcessor::kScope;
        const int M = 56;                               // few points -> rounded, flowing curves
        auto smoothTop = [&] (const float* d)
        {
            std::vector<float> X, Y;
            for (int m = 0; m < M; ++m)
            {
                const float t  = (float) m / (float) (M - 1);
                const float fi = juce::jlimit (0.0f, (float) (N - 1), t * (float) (N - 1));
                const int i0 = (int) fi; const float fr = fi - (float) i0;
                const float v = d[i0] * (1.0f - fr) + d[juce::jmin (N - 1, i0 + 1)] * fr;
                X.push_back (p.getX() + t * p.getWidth());
                Y.push_back (base - std::sqrt (juce::jlimit (0.0f, 1.3f, v)) * amp * 0.82f);
            }
            juce::Path pth; pth.startNewSubPath (X[0], Y[0]);
            for (int i = 0; i < M - 1; ++i)             // Catmull-Rom
            {
                const int i0 = i > 0 ? i - 1 : 0, i3 = i < M - 2 ? i + 2 : i + 1;
                const float c1x = X[i] + (X[i + 1] - X[i0]) / 6.0f, c1y = Y[i] + (Y[i + 1] - Y[i0]) / 6.0f;
                const float c2x = X[i + 1] - (X[i3] - X[i]) / 6.0f, c2y = Y[i + 1] - (Y[i3] - Y[i]) / 6.0f;
                pth.cubicTo (c1x, c1y, c2x, c2y, X[i + 1], Y[i + 1]);
            }
            return pth;
        };
        auto fillTo = [&] (juce::Path top) { top.lineTo (p.getRight(), base); top.lineTo (p.getX(), base); top.closeSubPath(); return top; };

        auto inTop = smoothTop (dIn);
        auto shTop = smoothTop (dSh);
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillPath (fillTo (inTop));
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.15f)); g.fillPath (fillTo (shTop));
        g.setColour (juce::Colours::white.withAlpha (0.32f));
        g.strokePath (inTop, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff39ffb0));
        g.strokePath (shTop, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    void timerCallback() override
    {
        float si[CohesionTransientAudioProcessor::kScope], ss[CohesionTransientAudioProcessor::kScope];
        proc.readScope (si, ss);
        const float k = avgBtn.getToggleState() ? 0.12f : 0.5f;   // AVG -> calmer, averaged view
        for (int i = 0; i < CohesionTransientAudioProcessor::kScope; ++i)
        {
            dIn[i] = dIn[i] * (1.0f - k) + si[i] * k;
            dSh[i] = dSh[i] * (1.0f - k) + ss[i] * k;
        }
        repaint();
    }

    CohesionTransientAudioProcessor& proc;
    juce::TextButton avgBtn;
    float dIn[CohesionTransientAudioProcessor::kScope];
    float dSh[CohesionTransientAudioProcessor::kScope];
};

// ===========================================================================
class CohesionTransientAudioProcessorEditor : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit CohesionTransientAudioProcessorEditor (CohesionTransientAudioProcessor&);
    ~CohesionTransientAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void addKnob (juce::Slider& s, juce::Label& l, const juce::String& id, const juce::String& name, const juce::String& suffix);

    CohesionTransientAudioProcessor& proc;
    CohesionLookAndFeel lnf;

    juce::Label title, subtitle, subcap;
    cohesion::UpdateBadge updateBadge;   // UPDATE AVAILABLE (start-up version check)
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn;

    TransientView viz { proc };

    juce::Slider attackS, sustainS, sensS, outS;
    juce::Label  attackL, sustainL, sensL, outL;
    std::vector<std::unique_ptr<SliderAtt>> atts;

    float outMeter = 0.0f;

    static constexpr int kBaseW = 980, kBaseH = 520;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionTransientAudioProcessorEditor)
};
