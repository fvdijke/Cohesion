#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// ===========================================================================
//  Preset LIBRARY: overlay with categories (left) and presets (right).
//  Click = load, double-click = load + close, Esc / X / backdrop = close.
// ===========================================================================
struct LibraryOverlay : juce::Component
{
    CohesionSynthAudioProcessor& proc;
    std::function<void()> onPresetLoaded, onClose;

    juce::StringArray cats;          // "All" + categories in factory order
    std::vector<int>  visible;       // preset indices in the selected category
    int selCat = 0;

    // ---------------------------------------------------------------- models
    struct CatModel : juce::ListBoxModel
    {
        LibraryOverlay& o; explicit CatModel (LibraryOverlay& ov) : o (ov) {}
        int getNumRows() override { return o.cats.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
        {
            o.paintRow (g, w, h, sel, o.cats[row], juce::String (o.countIn (row)), false);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override { o.selectCategory (row, false); }
    };
    struct PresetModel : juce::ListBoxModel
    {
        LibraryOverlay& o; explicit PresetModel (LibraryOverlay& ov) : o (ov) {}
        int getNumRows() override { return (int) o.visible.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
        {
            if (row < 0 || row >= (int) o.visible.size()) return;
            const int idx = o.visible[(size_t) row];
            o.paintRow (g, w, h, sel, o.proc.presetName (idx),
                        o.selCat == 0 ? o.proc.presetCategory (idx) : juce::String(), idx == o.proc.currentPresetIndex());
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override { o.loadRow (row, false); }
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { o.loadRow (row, true); }
        void returnKeyPressed (int row) override { o.loadRow (row, true); }
    };

    CatModel catModel { *this };
    PresetModel presetModel { *this };
    juce::ListBox catList { "categories", &catModel }, presetList { "presets", &presetModel };
    juce::TextButton closeBtn { "X" };

    explicit LibraryOverlay (CohesionSynthAudioProcessor& p) : proc (p)
    {
        juce::StringArray found;
        for (int i = 0; i < proc.numPresets(); ++i) found.addIfNotAlreadyThere (proc.presetCategory (i));
        found.sortNatural();
        if (found.contains ("Init")) { found.removeString ("Init"); found.insert (0, "Init"); }
        cats.add ("All");
        cats.addArray (found);

        for (auto* lb : { &catList, &presetList })
        {
            lb->setRowHeight (34);
            lb->setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0a0c0f));
            lb->setColour (juce::ListBox::outlineColourId, juce::Colour (0xff1c2127));
            lb->setOutlineThickness (1);
            addAndMakeVisible (*lb);
        }
        closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        closeBtn.onClick = [this] { close(); };
        addAndMakeVisible (closeBtn);
        setWantsKeyboardFocus (true);
    }

    int countIn (int catRow) const
    {
        if (catRow == 0) return proc.numPresets();
        int c = 0; for (int i = 0; i < proc.numPresets(); ++i) if (proc.presetCategory (i) == cats[catRow]) ++c;
        return c;
    }

    void paintRow (juce::Graphics& g, int w, int h, bool sel, const juce::String& text, const juce::String& right, bool current)
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.0f, 2.0f);
        if (sel)          { g.setColour (juce::Colour (0xff0f6e56)); g.fillRoundedRectangle (r, 4.0f); }
        else if (current) { g.setColour (juce::Colour (0xff0f2a22)); g.fillRoundedRectangle (r, 4.0f); }
        if (current) { g.setColour (juce::Colour (0xff39ffb0)); g.fillEllipse (r.getX() + 8, r.getCentreY() - 4, 8, 8); }
        g.setColour (sel ? juce::Colour (0xffeafff6) : juce::Colour (0xffdfe6e2));
        g.setFont (juce::Font ("Segoe UI", 15.0f, juce::Font::plain));
        g.drawText (text, r.withTrimmedLeft (24).withTrimmedRight (90), juce::Justification::centredLeft);
        if (right.isNotEmpty())
        {
            g.setColour (sel ? juce::Colour (0xffbff5e2) : juce::Colour (0xff6c7a74));
            g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::plain));
            g.drawText (right, r.withTrimmedRight (12), juce::Justification::centredRight);
        }
    }

    void selectCategory (int row, bool keepSelection)
    {
        selCat = juce::jlimit (0, cats.size() - 1, row);
        visible.clear();
        for (int i = 0; i < proc.numPresets(); ++i)
            if (selCat == 0 || proc.presetCategory (i) == cats[selCat]) visible.push_back (i);
        catList.selectRow (selCat, false, true);
        presetList.updateContent();
        presetList.deselectAllRows();
        if (keepSelection)
            for (size_t k = 0; k < visible.size(); ++k)
                if (visible[k] == proc.currentPresetIndex()) { presetList.selectRow ((int) k); break; }
        presetList.repaint();
        repaint();
    }

    void loadRow (int row, bool closeAfter)
    {
        if (row < 0 || row >= (int) visible.size()) return;
        proc.loadPreset (visible[(size_t) row]);
        if (onPresetLoaded) onPresetLoaded();
        presetList.repaint();
        if (closeAfter) close();
    }

    void open()
    {
        const int cur = proc.currentPresetIndex();
        const int row = juce::jmax (0, cats.indexOf (proc.presetCategory (cur)));
        catList.updateContent();
        selectCategory (row, true);
        setVisible (true);
        toFront (true);
        grabKeyboardFocus();
    }

    void close() { setVisible (false); if (onClose) onClose(); }

    juce::Rectangle<int> panelBounds() const { return getLocalBounds().withSizeKeepingCentre (800, 560); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        auto pb = panelBounds().toFloat();
        g.setColour (juce::Colour (0xff15181c)); g.fillRoundedRectangle (pb, 9.0f);
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.35f)); g.drawRoundedRectangle (pb.reduced (0.5f), 9.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font ("Segoe UI", 19.0f, juce::Font::bold));
        g.drawText ("LIBRARY", (int) pb.getX() + 22, (int) pb.getY() + 14, 240, 26, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff6c7a74));
        g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::plain));
        g.drawText ("CATEGORY", catList.getX(), catList.getY() - 20, 160, 16, juce::Justification::centredLeft);
        g.drawText ("PRESET  (" + juce::String ((int) visible.size()) + ")", presetList.getX(), presetList.getY() - 20, 240, 16, juce::Justification::centredLeft);
        g.drawText ("click = load   |   double-click = load + close   |   Esc = close",
                    (int) pb.getX(), (int) pb.getBottom() - 28, (int) pb.getWidth(), 18, juce::Justification::centred);
    }

    void resized() override
    {
        auto pb = panelBounds();
        closeBtn.setBounds (pb.getRight() - 44, pb.getY() + 12, 30, 30);
        auto body = pb.reduced (22).withTrimmedTop (52).withTrimmedBottom (26);
        catList.setBounds (body.removeFromLeft (240));
        body.removeFromLeft (14);
        presetList.setBounds (body);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! panelBounds().contains (e.getPosition())) close();   // click on the backdrop
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { close(); return true; }
        return false;
    }
};
