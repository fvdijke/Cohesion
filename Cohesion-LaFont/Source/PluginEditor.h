#pragma once

#include "PluginProcessor.h"
#include "CohesionUpdateCheck.h"
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

class FontLookAndFeel : public CohesionLookAndFeel
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
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return ui::font (b.getHeight() > 24 ? 13.0f : 11.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (4, 0, box.getWidth() - 18, box.getHeight());
        l.setFont (getComboBoxFont (box));
        l.setJustificationType (juce::Justification::centredLeft);
    }
};

struct Knob : juce::Component
{
    juce::Slider s; juce::String name; std::unique_ptr<SliderAtt> att;
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& nm, bool value = true) : name (nm)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (value ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, false, 70, 15);
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

struct TitleToggle : juce::Button
{
    explicit TitleToggle (const juce::String& t) : juce::Button (t) { setClickingTogglesState (true); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat();
        auto dot = r.removeFromLeft (12.0f).withSizeKeepingCentre (7.0f, 7.0f);
        g.setColour (on ? ui::teal : juce::Colour (0xff2b3138)); g.fillEllipse (dot);
        g.setColour (on ? ui::teal : ui::label.withAlpha (over ? 0.9f : 0.6f));
        g.setFont (ui::font (10.5f, true));
        g.drawText (getButtonText(), r.withTrimmedLeft (3.0f), juce::Justification::centredLeft);
    }
};

struct HelpButton : juce::Button
{
    HelpButton() : juce::Button ("?") { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTooltip ("Help, credits and disclaimer"); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (down ? ui::tealDark : ui::boxBg); g.fillEllipse (r);
        g.setColour (ui::teal.withAlpha (over ? 0.9f : 0.5f)); g.drawEllipse (r, 1.2f);
        g.setColour (over ? ui::teal : ui::text); g.setFont (ui::font (13.0f, true));
        g.drawText ("?", r, juce::Justification::centred);
    }
};

struct HelpOverlay : juce::Component
{
    std::function<void()> onClose;
    void mouseDown (const juce::MouseEvent&) override { if (onClose) onClose(); }
    void paint (juce::Graphics& g) override;
};

// clickable two-line name box
struct NameBox : juce::Component
{
    juce::String text; std::function<void()> onClick; float alpha = 1.0f;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (ui::teal.withAlpha ((isMouseOver() ? 0.6f : 0.3f) * alpha)); g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (ui::text.withAlpha (alpha)); g.setFont (ui::font (11.5f));
        g.drawFittedText (text, getLocalBounds().reduced (4, 2), juce::Justification::centred, 2, 0.9f);
    }
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
};

class LaFontEditor;

struct PartStrip : juce::Component
{
    PartStrip (LaFontEditor& e, LaFontProcessor& p, int index);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh (bool selected);

    LaFontEditor& ed; LaFontProcessor& proc; const int idx;
    Led led;
    juce::ComboBox chBox, outBox;
    std::unique_ptr<ComboAtt> chAtt, outAtt;
    NameBox name;
    juce::Slider fader;
    std::unique_ptr<SliderAtt> faderAtt;
    std::unique_ptr<Knob> pan;
    juce::TextButton muteBtn { "M" }, soloBtn { "S" };
    std::unique_ptr<ButtonAtt> muteAtt, soloAtt;
    bool selected = false, dimmed = false, rvOn = true, choOn = true;
    int voices = 0;
    juce::String tag;
    juce::Rectangle<int> infoArea, fxArea;
};

struct Lcd : juce::Component
{
    juce::String line1, line2;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a2a22)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ui::tealDark); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        auto t = r.reduced (14.0f, 6.0f);
        g.setColour (juce::Colour (0xff5dffc0)); g.setFont (juce::Font ("Consolas", 22.0f, juce::Font::plain));
        g.drawText (line1, t.removeFromTop (30.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff3fae86)); g.setFont (juce::Font ("Consolas", 13.0f, juce::Font::plain));
        g.drawText (line2, t, juce::Justification::centredLeft);
    }
};

struct Canvas : juce::Component
{
    std::function<void (juce::Graphics&)> onPaint;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
};

class LaFontEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LaFontEditor (LaFontProcessor&);
    ~LaFontEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void selectPart (int p);
    void showPresetMenu (int part);
    void showHelp() { help.setVisible (true); help.toFront (false); }

private:
    void timerCallback() override;
    void paintCanvas (juce::Graphics&);
    void layout();
    void bindFx();
    void refreshAll();
    void refreshFontBox();
    void showFontMenu();
    void addFonts (bool folder);

    LaFontProcessor& proc;
    FontLookAndFeel lnf;
    static constexpr int kW = 1440, kH = 720;
    Canvas canvas;
    cohesion::UpdateBadge updateBadge;   // UPDATE AVAILABLE (start-up version check)
    bool sizeRestored = false;
    int selected = 0;

    juce::TextButton fontBtn { "SOUNDFONT" };
    Led midiLed;
    std::unique_ptr<Knob> master;
    HelpButton helpBtn;
    HelpOverlay help;

    Lcd lcd;
    int totalVoices = 0;
    float voiceShare[LaFontProcessor::kParts] {};
    juce::TextButton resetBtn { "GM RESET" }, panicBtn { "PANIC" };
    juce::String status;

    std::unique_ptr<PartStrip> strips[LaFontProcessor::kParts];

    // SOUND / REVERB / CHORUS of the selected part
    juce::ComboBox fontBox;
    juce::StringArray fontItems;
    juce::TextButton presetBtn, prevBtn { "<" }, nextBtn { ">" };
    TitleToggle rvOnBtn { "REVERB" }, choOnBtn { "CHORUS" };
    std::unique_ptr<ButtonAtt> rvOnAtt, choOnAtt;
    std::unique_ptr<Knob> rvRoom, rvDamp, rvWidth, rvLevel, choDepth, choRate, choLevel, choVoices;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaFontEditor)
};
