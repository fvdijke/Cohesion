#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "LookAndFeel.h"
#include "AlgoDiagram.h"
#include "VoiceIndex.h"
#include "PreviewSynth.h"
#include "CohesionUpdateCheck.h"

namespace ui
{
    static const juce::Colour bg { 0xff15181c }, panel { 0xff0d0f12 }, list { 0xff0a0c0f }, line { 0xff1c2127 },
                              text { 0xffdfe6e2 }, dim { 0xff6c7a74 }, mid { 0xff9fb0aa }, teal { 0xff39ffb0 },
                              tealDark { 0xff0f6e56 }, tealText { 0xff9fe1cb }, light { 0xffeafff6 };
    inline juce::Font font (float h, bool bold = false) { return juce::Font ("Segoe UI", h, bold ? juce::Font::bold : juce::Font::plain); }
    inline juce::Font mono (float h) { return juce::Font ("Consolas", h, juce::Font::plain); }

    inline juce::String noteName (int transpose)   // DX7 transpose: 24 = C3
    {
        static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (n[transpose % 12]) + juce::String (transpose / 12 + 1);
    }

    inline juce::String midiName (int n)          // Yamaha octave numbering: 60 = C3
    {
        static const char* nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (nm[((n % 12) + 12) % 12]) + juce::String (n / 12 - 2);
    }

    inline void setName (dx7::Patch& p, const juce::String& s)
    {
        std::memset (p.name, 0, sizeof (p.name));
        for (int i = 0; i < 10; ++i)
        {
            const juce::juce_wchar c = i < s.length() ? s[i] : ' ';
            p.name[i] = (c >= 32 && c < 127) ? (char) c : ' ';
        }
    }
}

// ===========================================================================
//  The 32 slots of the bank being built.
// ===========================================================================
struct BankSlot { bool used = false; dx7::Patch patch; juce::String source; };

struct SlotGrid : juce::Component, juce::DragAndDropTarget
{
    std::array<BankSlot, 32>& slots;
    int selected = 0, hover = -1, hoverCount = 1;
    std::function<void (int)> onSelect, onRename, onClear, onPlay;
    std::function<void (const juce::String&, int)> onDrop;
    std::function<void (int, int)> onSwap;

    explicit SlotGrid (std::array<BankSlot, 32>& s) : slots (s) { setWantsKeyboardFocus (true); }

    static constexpr int cols = 4, rows = 8;            // DX7 cartridge order: 1-8 down the first column
    juce::Rectangle<int> cell (int i) const
    {
        const float w = getWidth() / (float) cols, h = getHeight() / (float) rows;
        const int c = i / rows, r = i % rows;
        return juce::Rectangle<float> (c * w, r * h, w, h).reduced (3.0f, 2.5f).toNearestInt();
    }
    int indexAt (juce::Point<int> p) const
    {
        for (int i = 0; i < 32; ++i) if (cell (i).expanded (3, 3).contains (p)) return i;
        return -1;
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < 32; ++i)
        {
            auto r = cell (i).toFloat();
            const auto& s = slots[(size_t) i];
            const bool sel = i == selected;
            const bool target = hover >= 0 && i >= hover && i < hover + hoverCount;
            g.setColour (sel ? ui::tealDark : (s.used ? juce::Colour (0xff12161a) : ui::list));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (target ? ui::teal : (sel ? ui::teal.withAlpha (0.8f) : ui::line));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, target ? 2.0f : 1.0f);

            auto inner = r.reduced (8.0f, 4.0f);
            g.setColour (sel ? ui::light.withAlpha (0.7f) : ui::dim);
            g.setFont (ui::font (11.0f, true));
            g.drawText (juce::String (i + 1), inner.removeFromLeft (22.0f), juce::Justification::centredLeft);
            if (s.used)
            {
                g.setColour (sel ? ui::light : ui::text);
                g.setFont (ui::mono (15.0f));
                g.drawText (s.patch.getName(), inner.removeFromTop (inner.getHeight() * 0.58f), juce::Justification::bottomLeft);
                g.setColour (sel ? ui::light.withAlpha (0.65f) : ui::dim);
                g.setFont (ui::font (10.5f));
                g.drawText (s.source, inner, juce::Justification::topLeft, true);
            }
            else
            {
                g.setColour (juce::Colour (0xff3a4149));
                g.setFont (ui::font (11.5f).italicised());
                g.drawText ("empty  (INIT VOICE)", inner, juce::Justification::centredLeft);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.getPosition());
        if (i < 0) return;
        grabKeyboardFocus();
        if (onSelect) onSelect (i);
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "Play", slots[(size_t) i].used);
            m.addItem (2, "Rename...", slots[(size_t) i].used);
            m.addItem (3, "Clear slot", slots[(size_t) i].used);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, i] (int r)
            {
                if (r == 1 && onPlay) onPlay (i);
                if (r == 2 && onRename) onRename (i);
                if (r == 3 && onClear) onClear (i);
            });
        }
        else if (onPlay && slots[(size_t) i].used && e.getNumberOfClicks() == 1) onPlay (i);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.getPosition());
        if (i >= 0 && slots[(size_t) i].used && onRename) onRename (i);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.getMouseDownPosition());
        if (i < 0 || ! slots[(size_t) i].used || e.getDistanceFromDragStart() < 6) return;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive())
                c->startDragging ("slot:" + juce::String (i), this);
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && onClear) { onClear (selected); return true; }
        if (k == juce::KeyPress::F2Key && onRename) { onRename (selected); return true; }
        int d = 0;
        if (k == juce::KeyPress::upKey) d = -1; else if (k == juce::KeyPress::downKey) d = 1;
        else if (k == juce::KeyPress::leftKey) d = -rows; else if (k == juce::KeyPress::rightKey) d = rows;
        if (d != 0 && onSelect)
        {
            const int n = juce::jlimit (0, 31, selected + d);
            onSelect (n);
            if (onPlay && slots[(size_t) n].used) onPlay (n);
            return true;
        }
        return false;
    }

    // drag & drop: "rows:1,2,3" from the voice list or "slot:n" from this grid
    static int countOf (const juce::String& d) { return d.startsWith ("rows:") ? juce::StringArray::fromTokens (d.substring (5), ",", "").size() : 1; }
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        const auto s = d.description.toString();
        return s.startsWith ("rows:") || s.startsWith ("slot:");
    }
    void itemDragMove (const SourceDetails& d) override
    {
        const int i = indexAt (d.localPosition);
        const int n = countOf (d.description.toString());
        if (i != hover || n != hoverCount) { hover = i; hoverCount = n; repaint(); }
    }
    void itemDragExit (const SourceDetails&) override { hover = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int i = indexAt (d.localPosition);
        hover = -1; repaint();
        if (i < 0) return;
        const auto s = d.description.toString();
        if (s.startsWith ("slot:")) { if (onSwap) onSwap (s.substring (5).getIntValue(), i); }
        else if (onDrop) onDrop (s, i);
    }
};

// ===========================================================================
//  Details of the voice being previewed.
// ===========================================================================
struct VoiceDetails : juce::Component
{
    AlgoDiagram diagram;
    dx7::Patch patch; bool has = false;
    juce::String title, origin;

    VoiceDetails() { addAndMakeVisible (diagram); }

    void show (const dx7::Patch& p, const juce::String& where)
    {
        patch = p; has = true; title = p.getName(); origin = where;
        diagram.alg = p.alg; diagram.repaint(); repaint();
    }

    void resized() override { diagram.setBounds (getLocalBounds().reduced (10).removeFromLeft (210)); }

    void paint (juce::Graphics& g) override
    {
        fmui::card (g, getLocalBounds().toFloat());
        auto r = getLocalBounds().reduced (10).withTrimmedLeft (222);
        if (! has)
        {
            g.setColour (ui::dim); g.setFont (ui::font (13.0f));
            g.drawText ("Click a voice to hear it and see its parameters.", r, juce::Justification::centredLeft);
            return;
        }
        static const char* waves[6] = { "TRI", "SAW DN", "SAW UP", "SQUARE", "SINE", "S&H" };
        auto head = r.removeFromTop (22);
        g.setColour (ui::light); g.setFont (ui::mono (18.0f));
        g.drawText (title, head.removeFromLeft (130), juce::Justification::centredLeft);
        g.setColour (ui::dim); g.setFont (ui::font (12.0f));
        g.drawText (origin, head, juce::Justification::centredLeft, true);

        const auto& p = patch;
        juce::String info;
        info << "ALG " << (p.alg + 1) << "   FB " << (int) p.fb << "   KEY SYNC " << (p.oks ? "on" : "off")
             << "   TRANSPOSE " << ui::noteName (p.transpose)
             << "   LFO " << waves[juce::jlimit (0, 5, (int) p.lfoWave)] << " spd " << (int) p.lfoSpeed << " dly " << (int) p.lfoDelay
             << " PMD " << (int) p.lfoPmd << " AMD " << (int) p.lfoAmd << " PMS " << (int) p.pms
             << "   PEG R " << (int) p.pr[0] << "/" << (int) p.pr[1] << "/" << (int) p.pr[2] << "/" << (int) p.pr[3]
             << " L " << (int) p.pl[0] << "/" << (int) p.pl[1] << "/" << (int) p.pl[2] << "/" << (int) p.pl[3];
        g.setColour (ui::tealText); g.setFont (ui::font (12.0f));
        g.drawText (info, r.removeFromTop (20), juce::Justification::centredLeft, true);
        r.removeFromTop (4);

        const auto& A = dx7::algorithm (p.alg);
        const int rowH = juce::jmin (16, r.getHeight() / 7);
        auto rowText = [] (const juce::String& a, const juce::String& b, const juce::String& c, const juce::String& d,
                           const juce::String& e, const juce::String& f, const juce::String& h)
        { return a.paddedRight (' ', 4) + b.paddedRight (' ', 11) + c.paddedRight (' ', 5) + d.paddedRight (' ', 5)
               + e.paddedRight (' ', 17) + f.paddedRight (' ', 17) + h; };
        g.setFont (ui::mono (12.5f));
        g.setColour (ui::dim);
        g.drawText (rowText ("OP", "FREQ", "OUT", "DET", "RATE 1 2 3 4", "LEVEL 1 2 3 4", "VEL AMS RS  SCALING"),
                    r.removeFromTop (rowH), juce::Justification::centredLeft);
        auto four = [] (const uint8_t* v) { juce::String s; for (int i = 0; i < 4; ++i) s << juce::String (v[i]).paddedLeft (' ', 3) << (i < 3 ? " " : ""); return s; };
        for (int i = 0; i < 6; ++i)
        {
            const auto& o = p.op[(size_t) i];
            const bool car = (A.carriers & (1 << i)) != 0;
            static const char* curves[4] = { "-LN", "-EX", "+EX", "+LN" };
            juce::String sc; sc << "BP " << ui::midiName (o.bp + 21) << " " << curves[o.lc & 3] << (int) o.ld << " " << curves[o.rc & 3] << (int) o.rd;
            g.setColour (o.ol == 0 ? ui::dim.withAlpha (0.6f) : (car ? ui::teal : ui::text));
            g.drawText (rowText (juce::String (i + 1), fmui::ratioText (o), juce::String (o.ol), juce::String ((int) o.det - 7),
                                 four (o.r), four (o.l),
                                 juce::String (o.kvs) + "   " + juce::String (o.ams) + "   " + juce::String (o.rs) + "   " + sc),
                        r.removeFromTop (rowH), juce::Justification::centredLeft);
        }
    }
};

// ===========================================================================
//  Main window content.
// ===========================================================================
class MainComponent : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    MainComponent (juce::PropertiesFile* propsIn, bool withAudio)
        : props (propsIn), grid (slots)
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
        lnf.setColour (juce::TextEditor::backgroundColourId, ui::list);
        lnf.setColour (juce::TextEditor::textColourId, ui::text);
        lnf.setColour (juce::TextEditor::outlineColourId, ui::line);
        lnf.setColour (juce::TextEditor::focusedOutlineColourId, ui::teal.withAlpha (0.7f));
        lnf.setColour (juce::TextEditor::highlightColourId, ui::tealDark);
        lnf.setColour (juce::CaretComponent::caretColourId, ui::teal);
        lnf.setColour (juce::ListBox::backgroundColourId, ui::list);
        lnf.setColour (juce::AlertWindow::backgroundColourId, ui::bg);
        lnf.setColour (juce::AlertWindow::textColourId, ui::text);
        lnf.setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
        addChildComponent (updateBadge);   // UPDATE AVAILABLE (start-up version check)
        lnf.setColour (juce::Label::textColourId, ui::text);
        lnf.setColour (juce::TooltipWindow::backgroundColourId, ui::panel);
        lnf.setColour (juce::TooltipWindow::textColourId, ui::text);

        // ---- header
        for (auto* b : { &collectionBtn, &rescanBtn, &audioBtn, &newBtn, &openBtn, &addBtn, &upBtn, &downBtn, &renameBtn, &clearBtn, &compactBtn })
            addAndMakeVisible (*b);
        exportBtn.setColour (juce::TextButton::buttonColourId, ui::tealDark);
        exportBtn.setColour (juce::TextButton::textColourOffId, ui::light);
        addBtn.setColour (juce::TextButton::buttonColourId, ui::tealDark);
        addBtn.setColour (juce::TextButton::textColourOffId, ui::light);
        addAndMakeVisible (exportBtn);
        collectionBtn.onClick = [this] { chooseCollection(); };
        rescanBtn.onClick     = [this] { startScan (index.root.isDirectory() ? index.root : juce::File (props ? props->getValue ("collection") : juce::String())); };
        audioBtn.onClick      = [this] { showAudioSettings(); };
        audioBtn.setEnabled (withAudio);

        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        volume.setRange (0.0, 1.0, 0.01);
        volume.setValue (props ? props->getDoubleValue ("volume", 0.6) : 0.6, juce::dontSendNotification);
        volume.onValueChange = [this] { synth.setVolume ((float) (volume.getValue() * 1.2)); if (props) props->setValue ("volume", volume.getValue()); };
        synth.setVolume ((float) (volume.getValue() * 1.2));
        volume.setTooltip ("Preview volume");
        addAndMakeVisible (volume);
        noteBox.addItemList ({ "C2", "C3", "C4", "C5" }, 1);
        noteBox.setSelectedId (props ? props->getIntValue ("auditionNote", 2) : 2, juce::dontSendNotification);
        noteBox.onChange = [this] { if (props) props->setValue ("auditionNote", noteBox.getSelectedId()); };
        noteBox.setTooltip ("Note played when you click a voice");
        addAndMakeVisible (noteBox);

        // ---- browser
        search.setTextToShowWhenEmpty ("search name, bank or folder...", ui::dim);
        search.setFont (ui::font (15.0f));
        search.onTextChange = [this] { rebuildRows(); };
        search.onReturnKey  = [this] { list.grabKeyboardFocus(); if (! rows.empty() && list.getNumSelectedRows() == 0) list.selectRow (0); };
        addAndMakeVisible (search);
        folderBox.onChange = [this] { rebuildRows(); };
        addAndMakeVisible (folderBox);
        sortBox.addItemList ({ "Collection order", "Name A-Z", "Most duplicated" }, 1);
        sortBox.setSelectedId (1, juce::dontSendNotification);
        sortBox.onChange = [this] { rebuildRows(); };
        addAndMakeVisible (sortBox);
        uniqueBtn.setToggleState (true, juce::dontSendNotification);
        uniqueBtn.setTooltip ("Merge identical voices (same data and name) found in several files");
        uniqueBtn.onClick = [this] { rebuildRows(); };
        addAndMakeVisible (uniqueBtn);

        list.setModel (&model);
        list.setRowHeight (30);
        list.setMultipleSelectionEnabled (true);
        list.setOutlineThickness (1);
        list.setColour (juce::ListBox::outlineColourId, ui::line);
        addAndMakeVisible (list);
        addBtn.onClick = [this] { addSelectedRows (-1); };

        // ---- bank
        bankName.setText (props ? props->getValue ("bankName", "NEWBANK") : juce::String ("NEWBANK"), false);
        bankName.setFont (ui::font (15.0f));
        bankName.setTooltip ("Bank / file name");
        addAndMakeVisible (bankName);
        newBtn.onClick    = [this] { newBank(); };
        openBtn.onClick   = [this] { openBank(); };
        exportBtn.onClick = [this] { exportBank(); };
        upBtn.onClick     = [this] { moveSlot (-1); };
        downBtn.onClick   = [this] { moveSlot (1); };
        renameBtn.onClick = [this] { beginRename (grid.selected); };
        clearBtn.onClick  = [this] { clearSlot (grid.selected); };
        compactBtn.onClick = [this] { compact(); };
        compactBtn.setTooltip ("Close the gaps: move all voices to the front");

        grid.onSelect = [this] (int i) { grid.selected = i; grid.repaint(); if (slots[(size_t) i].used) showVoice (slots[(size_t) i].patch, "slot " + juce::String (i + 1) + "  |  " + slots[(size_t) i].source); };
        grid.onPlay   = [this] (int i) { if (slots[(size_t) i].used) { synth.setPatch (slots[(size_t) i].patch); audition(); } };
        grid.onRename = [this] (int i) { beginRename (i); };
        grid.onClear  = [this] (int i) { clearSlot (i); };
        grid.onSwap   = [this] (int a, int b) { std::swap (slots[(size_t) a], slots[(size_t) b]); grid.selected = b; grid.repaint(); };
        grid.onDrop   = [this] (const juce::String& d, int at)
        {
            std::vector<int> r;
            for (auto& t : juce::StringArray::fromTokens (d.substring (5), ",", "")) r.push_back (t.getIntValue());
            placeRows (r, at, true);
        };
        addAndMakeVisible (grid);

        renameEd.setFont (ui::mono (15.0f));
        renameEd.setInputRestrictions (10);
        renameEd.onReturnKey = [this] { commitRename(); };
        renameEd.onEscapeKey = [this] { renameEd.setVisible (false); grid.grabKeyboardFocus(); };
        renameEd.onFocusLost = [this] { commitRename(); };
        addChildComponent (renameEd);

        addAndMakeVisible (details);
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::teal.withAlpha (0.6f));
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::teal.withAlpha (0.25f));
        keyboard.setLowestVisibleKey (24);
        keyboard.setKeyWidth (18.0f);
        addAndMakeVisible (keyboard);

        // ---- audio
        if (withAudio)
        {
            std::unique_ptr<juce::XmlElement> saved (props ? props->getXmlValue ("audio") : nullptr);
            deviceManager.initialise (0, 2, saved.get(), true);
            if (saved == nullptr)
                for (auto& d : juce::MidiInput::getAvailableDevices())
                    deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
            player.setSource (&synth);
            deviceManager.addAudioCallback (&player);
            deviceManager.addMidiInputDeviceCallback ({}, &synth.collector);
            audioOn = true;
        }

        setSize (1320, 860);
        startTimerHz (30);

        if (props != nullptr)
        {
            const juce::File dir (props->getValue ("collection"));
            if (dir.isDirectory()) startScan (dir);
        }
    }

    ~MainComponent() override
    {
        stopTimer();
        if (scanner) { scanner->cancel = true; scanner->stopThread (10000); }
        if (audioOn)
        {
            deviceManager.removeMidiInputDeviceCallback ({}, &synth.collector);
            deviceManager.removeAudioCallback (&player);
            player.setSource (nullptr);
            if (props != nullptr)
                if (auto xml = deviceManager.createStateXml()) props->setValue ("audio", xml.get());
        }
        if (props != nullptr) { props->setValue ("bankName", bankName.getText()); props->saveIfNeeded(); }
        list.setModel (nullptr);
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    // ---------------------------------------------------------------- scanning
    void scanNow (const juce::File& dir)             // synchronous (snapshot / tests)
    {
        index.scan (dir);
        afterScan();
    }

    void startScan (const juce::File& dir)
    {
        if (! dir.isDirectory()) { status = "Choose the folder with your .syx collection (COLLECTION...)"; repaint(); return; }
        if (scanner) { scanner->cancel = true; scanner->stopThread (10000); }
        scanner = std::make_unique<ScanThread> (dir);
        scanner->startThread();
        status = "Reading " + dir.getFullPathName() + " ...";
        repaint();
    }

    // demo content for the UI snapshot
    void fillDemo()
    {
        std::vector<int> r; for (int i = 0; i < juce::jmin (20, (int) rows.size()); ++i) r.push_back (i * 3 % juce::jmax (1, (int) rows.size()));
        placeRows (r, 0, true);
        list.selectRow (4);
        grid.selected = 2; grid.repaint();
    }

private:
    struct ScanThread : juce::Thread
    {
        explicit ScanThread (juce::File d) : juce::Thread ("syx scan"), dir (std::move (d)) {}
        void run() override { result.scan (dir, &progress, &cancel); done = true; }
        juce::File dir; VoiceIndex result;
        std::atomic<float> progress { 0.0f }; std::atomic<bool> cancel { false }, done { false };
    };

    struct Row { int entry; int source; };

    struct Model : juce::ListBoxModel
    {
        MainComponent& o; explicit Model (MainComponent& m) : o (m) {}
        int getNumRows() override { return (int) o.rows.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override { o.paintRow (row, g, w, h, sel); }
        void selectedRowsChanged (int last) override { o.rowSelected (last); }
        void listBoxItemClicked (int row, const juce::MouseEvent& e) override { if (! e.mods.isShiftDown() && ! e.mods.isCommandDown()) o.rowClicked (row); }
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { o.placeRows ({ row }, -1, false); }
        void returnKeyPressed (int) override { o.addSelectedRows (-1); }
        void deleteKeyPressed (int) override {}
        juce::var getDragSourceDescription (const juce::SparseSet<int>& sel) override
        {
            juce::StringArray s;
            for (int i = 0; i < sel.size() && s.size() < 32; ++i) s.add (juce::String (sel[i]));
            return "rows:" + s.joinIntoString (",");
        }
        juce::String getTooltipForRow (int row) override
        {
            if (row < 0 || row >= (int) o.rows.size()) return {};
            const auto& e = o.index.entries[(size_t) o.rows[(size_t) row].entry];
            juce::StringArray where;
            for (auto& s : e.sources) { if (where.size() == 12) { where.add ("... +" + juce::String ((int) e.sources.size() - 12) + " more"); break; } where.add (o.index.fileOf (s) + "  #" + juce::String (s.slot + 1)); }
            return e.name + "\n" + where.joinIntoString ("\n");
        }
    };

    // ---------------------------------------------------------------- browser
    void afterScan()
    {
        lowerFiles.clear();
        for (auto& f : index.files) lowerFiles.add (f.toLowerCase());
        folderBox.clear (juce::dontSendNotification);
        folderBox.addItem ("All folders", 1);
        folderBox.addItemList (index.folders, 2);
        folderBox.setSelectedId (1, juce::dontSendNotification);
        rebuildRows();
        status = juce::String (index.voicesTotal) + " voices in " + juce::String (index.filesRead) + " files  |  "
               + juce::String ((int) index.entries.size()) + " unique  |  " + juce::String (index.blanksSkipped) + " blank / INIT skipped"
               + (index.filesFailed > 0 ? "  |  " + juce::String (index.filesFailed) + " files without DX7 voices" : juce::String());
        repaint();
    }

    void rebuildRows()
    {
        rows.clear();
        const auto tokens = juce::StringArray::fromTokens (search.getText().toLowerCase(), " ", "\"");
        const int folderId = folderBox.getSelectedId();
        const juce::String folder = folderId > 1 ? index.folders[folderId - 2] : juce::String();
        const bool unique = uniqueBtn.getToggleState();

        for (int ei = 0; ei < (int) index.entries.size(); ++ei)
        {
            const auto& e = index.entries[(size_t) ei];
            for (int si = 0; si < (int) e.sources.size(); ++si)
            {
                const auto& s = e.sources[(size_t) si];
                if (folder.isNotEmpty() && index.folderOf (s) != folder) continue;
                bool ok = true;
                for (auto& t : tokens)
                    if (t.isNotEmpty() && ! e.nameLower.contains (t) && ! lowerFiles[s.file].contains (t)) { ok = false; break; }
                if (! ok) continue;
                rows.push_back ({ ei, si });
                if (unique) break;
            }
        }
        const int mode = sortBox.getSelectedId();
        auto srcLess = [this] (const Row& a, const Row& b)
        {
            const auto& sa = index.entries[(size_t) a.entry].sources[(size_t) a.source];
            const auto& sb = index.entries[(size_t) b.entry].sources[(size_t) b.source];
            return sa.file != sb.file ? sa.file < sb.file : sa.slot < sb.slot;
        };
        if (mode == 1 || ! unique) std::stable_sort (rows.begin(), rows.end(), srcLess);
        if (mode == 2)
            std::stable_sort (rows.begin(), rows.end(), [this] (const Row& a, const Row& b)
            {
                const auto& ea = index.entries[(size_t) a.entry]; const auto& eb = index.entries[(size_t) b.entry];
                const int c = ea.nameLower.compareNatural (eb.nameLower);
                return c != 0 ? c < 0 : a.entry < b.entry;
            });
        if (mode == 3)
            std::stable_sort (rows.begin(), rows.end(), [this] (const Row& a, const Row& b)
            { return index.entries[(size_t) a.entry].sources.size() > index.entries[(size_t) b.entry].sources.size(); });
        list.deselectAllRows();
        list.updateContent();
        list.scrollToEnsureRowIsOnscreen (0);
        repaint (list.getBounds().withY (list.getY() - 22).withHeight (22));
    }

    void paintRow (int row, juce::Graphics& g, int w, int h, bool sel)
    {
        if (row < 0 || row >= (int) rows.size()) return;
        const auto& e = index.entries[(size_t) rows[(size_t) row].entry];
        const auto& s = e.sources[(size_t) rows[(size_t) row].source];
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.0f, 1.5f);
        if (sel) { g.setColour (ui::tealDark); g.fillRoundedRectangle (r, 4.0f); }
        else if (row % 2) { g.setColour (juce::Colours::white.withAlpha (0.015f)); g.fillRect (r); }
        r.removeFromLeft (8);
        g.setColour (sel ? ui::light : ui::text);
        g.setFont (ui::mono (15.0f));
        g.drawText (e.name, r.removeFromLeft (118), juce::Justification::centredLeft);
        if (uniqueBtn.getToggleState() && e.sources.size() > 1)
        {
            auto badge = r.removeFromRight (46).reduced (4, 6);
            g.setColour (sel ? ui::light.withAlpha (0.2f) : ui::tealDark.withAlpha (0.5f)); g.fillRoundedRectangle (badge, 7.0f);
            g.setColour (sel ? ui::light : ui::tealText); g.setFont (ui::font (11.0f, true));
            g.drawText (juce::String::fromUTF8 ("\xc3\x97") + juce::String ((int) e.sources.size()), badge, juce::Justification::centred);
        }
        g.setColour (sel ? ui::light.withAlpha (0.75f) : ui::dim);
        g.setFont (ui::font (12.0f));
        g.drawText (index.fileOf (s).upToLastOccurrenceOf (".", false, false) + "  #" + juce::String (s.slot + 1),
                    r.reduced (6, 0), juce::Justification::centredLeft, true);
    }

    juce::String describe (const Row& r) const
    {
        const auto& s = index.entries[(size_t) r.entry].sources[(size_t) r.source];
        return index.fileOf (s) + "  #" + juce::String (s.slot + 1);
    }

    void rowSelected (int last)
    {
        if (last < 0 || last >= (int) rows.size()) return;
        const auto& r = rows[(size_t) last];
        const auto& e = index.entries[(size_t) r.entry];
        showVoice (e.patch, describe (r) + (e.sources.size() > 1 ? "   (+" + juce::String ((int) e.sources.size() - 1) + " identical)" : juce::String()));
        synth.setPatch (e.patch);
        audition();
        lastSelectedRow = last;
    }
    void rowClicked (int row)
    {
        if (juce::Time::getMillisecondCounter() - lastAuditionMs < 150) return;   // the selection change just played it
        if (row >= 0 && row < (int) rows.size()) { synth.setPatch (index.entries[(size_t) rows[(size_t) row].entry].patch); audition(); }
    }

    void showVoice (const dx7::Patch& p, const juce::String& where) { details.show (p, where); }

    void audition()
    {
        static const int notes[4] = { 48, 60, 72, 84 };
        if (auditionNote >= 0) kbState.noteOff (1, auditionNote, 0.0f);
        auditionNote = notes[juce::jlimit (1, 4, noteBox.getSelectedId()) - 1];
        kbState.noteOn (1, auditionNote, 0.8f);
        lastAuditionMs = juce::Time::getMillisecondCounter();
        auditionOffAt = lastAuditionMs + 800;
    }

    // ---------------------------------------------------------------- bank
    void addSelectedRows (int at)
    {
        std::vector<int> r;
        const auto sel = list.getSelectedRows();
        for (int i = 0; i < sel.size(); ++i) r.push_back (sel[i]);
        if (r.empty()) { status = "Select one or more voices on the left first."; repaint(); return; }
        placeRows (r, at, false);
    }

    // overwrite: fill at, at+1, ... (drop); otherwise fill empty slots from 'at' (or the selected / first empty slot)
    void placeRows (const std::vector<int>& r, int at, bool overwrite)
    {
        int placed = 0, skipped = 0;
        int pos = at >= 0 ? at : (slots[(size_t) grid.selected].used ? 0 : grid.selected);
        for (int row : r)
        {
            if (row < 0 || row >= (int) rows.size()) continue;
            if (! overwrite) { int p = pos; while (p < 32 && slots[(size_t) p].used) ++p; if (p == 32) { p = 0; while (p < 32 && slots[(size_t) p].used) ++p; } pos = p; }
            if (pos >= 32) { ++skipped; continue; }
            const auto& rr = rows[(size_t) row];
            const auto& e = index.entries[(size_t) rr.entry];
            const auto& s = e.sources[(size_t) rr.source];
            slots[(size_t) pos] = { true, e.patch, index.bankOf (s) + " #" + juce::String (s.slot + 1) + "  " + index.folderOf (s) };
            grid.selected = pos;
            ++placed; ++pos;
        }
        status = placed > 0 ? "Placed " + juce::String (placed) + " voice(s)" + (skipped > 0 ? "  |  bank full, " + juce::String (skipped) + " not placed" : juce::String())
                            : juce::String ("Bank is full: clear a slot first.");
        grid.repaint(); repaint();
    }

    int usedCount() const { int n = 0; for (auto& s : slots) if (s.used) ++n; return n; }

    void clearSlot (int i)
    {
        if (i < 0 || i >= 32 || ! slots[(size_t) i].used) return;
        slots[(size_t) i] = {};
        grid.repaint(); repaint();
    }
    void moveSlot (int d)
    {
        const int a = grid.selected, b = a + d;
        if (b < 0 || b >= 32) return;
        std::swap (slots[(size_t) a], slots[(size_t) b]);
        grid.selected = b; grid.repaint();
    }
    void compact()
    {
        std::array<BankSlot, 32> out {}; int n = 0;
        for (auto& s : slots) if (s.used) out[(size_t) n++] = s;
        slots = out; grid.selected = juce::jmin (grid.selected, juce::jmax (0, n - 1)); grid.repaint();
    }

    void beginRename (int i)
    {
        if (i < 0 || i >= 32 || ! slots[(size_t) i].used) return;
        renameSlot = i;
        auto c = grid.cell (i).translated (grid.getX(), grid.getY());
        renameEd.setBounds (c.withTrimmedLeft (26).reduced (2, 6).withHeight (juce::jmin (26, c.getHeight() - 8)));
        renameEd.setText (slots[(size_t) i].patch.getName(), false);
        renameEd.setVisible (true);
        renameEd.grabKeyboardFocus();
        renameEd.selectAll();
    }
    void commitRename()
    {
        if (! renameEd.isVisible()) return;
        renameEd.setVisible (false);
        if (renameSlot >= 0 && renameSlot < 32 && slots[(size_t) renameSlot].used)
        {
            ui::setName (slots[(size_t) renameSlot].patch, renameEd.getText());
            showVoice (slots[(size_t) renameSlot].patch, "slot " + juce::String (renameSlot + 1) + "  |  " + slots[(size_t) renameSlot].source);
        }
        grid.repaint();
    }

    void newBank()
    {
        if (usedCount() == 0) return;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "New bank", "Clear all 32 slots?", "Clear", "Cancel", this,
            juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) { slots = {}; grid.selected = 0; grid.repaint(); status = "New empty bank"; repaint(); } }));
    }

    juce::File lastDir() const
    {
        const juce::File d (props ? props->getValue ("lastDir") : juce::String());
        return d.isDirectory() ? d : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    }

    void openBank()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open a DX7 bank", lastDir(), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            juce::MemoryBlock mb; f.loadFileAsData (mb);
            const auto res = dx7::parseSysEx (mb.getData(), mb.getSize());
            if (res.voices.empty()) { status = f.getFileName() + ": no DX7 voices found"; repaint(); return; }
            slots = {};
            for (size_t i = 0; i < res.voices.size() && i < 32; ++i)
            {
                const auto& p = res.voices[i];
                if (p.isBlank() || VoiceIndex::isInitVoice (p)) continue;
                slots[i] = { true, p, f.getFileNameWithoutExtension() + " #" + juce::String ((int) i + 1) };
            }
            bankName.setText (f.getFileNameWithoutExtension(), false);
            if (props) props->setValue ("lastDir", f.getParentDirectory().getFullPathName());
            grid.selected = 0; grid.repaint();
            status = "Opened " + f.getFileName() + ": " + juce::String (usedCount()) + " voices" + (res.badChecksums ? "  (checksum was wrong)" : "");
            repaint();
        });
    }

    std::vector<dx7::Patch> bankVoices() const
    {
        std::vector<dx7::Patch> v;
        for (auto& s : slots) v.push_back (s.used ? s.patch : dx7::initVoice());
        return v;
    }

    void exportBank()
    {
        if (usedCount() == 0) { status = "The bank is empty: add voices first."; repaint(); return; }
        auto name = juce::File::createLegalFileName (bankName.getText().trim());
        if (name.isEmpty()) name = "NEWBANK";
        chooser = std::make_unique<juce::FileChooser> ("Export DX7 bank", lastDir().getChildFile (name + ".syx"), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (! f.hasFileExtension ("syx")) f = f.withFileExtension ("syx");
            const auto voices = bankVoices();
            const auto bytes = dx7::writeBank (voices);
            if (! f.replaceWithData (bytes.data(), bytes.size())) { status = "Could not write " + f.getFullPathName(); repaint(); return; }
            // verify by reading the file back
            juce::MemoryBlock mb; f.loadFileAsData (mb);
            const auto res = dx7::parseSysEx (mb.getData(), mb.getSize());
            bool same = res.banks == 1 && res.badChecksums == 0 && res.voices.size() == 32;
            for (size_t i = 0; same && i < 32; ++i) same = dx7::sameVoice (res.voices[i], voices[i]);
            if (props) { props->setValue ("lastDir", f.getParentDirectory().getFullPathName()); props->setValue ("bankName", f.getFileNameWithoutExtension()); }
            bankName.setText (f.getFileNameWithoutExtension(), false);
            status = (same ? "Exported " : "EXPORT CHECK FAILED: ") + f.getFullPathName() + "  |  " + juce::String (usedCount()) + " voices + "
                   + juce::String (32 - usedCount()) + " INIT VOICE  |  " + juce::String ((int) mb.getSize()) + " bytes" + (same ? ", checksum OK" : "");
            repaint();
        });
    }

    // ---------------------------------------------------------------- misc
    void chooseCollection()
    {
        chooser = std::make_unique<juce::FileChooser> ("Folder with DX7 .syx files", index.root.isDirectory() ? index.root : juce::File(), "");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
        {
            const auto d = fc.getResult();
            if (! d.isDirectory()) return;
            if (props) props->setValue ("collection", d.getFullPathName());
            startScan (d);
        });
    }

    void showAudioSettings()
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 1, 2, true, false, true, false);
        sel->setSize (560, 440);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (sel);
        o.dialogTitle = "Audio / MIDI";
        o.dialogBackgroundColour = ui::bg;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    void timerCallback() override
    {
        if (auditionNote >= 0 && juce::Time::getMillisecondCounter() >= auditionOffAt)
        { kbState.noteOff (1, auditionNote, 0.0f); auditionNote = -1; }
        if (scanner)
        {
            if (scanner->done)
            {
                scanner->stopThread (1000);
                index = std::move (scanner->result);
                scanner.reset();
                afterScan();
            }
            else
            {
                status = "Reading collection ... " + juce::String (juce::roundToInt (scanner->progress.load() * 100.0f)) + "%";
                repaint (getLocalBounds().removeFromBottom (26));
            }
        }
    }

    // ---------------------------------------------------------------- layout
    void paint (juce::Graphics& g) override
    {
        g.fillAll (ui::bg);
        auto top = getLocalBounds().removeFromTop (58).reduced (20, 0);
        g.setColour (juce::Colours::white); g.setFont (ui::font (22.0f, true));
        g.drawText ("COHESION - SYSEX", top.removeFromLeft (230), juce::Justification::centredLeft);
        g.setColour (ui::teal); g.setFont (ui::font (13.0f, true));
        g.drawText ("DX7 BANK BUILDER", top.removeFromLeft (200), juce::Justification::centredLeft);
        g.setColour (ui::line); g.drawHorizontalLine (58, 0.0f, (float) getWidth());

        g.setColour (ui::dim); g.setFont (ui::font (12.0f));
        g.drawText ("VOICES  (" + juce::String ((int) rows.size()) + (list.getNumSelectedRows() > 1 ? ",  " + juce::String (list.getNumSelectedRows()) + " selected" : juce::String()) + ")",
                    list.getX(), list.getY() - 20, 300, 16, juce::Justification::centredLeft);
        g.drawText ("BANK  (" + juce::String (usedCount()) + " / 32)", grid.getX() + 3, grid.getY() - 20, 200, 16, juce::Justification::centredLeft);
        g.drawText ("drag voices onto a slot  |  drag slots to swap  |  double-click / F2 = rename  |  Del = clear",
                    grid.getX(), grid.getY() - 20, grid.getWidth() - 3, 16, juce::Justification::centredRight);
        g.setFont (ui::font (11.0f));
        g.drawText ("PREVIEW", volume.getX() - 60, volume.getY(), 56, volume.getHeight(), juce::Justification::centredRight);

        if (index.entries.empty() && scanner == nullptr)
        {
            g.setColour (ui::dim); g.setFont (ui::font (14.0f));
            g.drawText ("No collection loaded: click COLLECTION... and choose your sysex/voice folder.",
                        list.getBounds().reduced (20), juce::Justification::centred, true);
        }
        auto sb = getLocalBounds().removeFromBottom (26).reduced (20, 0);
        g.setColour (ui::tealText); g.setFont (ui::font (12.5f));
        g.drawText (status, sb, juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (58).reduced (20, 13);
        // right after the "DX7 BANK BUILDER" caption painted in paint()
        updateBadge.setFontHeight (10.0f);
        updateBadge.placeAt (20 + 230 + juce::roundToInt (ui::font (13.0f, true).getStringWidthFloat ("DX7 BANK BUILDER")) + 12, 21, 17);
        audioBtn.setBounds (top.removeFromRight (110));      top.removeFromRight (8);
        rescanBtn.setBounds (top.removeFromRight (80));      top.removeFromRight (8);
        collectionBtn.setBounds (top.removeFromRight (130)); top.removeFromRight (20);
        noteBox.setBounds (top.removeFromRight (64));        top.removeFromRight (8);
        volume.setBounds (top.removeFromRight (120));

        r.removeFromBottom (26);
        r.reduce (20, 0);
        keyboard.setBounds (r.removeFromBottom (72));
        r.removeFromBottom (8);
        details.setBounds (r.removeFromBottom (150));
        r.removeFromBottom (10);
        r.removeFromTop (12);

        auto left = r.removeFromLeft (juce::roundToInt (r.getWidth() * 0.47f));
        r.removeFromLeft (18);
        auto tools = left.removeFromTop (32);
        uniqueBtn.setBounds (tools.removeFromRight (110)); tools.removeFromRight (6);
        sortBox.setBounds (tools.removeFromRight (150));   tools.removeFromRight (6);
        folderBox.setBounds (tools.removeFromRight (170)); tools.removeFromRight (6);
        search.setBounds (tools);
        left.removeFromTop (26);
        addBtn.setBounds (left.removeFromBottom (32).removeFromRight (190));
        left.removeFromBottom (8);
        list.setBounds (left);

        auto bank = r.removeFromTop (32);
        exportBtn.setBounds (bank.removeFromRight (130)); bank.removeFromRight (6);
        openBtn.setBounds (bank.removeFromRight (110));   bank.removeFromRight (6);
        newBtn.setBounds (bank.removeFromRight (70));     bank.removeFromRight (10);
        bankName.setBounds (bank);
        r.removeFromTop (26);
        auto slotTools = r.removeFromBottom (32);
        r.removeFromBottom (8);
        for (auto* b : { &upBtn, &downBtn }) { b->setBounds (slotTools.removeFromLeft (70)); slotTools.removeFromLeft (6); }
        for (auto* b : { &renameBtn, &clearBtn, &compactBtn }) { b->setBounds (slotTools.removeFromLeft (100)); slotTools.removeFromLeft (6); }
        grid.setBounds (r.expanded (3, 0));
        renameEd.setVisible (false);
    }

    juce::PropertiesFile* props = nullptr;
    CohesionLookAndFeel lnf;

    VoiceIndex index;
    juce::StringArray lowerFiles;
    std::vector<Row> rows;
    std::unique_ptr<ScanThread> scanner;
    std::array<BankSlot, 32> slots {};

    juce::TextButton collectionBtn { "COLLECTION..." }, rescanBtn { "RESCAN" }, audioBtn { "AUDIO / MIDI" },
                     newBtn { "NEW" }, openBtn { "OPEN .SYX" }, exportBtn { "EXPORT .SYX" },
                     addBtn { "ADD TO BANK  >>" }, upBtn { "UP" }, downBtn { "DOWN" },
                     renameBtn { "RENAME" }, clearBtn { "CLEAR SLOT" }, compactBtn { "COMPACT" };
    juce::ToggleButton uniqueBtn { "UNIQUE ONLY" };
    juce::TextEditor search, bankName, renameEd;
    juce::ComboBox folderBox, sortBox, noteBox;
    juce::Slider volume;
    Model model { *this };
    juce::ListBox list { "voices" };
    SlotGrid grid;
    VoiceDetails details;
    juce::TooltipWindow tooltips { this, 600 };
    cohesion::UpdateBadge updateBadge;

    juce::MidiKeyboardState kbState;
    juce::MidiKeyboardComponent keyboard { kbState, juce::MidiKeyboardComponent::horizontalKeyboard };
    PreviewSynth synth { kbState };
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer player;
    bool audioOn = false;

    int auditionNote = -1, renameSlot = -1, lastSelectedRow = -1;
    juce::uint32 auditionOffAt = 0, lastAuditionMs = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;
};
