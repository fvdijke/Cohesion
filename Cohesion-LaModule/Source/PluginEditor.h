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

// Panel title that is also its on / off switch: dimmed when off, green when on.
struct TitleToggle : juce::Button
{
    explicit TitleToggle (const juce::String& t) : juce::Button (t)
    {
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
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

// Round (?) button in the top-right corner.
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

// Help / credits / disclaimer, drawn over the whole canvas; click anywhere to close.
struct HelpOverlay : juce::Component
{
    std::function<void()> onClose;
    void mouseDown (const juce::MouseEvent&) override { if (onClose) onClose(); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.6f));
        auto panelR = getLocalBounds().toFloat().withSizeKeepingCentre (760.0f, 500.0f);
        g.setColour (ui::panel); g.fillRoundedRectangle (panelR, 10.0f);
        g.setColour (ui::teal.withAlpha (0.45f)); g.drawRoundedRectangle (panelR.reduced (0.5f), 10.0f, 1.0f);

        auto r = panelR.reduced (30.0f, 22.0f);
        g.setColour (juce::Colours::white); g.setFont (ui::font (17.0f, true).withExtraKerningFactor (0.12f));
        g.drawText ("COHESION - LA MODULE", r.removeFromTop (26.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.55f)); g.setFont (ui::font (11.0f));
        g.drawText (juce::String::fromUTF8 ("MT32 Emulator  \xc2\xb7  by Pepperwood  \xc2\xb7  click anywhere to close"), r.removeFromTop (18.0f), juce::Justification::centredLeft);
        r.removeFromTop (10.0f);

        juce::AttributedString a;
        a.setWordWrap (juce::AttributedString::byWord);
        a.setLineSpacing (2.0f);
        auto head = [&] (const char* t) { a.append (juce::String::fromUTF8 (t) + "\n", ui::font (15.0f, true), ui::teal); };
        auto body = [&] (const char* t) { a.append (juce::String::fromUTF8 (t) + "\n\n", ui::font (13.5f), ui::text); };
        head ("Getting started");
        body ("1. Click ROM and choose the folder with your MT-32 / CM-32L ROM files (control + PCM ROM; .zip files in the folder are scanned too). "
              "Choose the ROM set in the picker next to it.\n"
              "2. Parts 1\xe2\x80\x93" "8 listen on MIDI channels 2\xe2\x80\x93" "9, the rhythm part on channel 10 (change this per part with its CH picker).\n"
              "3. Click a part's name to choose a timbre. LOAD .SYX sends an MT-32 SysEx file, e.g. the setup of a game or song.");
        head ("Parts, reverb and echo");
        body ("Each strip has a fader, pan, mute / solo and an output: MAIN or the part's own output (OUT 2\xe2\x80\x93" "10; enable the extra outputs in your DAW). "
              "Click a strip to select it: the LCD and the REVERB / ECHO boxes then show that part. Reverb and echo are set per part; "
              "click a box's title to switch it on or off. An MT-32 reverb SysEx sets the reverb of every part. "
              "RESET power-cycles the MT-32: the ROM set is reloaded and every part returns to its power-on timbre. PANIC stops all notes.");
        head ("Credits");
        body ("Sound engine: Munt / mt32emu \xc2\xa9 the Munt contributors, licensed under the GNU Lesser General Public License 2.1 or later "
              "(github.com/munt/munt). It is not part of this plugin's code: it is included as the separate library mt32emu.dll next to the plugin, "
              "with its licence text (mt32emu-COPYING.LESSER.txt), and may be replaced by your own build of Munt.\n"
              "Built with JUCE.  COHESION - LA MODULE \xc2\xa9 Pepperwood. All rights reserved.");
        head ("Disclaimer");
        body ("Roland, MT-32, CM-32L and LA synthesis are trademarks or registered trademarks of Roland Corporation. "
              "This product is not affiliated with, endorsed or sponsored by Roland Corporation; these names are only used to describe compatibility. "
              "No Roland ROM data is included: the module plays ROM images that you provide yourself and are entitled to use. "
              "This software is provided \"as is\", without any warranty.");
        a.draw (g, r);
    }
};

class LaModuleEditor;

// One MT-32 part: MIDI channel, patch, fader, pan, mute / solo, output.
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
    std::unique_ptr<Knob> pan;
    juce::TextButton muteBtn { "M" }, soloBtn { "S" };
    std::unique_ptr<ButtonAtt> muteAtt, soloAtt;
    bool selected = false, dimmed = false, rvOn = true, ecOn = false;
    int partials = 0;
    juce::Rectangle<int> partialsArea;
    juce::Rectangle<int> fxArea;
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

    void selectPart (int p);
    int  selectedPart() const { return selected; }
    void showLibrary (int part);
    void showHelp() { help.setVisible (true); help.toFront (false); }

private:
    void timerCallback() override;
    void paintCanvas (juce::Graphics&);
    void layout();
    void layoutFx();
    void bindFx();                  // attach the REVERB / ECHO boxes to the selected part
    void refreshAll();
    void refreshRomBox();
    void readRomFolder();

    LaModuleProcessor& proc;
    ModuleLookAndFeel lnf;
    static constexpr int kW = 1040, kH = 660;
    Canvas canvas;
    bool sizeRestored = false;
    int selected = 0;

    juce::TextButton romBtn { "ROM" };
    juce::ComboBox romBox;
    Led midiLed;
    std::unique_ptr<Knob> master;
    HelpButton helpBtn;
    HelpOverlay help;

    Lcd lcd;
    int partialCount = 0;
    float partialShare[mt::kParts] {};          // smoothed per-part share of the bar
    juce::TextButton loadBtn { "LOAD .SYX" }, resetBtn { "RESET" }, panicBtn { "PANIC" };
    juce::String status;

    std::unique_ptr<PartStrip> strips[mt::kParts];

    // REVERB / ECHO of the selected part
    TitleToggle rvOnBtn { "REVERB" }, ecOnBtn { "ECHO" };
    juce::TextButton rvModeBtn[4];
    juce::TextButton ecSyncBtn { "SYNC" };
    std::unique_ptr<ButtonAtt> rvOnAtt, ecOnAtt, ecSyncAtt;
    std::unique_ptr<Knob> rvTime, rvLevel, ecDiv, ecTime, ecFb, ecLevel;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaModuleEditor)
};
