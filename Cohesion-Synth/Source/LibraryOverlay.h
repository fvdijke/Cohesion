#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// ===========================================================================
//  Preset LIBRARY: overlay with categories (left) and presets (right).
//  Click = load, double-click = load + close, Esc / X / backdrop = close.
//  SEARCH filters the selected category by name and category (every word must match).
// ===========================================================================
struct LibraryOverlay : juce::Component
{
    CohesionSynthAudioProcessor& proc;
    std::function<void()> onPresetLoaded, onClose, onListChanged;
    std::function<void (int)> onPartLoaded;   // a preset was loaded into one part (pid::kVA / kFM / kLA)
    int targetPart = -1;                      // >= 0: the LIBRARY loads into that part only
    int allCount = 0;
    bool fits (int i) const { return targetPart < 0 || proc.presetFitsPart (i, targetPart); }
    bool isCurrent (int i) const
    {
        return targetPart < 0 ? i == proc.currentPresetIndex() : proc.partSoundName (targetPart) == proc.presetDisplayName (i);
    }

    juce::StringArray cats;          // "All" + categories in factory order
    std::vector<int>  visible;       // preset indices in the selected category
    int selCat = 0;
    std::unordered_map<juce::String, int> catCount;   // presets per category (filled in rebuildCategories)
    juce::TextEditor search;
    juce::String lastQuery;

    // ---------------------------------------------------------------- models
    struct CatModel : juce::ListBoxModel
    {
        LibraryOverlay& o; explicit CatModel (LibraryOverlay& ov) : o (ov) {}
        int getNumRows() override { return o.cats.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
        {
            o.paintRow (g, w, h, sel, o.cats[row], juce::String (o.countIn (row)), false);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent& e) override
        {
            o.selectCategory (row, false);
            if (e.mods.isPopupMenu()) o.showBankMenu (row);
        }
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
                        (o.selCat == 0 || o.search.getText().isNotEmpty()) ? o.proc.presetCategory (idx) : juce::String(),
                        o.isCurrent (idx));
        }
        void listBoxItemClicked (int row, const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { o.showUserPresetMenu (row); return; }
            o.loadRow (row, false);
        }
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { o.loadRow (row, true); }
        void returnKeyPressed (int row) override { o.loadRow (row, true); }
    };

    CatModel catModel { *this };
    PresetModel presetModel { *this };
    juce::ListBox catList { "categories", &catModel }, presetList { "presets", &presetModel };
    juce::TextButton closeBtn { "X" }, importFilesBtn { "IMPORT .SYX" }, importRomsBtn { "IMPORT ROMS" },
                     removeBtn { "REMOVE BANK" }, renameBtn { "RENAME" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;

    // Init, User (always shown), the factory categories (A-Z), then imported banks (A-Z)
    void rebuildCategories()
    {
        juce::StringArray factory, imported;
        catCount.clear();
        allCount = 0;
        for (int i = 0; i < proc.numPresets(); ++i)
        {
            if (! fits (i)) continue;
            ++allCount;
            const auto c = proc.presetCategory (i);
            if (catCount[c]++ == 0) (proc.presetIsImported (i) ? imported : factory).add (c);
        }
        factory.sortNatural(); imported.sortNatural();
        const juce::String user (CohesionSynthAudioProcessor::kUserCategory);
        factory.removeString (user);
        factory.insert (0, user);
        if (factory.contains ("Init")) { factory.removeString ("Init"); factory.insert (0, "Init"); }
        cats.clear();
        cats.add ("All");
        cats.addArray (factory);
        cats.addArray (imported);
        catList.updateContent();
    }

    // the last folder an import came from (per kind: "syx", "roms"), kept in Settings.xml
    static juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Settings"; o.filenameSuffix = ".xml";
        o.folderName = "Pepperwood/Cohesion Synth"; o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return o;
    }
    static juce::File lastImportDir (const char* kind)
    {
        juce::PropertiesFile f (settingsOptions());
        const juce::File d (f.getValue (juce::String ("importDir_") + kind));
        return d.isDirectory() ? d : juce::File();
    }
    static void rememberImportDir (const char* kind, const juce::File& d)
    {
        if (! d.isDirectory()) return;
        juce::PropertiesFile f (settingsOptions());
        f.setValue (juce::String ("importDir_") + kind, d.getFullPathName());
        f.saveIfNeeded();
    }

    void doImport()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import DX7 SysEx banks", lastImportDir ("syx"), "*.syx");
        const int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                        | juce::FileBrowserComponent::canSelectMultipleItems;
        chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto items = fc.getResults();
            if (items.isEmpty()) return;
            rememberImportDir ("syx", items.getFirst().getParentDirectory());
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

    bool isSysExCategory (int row) const
    {
        return row > 0 && cats[row].startsWith (juce::String::fromUTF8 ("DX7 \xc2\xb7 "));
    }
    bool isUserCategory (int row) const { return row > 0 && cats[row] == CohesionSynthAudioProcessor::kUserCategory; }

    // the User preset a button acts on: the selected row, else the current preset if it is in the list
    int targetUserPreset() const
    {
        const int row = presetList.getSelectedRow();
        if (row >= 0 && row < (int) visible.size() && proc.presetIsUser (visible[(size_t) row])) return visible[(size_t) row];
        const int cur = proc.currentPresetIndex();
        return (proc.presetIsUser (cur) && std::find (visible.begin(), visible.end(), cur) != visible.end()) ? cur : -1;
    }

    void showUserPresetMenu (int row)
    {
        if (row < 0 || row >= (int) visible.size() || ! proc.presetIsUser (visible[(size_t) row])) return;
        presetList.selectRow (row);
        const int idx = visible[(size_t) row];
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.addItem (2, "Delete");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetList), [this, idx] (int r)
        {
            if (r == 1) renameUserPreset (idx);
            if (r == 2) confirmDeleteUserPreset (idx);
        });
    }

    void userListChanged (int selectIndex)
    {
        rebuildCategories();
        selectCategory (juce::jmax (0, cats.indexOf (CohesionSynthAudioProcessor::kUserCategory)), true);
        for (size_t k = 0; k < visible.size(); ++k)
            if (visible[k] == selectIndex) { presetList.selectRow ((int) k); break; }
        if (onListChanged) onListChanged();
        repaint();
    }

    void renameUserPreset (int idx)
    {
        if (! proc.presetIsUser (idx)) return;
        auto* w = new juce::AlertWindow ("Rename preset", {}, juce::MessageBoxIconType::NoIcon, this);
        w->setLookAndFeel (&getLookAndFeel());
        w->addTextEditor ("name", proc.presetName (idx), "Name");
        w->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        if (auto* te = w->getTextEditor ("name")) { te->setInputRestrictions (64); te->selectAll(); }
        juce::Component::SafePointer<LibraryOverlay> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, idx] (int r)
        {
            if (r != 1 || safe == nullptr) return;
            const auto name = w->getTextEditorContents ("name");
            const int res = safe->proc.renameUserPreset (idx, name);
            if (res == CohesionSynthAudioProcessor::kSaveExists)  safe->status = "A User preset \"" + name.trim() + "\" already exists";
            else if (res == CohesionSynthAudioProcessor::kSaveBadName) safe->status = "Please enter a name";
            else if (res < 0)                                    safe->status = "The preset could not be renamed";
            else                                                 safe->status = "Renamed to \"" + safe->proc.presetName (res) + "\"";
            safe->userListChanged (res);
        }), true);
    }

    void confirmDeleteUserPreset (int idx)
    {
        if (! proc.presetIsUser (idx)) return;
        const auto name = proc.presetName (idx);
        juce::Component::SafePointer<LibraryOverlay> safe (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete preset",
            "Delete the User preset \"" + name + "\"?\n\nThe file goes to the recycle bin.", "Delete", "Cancel", this,
            juce::ModalCallbackFunction::create ([safe, idx, name] (int ok)
            {
                if (ok == 0 || safe == nullptr) return;
                safe->status = safe->proc.deleteUserPreset (idx) ? "Deleted \"" + name + "\" (in the recycle bin)" : "The preset could not be deleted";
                safe->userListChanged (-1);
            }));
    }

    void showBankMenu (int row)
    {
        juce::PopupMenu m;
        m.addItem (1, "Remove bank  " + cats[row].fromFirstOccurrenceOf (juce::String::fromUTF8 ("\xc2\xb7 "), false, false), isSysExCategory (row));
        m.addItem (2, "Remove all DX7 banks", std::any_of (cats.begin(), cats.end(), [] (const juce::String& c) { return c.startsWith ("DX7"); }));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&catList), [this, row] (int r)
        {
            if (r == 1) confirmRemove (cats[row]);
            if (r == 2) confirmRemove ({});
        });
    }

    // removes the bank's copy from the library folder; the original .syx files are not touched
    void confirmRemove (const juce::String& category)
    {
        const auto what = category.isEmpty() ? juce::String ("all imported DX7 banks")
                                             : "the bank \"" + category.fromFirstOccurrenceOf (juce::String::fromUTF8 ("\xc2\xb7 "), false, false) + "\"";
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Remove from the library",
            "Remove " + what + " from the LIBRARY?\n\nThe copy in the library folder is deleted; your original .syx files stay where they are. "
            "You can import them again at any time.",
            "Remove", "Cancel", this,
            juce::ModalCallbackFunction::create ([this, category] (int ok)
            {
                if (ok == 0) return;
                const int n = proc.removeSysExBank (category);
                status = "Removed " + juce::String (n) + " voice(s) from the library";
                rebuildCategories();
                selectCategory (0, true);
                if (onListChanged) onListChanged();   // the preset list (and its index) changed
                repaint();
            }));
    }

    void doImportRoms()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder with MT-32 / CM-32L ROM dumps (.zip files inside are scanned too)", lastImportDir ("roms"));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            const auto items = fc.getResults();
            if (items.isEmpty()) return;
            rememberImportDir ("roms", items.getFirst());
            const auto r = proc.importRoms (items);
            status = "ROM sets added: " + juce::String (r.sets)
                   + (r.duplicates > 0 ? "  |  " + juce::String (r.duplicates) + " duplicate version(s) skipped" : juce::String())
                   + (r.failed.size() > 0 ? "  |  not recognised: " + r.failed.joinIntoString (", ") : juce::String());
            if (r.sets == 0 && r.duplicates == 0 && r.failed.isEmpty()) status = "No MT-32 / CM-32L control + PCM ROM pair found in that folder";
            rebuildCategories();
            selectCategory (r.firstCategory.isNotEmpty() ? juce::jmax (0, cats.indexOf (r.firstCategory)) : selCat, false);
            catList.scrollToEnsureRowIsOnscreen (selCat);
            repaint();
        });
    }

    explicit LibraryOverlay (CohesionSynthAudioProcessor& p) : proc (p)
    {
        rebuildCategories();

        catList.setRowHeight (28);
        presetList.setRowHeight (30);
        for (auto* lb : { &catList, &presetList })
        {
            lb->setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0a0c0f));
            lb->setColour (juce::ListBox::outlineColourId, juce::Colour (0xff1c2127));
            lb->setOutlineThickness (1);
            addAndMakeVisible (*lb);
        }
        closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        closeBtn.onClick = [this] { close(); };
        addAndMakeVisible (closeBtn);
        for (auto* b : { &importFilesBtn, &importRomsBtn })
        {
            b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0f6e56));
            b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffeafff6));
            addAndMakeVisible (*b);
        }
        importFilesBtn.onClick  = [this] { doImport(); };
        importRomsBtn.onClick   = [this] { doImportRoms(); };
        removeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        removeBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fb0aa));
        removeBtn.onClick = [this]
        {
            if (isSysExCategory (selCat)) confirmRemove (cats[selCat]);
            else if (isUserCategory (selCat))
            {
                const int idx = targetUserPreset();
                if (idx >= 0) confirmDeleteUserPreset (idx); else { status = "Select a User preset first"; repaint(); }
            }
        };
        addChildComponent (removeBtn);
        renameBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013));
        renameBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff9fb0aa));
        renameBtn.onClick = [this]
        {
            const int idx = targetUserPreset();
            if (idx >= 0) renameUserPreset (idx); else { status = "Select a User preset first"; repaint(); }
        };
        addChildComponent (renameBtn);

        search.setTextToShowWhenEmpty ("Search presets...", juce::Colour (0xff6c7a74));
        search.setFont (juce::Font ("Segoe UI", 17.0f, juce::Font::plain));
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0a0c0f));
        search.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff2b3138));
        search.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff39ffb0));
        search.setColour (juce::TextEditor::textColourId, juce::Colour (0xffdfe6e2));
        search.setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff0f6e56));
        search.setColour (juce::CaretComponent::caretColourId, juce::Colour (0xff39ffb0));
        search.setIndents (10, 6);
        // a new search looks in every category; a category clicked during the search narrows it
        search.onTextChange = [this]
        {
            const bool starting = lastQuery.isEmpty() && search.getText().isNotEmpty();
            lastQuery = search.getText();
            selectCategory (starting ? 0 : selCat, true);
        };
        search.onEscapeKey  = [this] { if (search.getText().isNotEmpty()) { search.clear(); lastQuery.clear(); selectCategory (selCat, true); } else close(); };
        search.onReturnKey  = [this] { if (! visible.empty()) { presetList.selectRow (0); loadRow (0, false); } };
        addAndMakeVisible (search);
        setWantsKeyboardFocus (true);
    }

    int countIn (int catRow) const
    {
        if (catRow == 0) return allCount;
        const auto it = catCount.find (cats[catRow]);
        return it != catCount.end() ? it->second : 0;
    }

    void paintRow (juce::Graphics& g, int w, int h, bool sel, const juce::String& text, const juce::String& right, bool current)
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.0f, 2.0f);
        if (sel)          { g.setColour (juce::Colour (0xff0f6e56)); g.fillRoundedRectangle (r, 4.0f); }
        else if (current) { g.setColour (juce::Colour (0xff0f2a22)); g.fillRoundedRectangle (r, 4.0f); }
        if (current) { g.setColour (juce::Colour (0xff39ffb0)); g.fillEllipse (r.getX() + 8, r.getCentreY() - 4, 8, 8); }
        g.setColour (sel ? juce::Colour (0xffeafff6) : juce::Colour (0xffdfe6e2));
        g.setFont (juce::Font ("Segoe UI", 17.0f, juce::Font::plain));
        g.drawText (text, r.withTrimmedLeft (24).withTrimmedRight (90), juce::Justification::centredLeft);
        if (right.isNotEmpty())
        {
            g.setColour (sel ? juce::Colour (0xffbff5e2) : juce::Colour (0xff6c7a74));
            g.setFont (juce::Font ("Segoe UI", 13.5f, juce::Font::plain));
            g.drawText (right, r.withTrimmedRight (12), juce::Justification::centredRight);
        }
    }

    void selectCategory (int row, bool keepSelection)
    {
        selCat = juce::jlimit (0, cats.size() - 1, row);
        visible.clear();
        juce::StringArray words;
        words.addTokens (search.getText().toLowerCase(), " ", "\"");
        words.removeEmptyStrings();
        for (int i = 0; i < proc.numPresets(); ++i)
        {
            if (! fits (i)) continue;
            if (selCat != 0 && proc.presetCategory (i) != cats[selCat]) continue;
            if (! words.isEmpty())
            {
                const auto hay = (proc.presetName (i) + " " + proc.presetCategory (i)).toLowerCase();
                bool all = true;
                for (auto& w : words) if (! hay.contains (w)) { all = false; break; }
                if (! all) continue;
            }
            visible.push_back (i);
        }
        catList.selectRow (selCat, false, true);
        const bool user = isUserCategory (selCat);
        removeBtn.setButtonText (user ? "DELETE" : "REMOVE BANK");
        removeBtn.setVisible (isSysExCategory (selCat) || (user && ! visible.empty()));
        renameBtn.setVisible (user && ! visible.empty());
        layoutBottomButtons();
        presetList.updateContent();
        presetList.deselectAllRows();
        if (keepSelection)
            for (size_t k = 0; k < visible.size(); ++k)
                if (isCurrent (visible[k])) { presetList.selectRow ((int) k); break; }
        presetList.repaint();
        repaint();
    }

    void loadRow (int row, bool closeAfter)
    {
        if (row < 0 || row >= (int) visible.size()) return;
        if (targetPart >= 0)
        {
            proc.loadPresetIntoPart (visible[(size_t) row], targetPart);
            if (onPartLoaded) onPartLoaded (targetPart);
        }
        else
        {
            proc.loadPreset (visible[(size_t) row]);
            if (onPresetLoaded) onPresetLoaded();
        }
        presetList.repaint();
        if (closeAfter) close();
    }

    // part < 0: whole presets; otherwise only what fits that part, loaded into it alone
    void open (int part = -1)
    {
        targetPart = part;
        int cur = proc.currentPresetIndex();
        if (part >= 0)
        {
            cur = -1;
            for (int i = 0; i < proc.numPresets() && cur < 0; ++i) if (isCurrent (i)) cur = i;
        }
        rebuildCategories();
        const bool importOk = part != pid::kVA;
        importFilesBtn.setVisible (importOk && part != pid::kLA);
        importRomsBtn.setVisible (importOk && part != pid::kFM);
        int row = 0;
        if (cur >= 0) row = juce::jmax (0, cats.indexOf (proc.presetCategory (cur)));
        else if (part == pid::kFM || part == pid::kLA)   // nothing loaded yet: the first bank
            for (int c = 1; c < cats.size() && row == 0; ++c)
                if (cats[c] != CohesionSynthAudioProcessor::kUserCategory) row = c;
        selectCategory (row, true);
        setVisible (true);
        toFront (true);
        search.grabKeyboardFocus();
        search.selectAll();
    }

    void close() { setVisible (false); if (onClose) onClose(); }

    juce::Rectangle<int> panelBounds() const { return getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 40, 900), juce::jmin (getHeight() - 40, 600)); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        auto pb = panelBounds().toFloat();
        g.setColour (juce::Colour (0xff15181c)); g.fillRoundedRectangle (pb, 9.0f);
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.35f)); g.drawRoundedRectangle (pb.reduced (0.5f), 9.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font ("Segoe UI", 19.0f, juce::Font::bold));
        g.drawText ("LIBRARY", (int) pb.getX() + 22, (int) pb.getY() + 14, 240, 26, juce::Justification::centredLeft);
        if (targetPart >= 0)
        {
            g.setColour (juce::Colour (0xff39ffb0));
            g.setFont (juce::Font ("Segoe UI", 11.5f, juce::Font::bold));
            g.drawText ("INTO " + pid::partNames[targetPart] + " PART", (int) pb.getX() + 22, (int) pb.getY() + 38, 100, 14,
                        juce::Justification::centredLeft);
        }
        g.setColour (juce::Colour (0xff6c7a74));
        g.setFont (juce::Font ("Segoe UI", 13.0f, juce::Font::plain));
        g.drawText ("CATEGORY", catList.getX(), catList.getY() - 20, 160, 16, juce::Justification::centredLeft);
        g.drawText ((search.getText().isNotEmpty() ? "FOUND  (" : "PRESET  (") + juce::String ((int) visible.size()) + ")", presetList.getX(), presetList.getY() - 20, 240, 16, juce::Justification::centredLeft);
        if (status.isNotEmpty()) g.setColour (juce::Colour (0xff9fe1cb));
        const juce::String hint = targetPart >= 0
            ? "loads into the " + pid::partNames[targetPart] + " part only: the other parts, MIXER, FX and ARP stay   |   double-click = load + close   |   Esc = close"
            : juce::String ("type to search   |   click = load   |   double-click = load + close   |   right-click = remove / rename   |   Esc = close");
        g.drawText (status.isNotEmpty() ? status : hint,
                    (int) pb.getX() + 16, (int) pb.getBottom() - 28, (int) pb.getWidth() - 32, 18, juce::Justification::centred);
    }

    void resized() override
    {
        auto pb = panelBounds();
        closeBtn.setBounds (pb.getRight() - 44, pb.getY() + 12, 30, 30);
        importRomsBtn.setBounds (closeBtn.getX() - 10 - 120, pb.getY() + 12, 120, 30);
        importFilesBtn.setBounds (importRomsBtn.getX() - 8 - 120, pb.getY() + 12, 120, 30);
        search.setBounds (pb.getX() + 130, pb.getY() + 12, importFilesBtn.getX() - 14 - (pb.getX() + 130), 30);
        auto body = pb.reduced (22).withTrimmedTop (52).withTrimmedBottom (26);
        auto left = body.removeFromLeft (280);
        bottomRow = left.removeFromBottom (28);
        layoutBottomButtons();
        left.removeFromBottom (8);
        catList.setBounds (left);
        body.removeFromLeft (14);
        presetList.setBounds (body);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! (isUserCategory (selCat) && visible.empty() && search.getText().isEmpty())) return;
        g.setColour (juce::Colour (0xff9fb0aa));
        g.setFont (juce::Font ("Segoe UI", 16.0f, juce::Font::plain));
        g.drawFittedText ("No User presets yet.\n\nPress SAVE in the header to store the current sound here.",
                          presetList.getBounds().reduced (30), juce::Justification::centred, 4);
    }

    juce::Rectangle<int> bottomRow;
    void layoutBottomButtons()
    {
        auto r = bottomRow;
        if (renameBtn.isVisible()) { renameBtn.setBounds (r.removeFromLeft (r.getWidth() / 2 - 4)); r.removeFromLeft (8); }
        removeBtn.setBounds (r);
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
