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
    juce::TextButton closeBtn { "X" }, importFilesBtn { "IMPORT .SYX" }, importFolderBtn { "IMPORT FOLDER" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;

    // factory categories (Init first, then A-Z), then imported DX7 banks (A-Z)
    void rebuildCategories()
    {
        juce::StringArray factory, imported;
        for (int i = 0; i < proc.numPresets(); ++i)
        {
            const auto c = proc.presetCategory (i);
            (proc.presetIsImported (i) ? imported : factory).addIfNotAlreadyThere (c);
        }
        factory.sortNatural(); imported.sortNatural();
        if (factory.contains ("Init")) { factory.removeString ("Init"); factory.insert (0, "Init"); }
        cats.clear();
        cats.add ("All");
        cats.addArray (factory);
        cats.addArray (imported);
        catList.updateContent();
    }

    void doImport (bool folder)
    {
        chooser = std::make_unique<juce::FileChooser> (folder ? "Import a folder with DX7 SysEx banks" : "Import DX7 SysEx banks",
                                                       juce::File(), folder ? juce::String() : juce::String ("*.syx"));
        const int flags = juce::FileBrowserComponent::openMode
                        | (folder ? juce::FileBrowserComponent::canSelectDirectories
                                  : juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems);
        chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto items = fc.getResults();
            if (items.isEmpty()) return;
            const auto r = proc.importSysEx (items);
            status = "Imported " + juce::String (r.voices) + " voices from " + juce::String (r.files) + " file(s)"
                   + (r.duplicates > 0 ? "  |  " + juce::String (r.duplicates) + " duplicates skipped" : juce::String())
                   + (r.failed.size() > 0 ? "  |  not readable: " + r.failed.joinIntoString (", ") : juce::String());
            rebuildCategories();
            selectCategory (r.firstCategory.isNotEmpty() ? juce::jmax (0, cats.indexOf (r.firstCategory)) : selCat, false);
            catList.scrollToEnsureRowIsOnscreen (selCat);
            repaint();
        });
    }

    explicit LibraryOverlay (CohesionSynthAudioProcessor& p) : proc (p)
    {
        rebuildCategories();

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
        for (auto* b : { &importFilesBtn, &importFolderBtn })
        {
            b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0f6e56));
            b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeafff6));
            addAndMakeVisible (*b);
        }
        importFilesBtn.onClick  = [this] { doImport (false); };
        importFolderBtn.onClick = [this] { doImport (true); };
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
        rebuildCategories();
        const int row = juce::jmax (0, cats.indexOf (proc.presetCategory (cur)));
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
        if (status.isNotEmpty()) g.setColour (juce::Colour (0xff9fe1cb));
        g.drawText (status.isNotEmpty() ? status : juce::String ("click = load   |   double-click = load + close   |   Esc = close"),
                    (int) pb.getX() + 16, (int) pb.getBottom() - 28, (int) pb.getWidth() - 32, 18, juce::Justification::centred);
    }

    void resized() override
    {
        auto pb = panelBounds();
        closeBtn.setBounds (pb.getRight() - 44, pb.getY() + 12, 30, 30);
        importFolderBtn.setBounds (closeBtn.getX() - 10 - 130, pb.getY() + 12, 130, 30);
        importFilesBtn.setBounds (importFolderBtn.getX() - 8 - 120, pb.getY() + 12, 120, 30);
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
