#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "CohesionUpdateCheck.h"
#include "LookAndFeel.h"
#include "Params.h"

using SliderAtt   = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt   = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboAtt    = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

// ---------------------------------------------------------------------------
//  Rotary knob with a caption drawn underneath.
// ---------------------------------------------------------------------------
struct LabeledKnob : juce::Component
{
    juce::Slider s;
    juce::String name;
    std::unique_ptr<SliderAtt> att;

    LabeledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& nm)
        : name (nm)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 17);
        addAndMakeVisible (s);
        att = std::make_unique<SliderAtt> (apvts, id, s);
    }
    void resized() override { auto r = getLocalBounds(); r.removeFromBottom (12); s.setBounds (r); }
    // the value box is built at construction (default look); rebuild it once we are parented
    void parentHierarchyChanged() override { s.sendLookAndFeelChange(); }
    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff9fb0aa));
        g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText (name, getLocalBounds().removeFromBottom (12), juce::Justification::centred);
    }
};

// Dims every child except the ON button (used for bypassed oscillators).
inline void setPanelDimmed (juce::Component& panel, juce::Component& keepBright, bool dimmed)
{
    for (auto* c : panel.getChildren())
        if (c != &keepBright) c->setAlpha (dimmed ? 0.38f : 1.0f);
}

// ---------------------------------------------------------------------------
//  One oscillator panel (OSC 1..4).
// ---------------------------------------------------------------------------
struct OscPanel : juce::Component
{
    int idx;
    bool dimmed = false;
    void setDimmed (bool d) { if (d == dimmed) return; dimmed = d; setPanelDimmed (*this, onBtn, d); updateMode(); }
    juce::ToggleButton onBtn;
    juce::ComboBox modeBox, waveBox, bankBox;
    std::unique_ptr<ButtonAtt> onAtt;
    std::unique_ptr<ComboAtt>  modeAtt, waveAtt, bankAtt;
    juce::OwnedArray<LabeledKnob> knobs;

    OscPanel (juce::AudioProcessorValueTreeState& apvts, int i) : idx (i)
    {
        onBtn.setButtonText ("ON");
        addAndMakeVisible (onBtn);
        onAtt = std::make_unique<ButtonAtt> (apvts, pid::oscOn (i), onBtn);

        modeBox.addItemList ({ "VA", "WT" }, 1);
        addAndMakeVisible (modeBox);
        modeAtt = std::make_unique<ComboAtt> (apvts, pid::oscMode (i), modeBox);

        waveBox.addItemList ({ "Saw", "Square", "Triangle", "Sine" }, 1);
        addAndMakeVisible (waveBox);
        waveAtt = std::make_unique<ComboAtt> (apvts, pid::oscWave (i), waveBox);

        bankBox.addItemList (pid::wtBankNames(), 1);
        addChildComponent (bankBox);
        bankAtt = std::make_unique<ComboAtt> (apvts, pid::oscWtBank (i), bankBox);

        auto add = [&] (const juce::String& id, const juce::String& nm)
        { auto* k = new LabeledKnob (apvts, id, nm); knobs.add (k); addAndMakeVisible (k); };

        add (pid::oscOct (i),  "OCT");   add (pid::oscSemi (i), "SEMI");   add (pid::oscFine (i),  "FINE");
        add (pid::oscWtPos (i),"WT POS");add (pid::oscUni (i),  "UNISON"); add (pid::oscDet (i),   "DETUNE");
        add (pid::oscPan (i),  "PAN");   add (pid::oscLevel (i),"LEVEL");  add (pid::oscPhase (i), "PHASE");

        // VA: the wave box picks the waveform; WT: the bank box (same place) picks the
        // wavetable bank and WT POS morphs through it
        modeBox.onChange = [this] { updateMode(); };
        updateMode();
    }

    void updateMode()
    {
        const bool wt = modeBox.getSelectedId() == 2;
        waveBox.setVisible (! wt);
        bankBox.setVisible (wt);
        if (knobs.size() > 3) knobs[3]->setAlpha (dimmed ? 0.38f : (wt ? 1.0f : 0.38f));   // WT POS
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (dimmed ? juce::Colour (0xff2b3138) : juce::Colour (0xff39ffb0)); g.fillEllipse (14.0f, 14.0f, 10.0f, 10.0f);
        if (dimmed) { g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.5f)); g.drawEllipse (14.0f, 14.0f, 10.0f, 10.0f, 1.0f); }
        g.setColour (dimmed ? juce::Colour (0xff6c7a74) : juce::Colour (0xffe8eef0));
        g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
        g.drawText ("OSC " + juce::String (idx), 32, 10, 120, 18, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (24);
        onBtn.setBounds (top.removeFromRight (46));
        r.removeFromTop (6);
        auto combos = r.removeFromTop (24);
        modeBox.setBounds (combos.removeFromLeft (combos.getWidth() / 2 - 4));
        combos.removeFromLeft (8);
        waveBox.setBounds (combos);
        bankBox.setBounds (combos);
        r.removeFromTop (10);
        const int cols = 3, rows = 3;
        const int cw = r.getWidth() / cols;
        const int ch = r.getHeight() / rows;
        for (int k = 0; k < knobs.size(); ++k)
            knobs[k]->setBounds (r.getX() + (k % cols) * cw, r.getY() + (k / cols) * ch, cw, ch);
    }
};

// ---------------------------------------------------------------------------
//  SUB / NOISE small panels.
// ---------------------------------------------------------------------------
struct MiniPanel : juce::Component
{
    juce::String title;
    bool dimmed = false;
    void setDimmed (bool d) { if (d == dimmed) return; dimmed = d; setPanelDimmed (*this, onBtn, d); repaint(); }
    juce::ToggleButton onBtn;
    juce::ComboBox box;
    std::unique_ptr<ButtonAtt> onAtt;
    std::unique_ptr<ComboAtt>  boxAtt;
    juce::OwnedArray<LabeledKnob> knobs;

    MiniPanel (juce::AudioProcessorValueTreeState& apvts, const juce::String& t,
               const juce::String& onId, const juce::String& boxId,
               const juce::StringArray& boxItems,
               const std::vector<std::pair<juce::String, juce::String>>& knobDefs)
        : title (t)
    {
        onBtn.setButtonText ("ON"); addAndMakeVisible (onBtn);
        onAtt = std::make_unique<ButtonAtt> (apvts, onId, onBtn);
        box.addItemList (boxItems, 1); addAndMakeVisible (box);
        boxAtt = std::make_unique<ComboAtt> (apvts, boxId, box);
        for (auto& d : knobDefs) { auto* k = new LabeledKnob (apvts, d.first, d.second); knobs.add (k); addAndMakeVisible (k); }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (dimmed ? juce::Colour (0xff2b3138) : juce::Colour (0xff39ffb0)); g.fillEllipse (14.0f, 16.0f, 10.0f, 10.0f);
        if (dimmed) { g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.5f)); g.drawEllipse (14.0f, 16.0f, 10.0f, 10.0f, 1.0f); }
        g.setColour (dimmed ? juce::Colour (0xff6c7a74) : juce::Colour (0xffe8eef0));
        g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::bold));
        g.drawText (title, 32, 12, 120, 18, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (24);
        onBtn.setBounds (top.removeFromRight (46));
        r.removeFromTop (6);
        box.setBounds (r.removeFromTop (24));
        r.removeFromTop (8);
        const int cw = r.getWidth() / juce::jmax (1, knobs.size());
        for (int k = 0; k < knobs.size(); ++k)
            knobs[k]->setBounds (r.getX() + k * cw, r.getY(), cw, r.getHeight());
    }
};

// ---------------------------------------------------------------------------
//  LEVELS: 6 faders (OSC1-4, SUB, NOISE) + FM / RING knobs.
// ---------------------------------------------------------------------------
struct MixerPanel : juce::Component
{
    juce::OwnedArray<juce::Slider> faders;
    juce::OwnedArray<SliderAtt> faderAtts;
    juce::StringArray labels;
    bool stripDim[6] {};
    void setStripDimmed (int i, bool d)
    {
        if (i < 0 || i >= faders.size() || stripDim[i] == d) return;
        stripDim[i] = d; faders[i]->setAlpha (d ? 0.38f : 1.0f); repaint();
    }
    std::unique_ptr<LabeledKnob> fmKnob, ringKnob;
    juce::TextButton loadBtn { "LOAD VA..." };     // load a sound into the VA part only
    std::function<void()> onLoad;

    MixerPanel (juce::AudioProcessorValueTreeState& apvts)
    {
        loadBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0f6e56));
        loadBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeafff6));
        loadBtn.setTooltip ("Load a sound into the VA part only (FM, LA, MIXER, FX and ARP stay)");
        loadBtn.onClick = [this] { if (onLoad) onLoad(); };
        addAndMakeVisible (loadBtn);
        auto addFader = [&] (const juce::String& id, const juce::String& lab)
        {
            auto* f = new juce::Slider();
            f->setSliderStyle (juce::Slider::LinearVertical);
            f->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            faders.add (f); addAndMakeVisible (f);
            faderAtts.add (new SliderAtt (apvts, id, *f));
            labels.add (lab);
        };
        addFader (pid::oscLevel (1), "OSC 1"); addFader (pid::oscLevel (2), "OSC 2");
        addFader (pid::oscLevel (3), "OSC 3"); addFader (pid::oscLevel (4), "OSC 4");
        addFader (pid::subLevel, "SUB");       addFader (pid::noiseLevel, "NOISE");

        fmKnob   = std::make_unique<LabeledKnob> (apvts, pid::fmAmt,   "FM 2>1");  addAndMakeVisible (*fmKnob);
        ringKnob = std::make_unique<LabeledKnob> (apvts, pid::ringAmt, "RING 3x4"); addAndMakeVisible (*ringKnob);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (juce::Colour (0xff5dcaa5));
        g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        g.drawText ("LEVELS", 16, 10, 120, 16, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff9fe1cb));
        g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        for (int i = 0; i < labels.size(); ++i)
            if (auto* f = faders[i])
            {
                g.setColour (juce::Colour (0xff9fe1cb).withAlpha (stripDim[i] ? 0.38f : 1.0f));
                g.drawText (labels[i], f->getX() - 8, f->getBottom() + 2, f->getWidth() + 16, 14, juce::Justification::centred);
            }
    }

    void resized() override
    {
        loadBtn.setBounds (getWidth() - 14 - 100, 6, 100, 22);
        auto r = getLocalBounds().reduced (14);
        r.removeFromTop (24);
        auto right = r.removeFromRight (150);
        const int sw = r.getWidth() / 6;
        for (int i = 0; i < faders.size(); ++i)
            faders[i]->setBounds (r.getX() + i * sw + sw / 4, r.getY(), sw / 2, r.getHeight() - 18);

        auto rtop = right.removeFromTop (right.getHeight());
        fmKnob->setBounds   (rtop.removeFromLeft (rtop.getWidth() / 2).reduced (4));
        ringKnob->setBounds (rtop.reduced (4));
    }
};

// ---------------------------------------------------------------------------
//  Placeholder for tabs not yet implemented.
// ---------------------------------------------------------------------------
struct PlaceholderPanel : juce::Component
{
    juce::String text;
    explicit PlaceholderPanel (juce::String t) : text (std::move (t)) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 7.0f, 1.0f);
        g.setColour (juce::Colour (0xff6c7a74));
        g.setFont (juce::Font ("Segoe UI", 15.0f, juce::Font::bold));
        g.drawText (text, getLocalBounds(), juce::Justification::centred);
    }
};

// ---------------------------------------------------------------------------
//  MIDI activity indicator (polls the processor).
// ---------------------------------------------------------------------------
struct MidiLed : juce::Component
{
    float level = 0.0f;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff9fb0aa));
        g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::bold));
        g.drawText ("MIDI", r.removeFromLeft (34.0f), juce::Justification::centredRight);
        r.removeFromLeft (6.0f);
        auto dot = r.removeFromLeft (14.0f).withSizeKeepingCentre (12.0f, 12.0f);
        auto c = juce::Colour (0xff39ffb0);
        g.setColour (juce::Colour (0xff11161b)); g.fillEllipse (dot.expanded (1.0f));
        g.setColour (c.withAlpha (0.18f + 0.82f * level)); g.fillEllipse (dot);
        if (level > 0.5f) { g.setColour (c.withAlpha (0.4f * level)); g.fillEllipse (dot.expanded (3.0f * level)); }
    }
};

#include "FilterModTab.h"
#include "FxTab.h"
#include "MixerTab.h"
#include "LibraryOverlay.h"
#include "ArpTab.h"
#include "FmTab.h"
#include "LaTab.h"

// ===========================================================================
// Plain component whose paint / layout are supplied by the editor (scaled canvas).
struct Canvas : juce::Component
{
    std::function<void (juce::Graphics&)> onPaint;
    std::function<void()> onResized;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    void resized() override                 { if (onResized) onResized(); }
};

class CohesionSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit CohesionSynthAudioProcessorEditor (CohesionSynthAudioProcessor&);
    ~CohesionSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
public:
    void showTab (int t);          // public so the offline UI snapshot can switch tabs
    void openLibrary() { library->open(); }
    LibraryOverlay* getLibrary() { return library.get(); }
    void updateDimming();
    int  getCurrentTab() const { return currentTab; }
    void presetLoaded();           // after a preset was loaded: name + the tab of its engine
    void openSaveDialog();         // SAVE: name the current sound as a User preset
private:
    void updatePresetName();
    void saveAs (const juce::String& name, bool overwrite);

    CohesionSynthAudioProcessor& proc;
    CohesionLookAndFeel lnf;
    static constexpr int kW = 1040, kH = 720;
    Canvas canvas;
    cohesion::UpdateBadge updateBadge;   // UPDATE AVAILABLE (start-up version check)
    bool sizeRestored = false;
    void paintCanvas (juce::Graphics&);
    void layoutCanvas();

    static constexpr int kNumTabs = 7;   // OSC/MIX, FM, LA, FILTER/MOD, FX, ARP, MIXER
    juce::TextButton tabs[kNumTabs];
    int currentTab = 0;

    // OSC / MIX tab
    juce::Component oscMix;
    std::unique_ptr<OscPanel>  oscPanels[4];
    std::unique_ptr<MiniPanel> subPanel, noisePanel;
    std::unique_ptr<MixerPanel> mixer;

    std::unique_ptr<FmTab>            fmTab;
    std::unique_ptr<LaTab>            laTab;
    std::unique_ptr<FilterModTab>     filterTab;
    std::unique_ptr<FxTab>            fxTab;
    std::unique_ptr<ArpTab>           arpTab;
    std::unique_ptr<MixerTab>         mixerTab;

    MidiLed midiLed;
    std::unique_ptr<LabeledKnob> mainKnob, glideKnob, velKnob;
    juce::ComboBox voiceModeBox;
    std::unique_ptr<ComboAtt> voiceModeAtt;
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, libraryBtn { "LIBRARY" }, saveBtn { "SAVE" };
    std::unique_ptr<LibraryOverlay> library;
    struct ClickLabel : juce::Label   // the preset name: a click opens the LIBRARY
    {
        std::function<void()> onClick;
        void mouseUp (const juce::MouseEvent& e) override { if (onClick && e.mouseWasClicked()) onClick(); }
    };
    ClickLabel presetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionSynthAudioProcessorEditor)
};
