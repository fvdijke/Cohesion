// ===========================================================================
//  COHESION - update check (shared by every COHESION app, header-only).
//
//  Each app compares its own version (COHESION_APP_VERSION, from versions.json via CMake)
//  with the entry for its own id (COHESION_APP_ID) in the versions.json published on GitHub.
//  When the published one is newer, UpdateBadge shows "UPDATE AVAILABLE"; hovering it names
//  the new version, clicking it opens the releases page.
//
//  - The download runs once per process on a low-priority background thread, started when
//    the first editor opens (never during plugin scanning, never on the audio thread).
//  - Offline, timeout, HTTP error, bad JSON, missing entry: the badge simply stays hidden.
//  - Environment overrides (testing):  COHESION_UPDATE_URL = other URL or a local file path,
//                                      COHESION_UPDATE_CHECK = 0  -> never go online.
//
//  Usage in an editor:   cohesion::UpdateBadge updateBadge;   addChildComponent (updateBadge);
//                        updateBadge.placeAt (x, y);           (in resized / layout)
// ===========================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

#ifndef COHESION_APP_ID
 #define COHESION_APP_ID ""          // not set (e.g. offline test builds): the check is disabled
#endif
#ifndef COHESION_APP_VERSION
 #define COHESION_APP_VERSION "0.0.0"
#endif

namespace cohesion
{
namespace update
{
    inline constexpr const char* kVersionsUrl = "https://raw.githubusercontent.com/fvdijke/Cohesion/master/versions.json";
    inline constexpr const char* kReleasesUrl = "https://github.com/fvdijke/Cohesion/releases/latest";
    inline constexpr int kTimeoutMs = 3000;

    inline juce::String appId()          { return COHESION_APP_ID; }
    inline juce::String currentVersion() { return COHESION_APP_VERSION; }

    inline bool isValidVersion (const juce::String& v)
    {
        return v.isNotEmpty() && v.length() < 32 && v.containsOnly ("0123456789.")
            && ! v.startsWithChar ('.') && ! v.endsWithChar ('.') && ! v.contains ("..");
    }

    /** <0 if a is older than b, 0 if equal, >0 if a is newer. "1.3" == "1.3.0". */
    inline int compareVersions (const juce::String& a, const juce::String& b)
    {
        const auto pa = juce::StringArray::fromTokens (a, ".", {});
        const auto pb = juce::StringArray::fromTokens (b, ".", {});
        for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
        {
            const int x = i < pa.size() ? pa[i].getIntValue() : 0;
            const int y = i < pb.size() ? pb[i].getIntValue() : 0;
            if (x != y) return x < y ? -1 : 1;
        }
        return 0;
    }

    inline bool isEnabled()
    {
        return appId().isNotEmpty()
            && juce::SystemStats::getEnvironmentVariable ("COHESION_UPDATE_CHECK", "1") != "0";
    }

    /** The version listed for app `id` in a versions.json text, or empty. */
    inline juce::String parsePublishedVersion (const juce::String& text, const juce::String& id)
    {
        juce::var json;
        if (juce::JSON::parse (text, json).failed()) return {};
        auto entry = json.getProperty ("apps", {}).getProperty (juce::Identifier (id), {});
        if (entry.isObject()) entry = entry.getProperty ("version", {});
        const auto v = entry.toString().trim();
        return isValidVersion (v) ? v : juce::String();
    }

    inline juce::String readText (juce::InputStream& in)
    {
        juce::MemoryBlock mb;
        in.readIntoMemoryBlock (mb, 64 * 1024);
        return juce::String::fromUTF8 ((const char*) mb.getData(), (int) mb.getSize());
    }

    // Result of this module's single check, kept for the lifetime of the process.
    struct State
    {
        std::atomic<int> phase { 0 };      // 0 = not started, 1 = running, 2 = done
        juce::CriticalSection lock;
        juce::String published;            // valid once phase == 2 (empty = unknown)
    };
    inline State& state() { static State s; return s; }

    /** Owns the background thread; shared by all editors of this module (SharedResourcePointer). */
    class Checker : private juce::Thread
    {
    public:
        Checker() : juce::Thread ("COHESION update check")
        {
            auto& s = state();
            int expected = 0;
            if (! isEnabled()) { s.phase = 2; return; }
            if (s.phase.compare_exchange_strong (expected, 1))
                startThread (juce::Thread::Priority::low);
        }

        ~Checker() override
        {
            signalThreadShouldExit();
            {   // abort a pending connect / read at once, so closing an editor never waits on the network
                const juce::ScopedLock sl (streamLock);
                if (stream != nullptr) stream->cancel();
            }
            stopThread (kTimeoutMs + 1000);
            int running = 1;
            state().phase.compare_exchange_strong (running, 0);   // interrupted: try again next time
        }

        bool isDone() const { return state().phase.load() == 2; }

        /** The newer published version, or empty when there is none (or it is unknown). */
        juce::String newerVersion() const
        {
            if (! isDone()) return {};
            const juce::ScopedLock sl (state().lock);
            const auto& p = state().published;
            return (p.isNotEmpty() && compareVersions (p, currentVersion()) > 0) ? p : juce::String();
        }

        /** Tests only: block until the check has finished. */
        bool waitUntilDone (int timeoutMs)
        {
            const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
            while (! isDone() && juce::Time::getMillisecondCounter() < end) juce::Thread::sleep (10);
            return isDone();
        }

    private:
        void run() override
        {
            const auto v = fetchPublishedVersion();
            if (threadShouldExit()) return;
            { const juce::ScopedLock sl (state().lock); state().published = v; }
            state().phase = 2;
        }

        /** The version published for this app, or empty on any problem. Runs on the thread. */
        juce::String fetchPublishedVersion()
        {
            auto src = juce::SystemStats::getEnvironmentVariable ("COHESION_UPDATE_URL", {}).trim();
            if (src.isEmpty()) src = kVersionsUrl;
            if (juce::File::isAbsolutePath (src))                       // local file (testing)
            {
                auto in = juce::File (src).createInputStream();
                return in != nullptr ? parsePublishedVersion (readText (*in), appId()) : juce::String();
            }
            {
                const juce::ScopedLock sl (streamLock);
                if (threadShouldExit()) return {};
                stream = std::make_unique<juce::WebInputStream> (juce::URL (src), false);
                stream->withConnectionTimeout (kTimeoutMs).withNumRedirectsToFollow (3);
            }
            juce::String text;
            if (stream->connect (nullptr) && stream->getStatusCode() == 200 && ! threadShouldExit())
                text = readText (*stream);
            {
                const juce::ScopedLock sl (streamLock);
                stream.reset();
            }
            return text.isNotEmpty() ? parsePublishedVersion (text, appId()) : juce::String();
        }

        juce::CriticalSection streamLock;
        std::unique_ptr<juce::WebInputStream> stream;
    };
} // namespace update

// ===========================================================================
/** "UPDATE AVAILABLE" pill. Hidden until the check found a newer version. */
class UpdateBadge : public juce::Component, private juce::Timer
{
public:
    static constexpr const char* kComponentId = "cohesionUpdateBadge";

    UpdateBadge()
    {
        setComponentID (kComponentId);
        setVisible (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimer (300);
    }
    ~UpdateBadge() override { hidePopup(); }

    void setFontHeight (float h) { fontHeight = h; }
    int  getPreferredWidth() const { return juce::roundToInt (font().getStringWidthFloat (kText)) + 14; }
    void placeAt (int x, int y, int h = 15) { setBounds (x, y, getPreferredWidth(), h); }
    /** Right after the text of a label (e.g. "by Pepperwood"), vertically centred on it. */
    void placeAfter (const juce::Label& l, int gap = 8)
    {
        const int x = l.getX() + l.getBorderSize().getLeft()
                    + juce::roundToInt (l.getFont().getStringWidthFloat (l.getText())) + gap;
        placeAt (x, l.getBounds().getCentreY() - 7);
    }

    bool hasUpdate() const { return newVersion.isNotEmpty(); }
    juce::String getNewVersion() const { return newVersion; }
    juce::String getTooltipText() const
    {
        return "Version " + newVersion + " is available (this is " + update::currentVersion()
             + "). Click to open the download page.";
    }

    /** Picks up the result of the check (called by the timer; tests may call it directly). */
    void refresh()
    {
        if (! checker->isDone()) return;
        stopTimer();
        newVersion = checker->newerVersion();
        setVisible (hasUpdate());
        repaint();
    }

    /** Tests only: wait for the check, then update the badge. */
    void waitForCheck (int timeoutMs) { checker->waitUntilDone (timeoutMs); refresh(); }

    /** Finds the badge anywhere inside a component tree (tests). */
    static UpdateBadge* find (juce::Component& root)
    {
        if (auto* b = dynamic_cast<UpdateBadge*> (&root)) return b;
        for (auto* c : root.getChildren())
            if (auto* b = find (*c)) return b;
        return nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const juce::Colour amber (0xfff0b44c);
        g.setColour (amber.withAlpha (hover ? 0.28f : 0.14f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (amber.withAlpha (0.9f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setFont (font());
        g.drawText (kText, getLocalBounds(), juce::Justification::centred, false);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); showPopup(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); hidePopup(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && hasUpdate()) juce::URL (update::kReleasesUrl).launchInDefaultBrowser();
    }

private:
    static constexpr const char* kText = "UPDATE AVAILABLE";

    juce::Font font() const { return juce::Font ("Segoe UI", fontHeight, juce::Font::bold).withExtraKerningFactor (0.08f); }
    void timerCallback() override { refresh(); }

    // A small tooltip-style card on the top-level component (works in every app, also those
    // without a TooltipWindow, and is not affected by the editors' scaled canvases).
    struct Popup : juce::Component
    {
        juce::String text;
        juce::Font f { "Segoe UI", 12.0f, juce::Font::plain };
        Popup() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (juce::Colour (0xf20e1013)); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colour (0xfff0b44c).withAlpha (0.7f)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setColour (juce::Colour (0xffeafff6)); g.setFont (f);
            g.drawText (text, getLocalBounds().reduced (8, 0), juce::Justification::centredLeft, false);
        }
    };

    void showPopup()
    {
        auto* top = getTopLevelComponent();
        if (! hasUpdate() || top == nullptr || top == this) return;
        if (popup == nullptr) popup = std::make_unique<Popup>();
        popup->text = getTooltipText();
        const auto at = top->getLocalArea (this, getLocalBounds());
        const int w = juce::roundToInt (popup->f.getStringWidthFloat (popup->text)) + 18, h = 24;
        auto pr = juce::Rectangle<int> (at.getX(), at.getBottom() + 4, w, h);
        pr.setX (juce::jlimit (0, juce::jmax (0, top->getWidth() - w), pr.getX()));
        popup->setBounds (pr);
        top->addAndMakeVisible (*popup);
        popup->toFront (false);
    }
    void hidePopup()
    {
        if (popup != nullptr)
            if (auto* p = popup->getParentComponent()) p->removeChildComponent (popup.get());
    }

    juce::SharedResourcePointer<update::Checker> checker;
    juce::String newVersion;
    std::unique_ptr<Popup> popup;
    float fontHeight = 9.5f;
    bool hover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateBadge)
};
} // namespace cohesion
