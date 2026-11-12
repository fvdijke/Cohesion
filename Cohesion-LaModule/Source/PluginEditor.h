#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

namespace ui
{
    const juce::Colour teal (0xff39ffb0), tealDark (0xff0f6e56), text (0xffdfe6e2), dim (0xff9fb0aa), label (0xff7f8f89),
                       panel (0xff11151a), panelEdge (0xff1f262d), boxBg (0xff0e1013);
    inline juce::Font font (float h, bool bold = false) { return juce::Font ("Segoe UI", h, bold ? juce::Font::bold : juce::Font::plain); }
}

// House style + compact pickers (small rounded box, teal chevron).
class ModuleLookAndFeel : public CohesionLookAndFeel
{
public:
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (ui::boxBg); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ui::teal.withAlpha (box.isMouseOver (true) ? 0.55f : 0.30f)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        const float cx = (float) w - 9.0f, cy = (float) h * 0.5f;
        juce::Path p; p.addTriangle (cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
        g.setColour (juce::Colour (0xff9fe1cb)); g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (11.0f); }
    juce::Font getTextButtonFont (juce::TextButton& b, int h) override
    {
        if (b.getProperties().contains ("fontSize")) return ui::font ((float) b.getProperties()["fontSize"]);
        return CohesionLookAndFeel::getTextButtonFont (b, h);
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (4, 0, box.getWidth() - 18, box.getHeight());
        l.setFont (ui::font (11.0f));
        l.setJustificationType (juce::Justification::centredLeft);
    }
};

struct Knob : juce::Component
{
    juce::Slider s; juce::String name; std::unique_ptr<SliderAtt> att; bool showValue;
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& nm, bool value = true)
        : name (nm), showValue (value)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (value ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, false, 60, 15);
        addAndMakeVisible (s);
        att = std::make_unique<SliderAtt> (apvts, id, s);
    }
    void resized() override { auto r = getLocalBounds(); if (name.isNotEmpty()) r.removeFromBottom (12); s.setBounds (r); }
    void parentHierarchyChanged() override { s.sendLookAndFeelChange(); }
    void paint (juce::Graphics& g) override
    {
        if (name.isEmpty()) return;
        g.setColour (ui::label); g.setFont (ui::font (9.5f));
        g.drawText (name, getLocalBounds().removeFromBottom (12), juce::Justification::centred);
    }
};

struct Led : juce::Component
{
    float level = 0.0f;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (8.0f, 8.0f);
        g.setColour (juce::Colour (0xff1d2a26)); g.fillEllipse (r);
        g.setColour (ui::teal.withAlpha (level)); g.fillEllipse (r);
    }
};

class LaModuleEditor;

// One MT-32 part: MIDI channel, patch, fader, pan, sends, mute / solo, output.
struct PartStrip : juce::Component
{
    PartStrip (LaModuleEditor& e, LaModuleProcessor& p, int index);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh (bool selected);

    LaModuleEditor& ed; LaModuleProcessor& proc; const int idx;
    Led led;
    juce::ComboBox chBox, outBox;
    std::unique_ptr<ComboAtt> chAtt, outAtt;
    juce::TextButton nameBtn;
    juce::Slider fader;
    std::unique_ptr<SliderAtt> faderAtt;
    std::unique_ptr<Knob> pan, rev, ech;
    juce::TextButton muteBtn { "M" }, soloBtn { "S" };
    std::unique_ptr<ButtonAtt> muteAtt, soloAtt;
    bool selected = false, dimmed = false;
};

struct Lcd : juce::Component
{
    juce::String line;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a2a22)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ui::tealDark); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (juce::Colour (0xff5dffc0));
        g.setFont (juce::Font ("Consolas", 21.0f, juce::Font::plain));
        g.drawText (line, r.reduced (12.0f, 0.0f), juce::Justification::centredLeft);
    }
};

struct Canvas : juce::Component
{
    std::function<void (juce::Graphics&)> onPaint;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
};

class LaModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LaModuleEditor (LaModuleProcessor&);
    ~LaModuleEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void selectPart (int p) { selected = p; refreshAll(); }
    int  selectedPart() const { return selected; }
    void showLibrary (int part);

private:
    void timerCallback() override;
    void paintCanvas (juce::Graphics&);
    void layout();
    void refreshAll();
    void refreshRomBox();

    LaModuleProcessor& proc;
    ModuleLookAndFeel lnf;
    static constexpr int kW = 1040, kH = 660;
    Canvas canvas;
    bool sizeRestored = false;
    int selected = 0;
    uint32_t lastVersion = 0;

    juce::TextButton engineBtn[2];
    juce::ComboBox romBox;
    Led midiLed;
    std::unique_ptr<Knob> master;

    Lcd lcd;
    float partialFrac = 0.0f; int partialCount = 0;
    juce::TextButton loadBtn { "LOAD .SYX" }, resetBtn { "RESET" }, panicBtn { "PANIC" }, libraryBtn { "LIBRARY" };
    juce::String status;

    std::unique_ptr<PartStrip> strips[mt::kParts];

    juce::TextButton rvModeBtn[4];
    std::unique_ptr<Knob> rvTime, rvLevel, ecDiv, ecTime, ecFb, ecLevel;
    juce::TextButton ecSyncBtn { "SYNC" };
    std::unique_ptr<ButtonAtt> ecSyncAtt;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaModuleEditor)
};
