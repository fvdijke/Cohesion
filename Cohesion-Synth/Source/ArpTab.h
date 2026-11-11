#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Params.h"
#include "ArpPatterns.h"

// Relies on LabeledKnob + the attachment typedefs from PluginEditor.h (included first).

namespace arpui
{
    static const juce::Colour teal { 0xff39ffb0 }, tealDark { 0xff0f6e56 }, txt { 0xff9fb0aa }, dim { 0xff6c7a74 };

    inline void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float realValue)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (realValue));
            p->endChangeGesture();
        }
    }
    inline float get (juce::AudioProcessorValueTreeState& apvts, const juce::String& id) { return apvts.getRawParameterValue (id)->load(); }

    inline void card (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff0d0f12)); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    }
    inline void caption (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j = juce::Justification::centredLeft)
    {
        g.setColour (dim); g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
        g.drawText (t, r, j);
    }
}

// ---------------------------------------------------------------------------
//  Tempo display: host BPM when SYNC is on, otherwise the internal tempo
//  (drag up/down to change, double-click = 120).
// ---------------------------------------------------------------------------
struct TempoBox : juce::Component
{
    CohesionSynthAudioProcessor& proc; float startVal = 120.0f;
    explicit TempoBox (CohesionSynthAudioProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
    bool synced() const { return arpui::get (proc.apvts, pid::arpSync) > 0.5f; }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a0c0f)); g.fillRoundedRectangle (r, 5.0f);
        const double bpm = synced() ? proc.getFxBpm() : (double) arpui::get (proc.apvts, pid::arpTempo);
        g.setColour (juce::Colour (0xffeafff6)); g.setFont (juce::Font ("Segoe UI", 13.0f, juce::Font::plain));
        g.drawText (juce::String (bpm, 1) + " BPM", r.withTrimmedBottom (10), juce::Justification::centred);
        g.setColour (synced() ? arpui::teal.withAlpha (0.8f) : arpui::dim); g.setFont (juce::Font ("Segoe UI", 8.0f, juce::Font::plain));
        g.drawText (synced() ? "HOST" : "INTERNAL", r.withTrimmedTop (r.getHeight() - 14), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { startVal = arpui::get (proc.apvts, pid::arpTempo); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (synced()) return;
        const float fine = e.mods.isShiftDown() ? 0.05f : 0.3f;
        arpui::setParam (proc.apvts, pid::arpTempo, juce::jlimit (40.0f, 240.0f, std::round ((startVal - (float) e.getDistanceFromDragStartY() * fine) * 10.0f) / 10.0f));
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (! synced()) arpui::setParam (proc.apvts, pid::arpTempo, 120.0f); }
};

// ---------------------------------------------------------------------------
//  16-step velocity sequencer. Click = step on/off, drag = draw velocity.
// ---------------------------------------------------------------------------
struct StepSequencer : juce::Component
{
    CohesionSynthAudioProcessor& proc;
    std::function<void()> onEdited;
    int downCol = -1; bool drawing = false; juce::Point<int> downPos;

    explicit StepSequencer (CohesionSynthAudioProcessor& p) : proc (p) {}

    juce::Rectangle<float> barArea() const { return { 16.0f, 44.0f, (float) getWidth() - 32.0f, (float) getHeight() - 44.0f - 54.0f }; }
    float colW() const { return barArea().getWidth() / 16.0f; }
    int colAt (int x) const { return (int) std::floor (((float) x - barArea().getX()) / colW()); }
    juce::Rectangle<float> barRect (int i) const
    {
        auto a = barArea(); const float cw = colW(), bw = juce::jmin (46.0f, cw - 14.0f);
        return { a.getX() + i * cw + (cw - bw) * 0.5f, a.getY(), bw, a.getHeight() };
    }

    void paint (juce::Graphics& g) override
    {
        arpui::card (g, getLocalBounds().toFloat());
        g.setColour (juce::Colour (0xff5dcaa5)); g.setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("STEP SEQUENCER  \xe2\x80\x94  VELOCITY"), 16, 10, 400, 18, juce::Justification::centredLeft);
        arpui::caption (g, juce::String::fromUTF8 ("click = step on/off  \xc2\xb7  drag = velocity"), getLocalBounds().withTrimmedRight (16).withHeight (38), juce::Justification::centredRight);

        auto& apvts = proc.apvts;
        const int steps = (int) arpui::get (apvts, pid::arpSteps);
        const int play = arpui::get (apvts, pid::arpOn) > 0.5f ? proc.getArp().uiStep.load() : -1;
        for (int i = 0; i < 16; ++i)
        {
            auto r = barRect (i);
            const bool on = arpui::get (apvts, pid::arpStepOn (i)) > 0.5f;
            const float vel = arpui::get (apvts, pid::arpStepVel (i));
            const float a = i < steps ? 1.0f : 0.25f;
            if (i == play) { g.setColour (arpui::teal.withAlpha (0.08f)); g.fillRoundedRectangle (r.expanded (3.0f, 4.0f), 5.0f);
                             g.setColour (arpui::teal.withAlpha (0.5f)); g.drawRoundedRectangle (r.expanded (3.0f, 4.0f), 5.0f, 1.0f); }
            g.setColour (juce::Colour (0xff0a0c0f).withAlpha (a)); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colour (0xff1c2127).withAlpha (a)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
            const float h = juce::jmax (3.0f, r.getHeight() * vel);
            auto fill = r.withTop (r.getBottom() - h);
            g.setColour ((on ? arpui::teal.withAlpha (i == play ? 1.0f : 0.8f) : juce::Colour (0xff2b3138)).withAlpha (a * (on ? (i == play ? 1.0f : 0.8f) : 1.0f)));
            g.fillRoundedRectangle (fill, 4.0f);
            const float cy = r.getBottom() + 20.0f;
            g.setColour ((on ? (i == play ? juce::Colour (0xffeafff6) : arpui::teal) : juce::Colour (0xff2b3138)).withAlpha (a));
            g.fillEllipse (r.getCentreX() - 6, cy - 6, 12, 12);
            if (! on) { g.setColour (juce::Colour (0xff4a5560).withAlpha (a)); g.drawEllipse (r.getCentreX() - 6, cy - 6, 12, 12, 1.0f); }
            g.setColour ((i == play ? juce::Colour (0xffeafff6) : on ? arpui::txt : juce::Colour (0xff4a5560)).withAlpha (a));
            g.setFont (juce::Font ("Segoe UI", 9.0f, juce::Font::plain));
            g.drawText (juce::String (i + 1), (int) r.getX(), (int) cy + 8, (int) r.getWidth(), 14, juce::Justification::centred);
        }
    }

    void setVelAt (juce::Point<int> p)
    {
        const int c = colAt (p.x); if (c < 0 || c > 15) return;
        auto r = barRect (c);
        const float v = juce::jlimit (0.0f, 1.0f, (r.getBottom() - (float) p.y) / r.getHeight());
        arpui::setParam (proc.apvts, pid::arpStepVel (c), v);
        if (onEdited) onEdited();
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override { downCol = colAt (e.x); downPos = e.getPosition(); drawing = false; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (downCol < 0 || downCol > 15) return;
        if (! drawing && e.getPosition().getDistanceFrom (downPos) > 3) drawing = true;
        if (drawing) setVelAt (e.getPosition());
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! drawing && downCol >= 0 && downCol <= 15 && colAt (e.x) == downCol)
        {
            const bool on = arpui::get (proc.apvts, pid::arpStepOn (downCol)) > 0.5f;
            arpui::setParam (proc.apvts, pid::arpStepOn (downCol), on ? 0.0f : 1.0f);
            if (onEdited) onEdited();
            repaint();
        }
        downCol = -1; drawing = false;
    }
};

// ---------------------------------------------------------------------------
//  Held-chord display: two octaves starting at the C of the lowest held note.
// ---------------------------------------------------------------------------
struct HeldKeyboard : juce::Component
{
    CohesionSynthAudioProcessor& proc;
    explicit HeldKeyboard (CohesionSynthAudioProcessor& p) : proc (p) {}
    bool isHeld (int n) const { return n >= 0 && n < 128 && (proc.getArp().heldMask[n >> 5].load() & (1u << (n & 31))) != 0; }

    void paint (juce::Graphics& g) override
    {
        int lowest = -1; for (int n = 0; n < 128; ++n) if (isHeld (n)) { lowest = n; break; }
        const int start = lowest >= 0 ? (lowest / 12) * 12 : 48;
        static const int whiteOff[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int blackOff[5] = { 1, 3, 6, 8, 10 };
        static const int blackPos[5] = { 0, 1, 3, 4, 5 };   // after white key index
        const float kw = (float) getWidth() / 14.0f, kh = (float) getHeight();
        for (int o = 0; o < 2; ++o)
            for (int w = 0; w < 7; ++w)
            {
                const int note = start + o * 12 + whiteOff[w];
                juce::Rectangle<float> r ((o * 7 + w) * kw, 0, kw - 1.0f, kh);
                g.setColour (isHeld (note) ? arpui::teal.withAlpha (0.85f) : juce::Colour (0xff1a1d22)); g.fillRoundedRectangle (r, 3.0f);
                g.setColour (juce::Colour (0xff0a0c0f)); g.drawRoundedRectangle (r, 3.0f, 1.0f);
                if (w == 0)
                {
                    g.setColour (isHeld (note) ? juce::Colour (0xff0a0c0f) : arpui::dim); g.setFont (juce::Font ("Segoe UI", 8.0f, juce::Font::plain));
                    g.drawText ("C" + juce::String (note / 12 - 1), r.withTrimmedTop (kh - 14), juce::Justification::centred);
                }
            }
        for (int o = 0; o < 2; ++o)
            for (int b = 0; b < 5; ++b)
            {
                const int note = start + o * 12 + blackOff[b];
                const float x = (o * 7 + blackPos[b] + 1) * kw - kw * 0.3f;
                juce::Rectangle<float> r (x, 0, kw * 0.6f, kh * 0.6f);
                g.setColour (isHeld (note) ? arpui::teal : juce::Colour (0xff07090b)); g.fillRoundedRectangle (r, 2.0f);
            }
    }
};

// ---------------------------------------------------------------------------
//  The ARP tab.
// ---------------------------------------------------------------------------
struct ArpTab : juce::Component, private juce::Timer
{
    CohesionSynthAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    // top
    juce::ToggleButton onBtn, syncBtn, holdBtn, latchBtn;
    std::unique_ptr<ButtonAtt> onAtt, syncAtt, holdAtt, latchAtt;
    juce::ComboBox modeBox, orderBox;
    std::unique_ptr<ComboAtt> modeAtt, orderAtt;
    juce::TextButton patPrev { "<" }, patNext { ">" }, stepsDown { "<" }, stepsUp { ">" }, randomizeBtn { "RANDOMIZE" };
    juce::Label stepsLbl;
    juce::ComboBox patBox;
    juce::OwnedArray<LabeledKnob> topKnobs, botKnobs;
    TempoBox tempo;
    StepSequencer seq;
    HeldKeyboard keys;
    int patIndex = 0, shownPattern = -2;

    explicit ArpTab (CohesionSynthAudioProcessor& p) : proc (p), apvts (p.apvts), tempo (p), seq (p), keys (p)
    {
        onBtn.setButtonText ("ON");     addAndMakeVisible (onBtn);    onAtt    = std::make_unique<ButtonAtt> (apvts, pid::arpOn, onBtn);
        syncBtn.setButtonText ("SYNC"); addAndMakeVisible (syncBtn);  syncAtt  = std::make_unique<ButtonAtt> (apvts, pid::arpSync, syncBtn);
        holdBtn.setButtonText ("HOLD"); addAndMakeVisible (holdBtn);  holdAtt  = std::make_unique<ButtonAtt> (apvts, pid::arpHold, holdBtn);
        latchBtn.setButtonText ("LATCH"); addAndMakeVisible (latchBtn); latchAtt = std::make_unique<ButtonAtt> (apvts, pid::arpLatch, latchBtn);

        modeBox.addItemList (pid::arpModes, 1);   addAndMakeVisible (modeBox);  modeAtt  = std::make_unique<ComboAtt> (apvts, pid::arpMode, modeBox);
        orderBox.addItemList (pid::arpOrders, 1); addAndMakeVisible (orderBox); orderAtt = std::make_unique<ComboAtt> (apvts, pid::arpOrder, orderBox);

        auto addK = [&] (juce::OwnedArray<LabeledKnob>& arr, const char* id, const char* nm)
        { auto* k = new LabeledKnob (apvts, id, nm); arr.add (k); addAndMakeVisible (k); };
        addK (topKnobs, pid::arpRate, "RATE"); addK (topKnobs, pid::arpOct, "OCTAVES"); addK (topKnobs, pid::arpGate, "GATE");
        addK (topKnobs, pid::arpSwing, "SWING"); addK (topKnobs, pid::arpRandom, "RANDOM");
        addK (botKnobs, pid::arpHuman, "HUMANIZE"); addK (botKnobs, pid::arpVelAmt, "VEL AMT"); addK (botKnobs, pid::arpSlide, "SLIDE");

        for (auto* b : { &patPrev, &patNext, &stepsDown, &stepsUp, &randomizeBtn })
        { b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1013)); addAndMakeVisible (*b); }
        patPrev.onClick = [this] { applyPattern (patIndex - 1); };
        patNext.onClick = [this] { applyPattern (patIndex + 1); };
        stepsDown.onClick = [this] { arpui::setParam (apvts, pid::arpSteps, juce::jmax (1.0f, arpui::get (apvts, pid::arpSteps) - 1.0f)); markCustom(); };
        stepsUp.onClick   = [this] { arpui::setParam (apvts, pid::arpSteps, juce::jmin (16.0f, arpui::get (apvts, pid::arpSteps) + 1.0f)); markCustom(); };
        randomizeBtn.onClick = [this] { randomize(); };

        for (int i = 0; i < (int) arpPatterns().size(); ++i) patBox.addItem (arpPatterns()[(size_t) i].name, i + 1);
        patBox.setTextWhenNothingSelected ("Custom");
        patBox.onChange = [this] { if (patBox.getSelectedId() > 0 && patBox.getSelectedId() - 1 != shownPattern) applyPattern (patBox.getSelectedId() - 1); };
        addAndMakeVisible (patBox);

        for (auto* l : { &stepsLbl })
        {
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::textColourId, juce::Colour (0xffeafff6));
            l->setColour (juce::Label::backgroundColourId, juce::Colour (0xff0a0c0f));
            l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
            l->setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::plain));
            addAndMakeVisible (*l);
        }
        addAndMakeVisible (tempo); addAndMakeVisible (seq); addAndMakeVisible (keys);
        seq.onEdited = [this] { markCustom(); };
        refresh();
        startTimerHz (30);
    }

    void applyPattern (int idx)
    {
        const auto& pats = arpPatterns();
        const int n = (int) pats.size();
        patIndex = ((idx % n) + n) % n;
        const auto& pt = pats[(size_t) patIndex];
        arpui::setParam (apvts, pid::arpSteps, (float) pt.steps);
        for (int i = 0; i < 16; ++i)
        {
            arpui::setParam (apvts, pid::arpStepOn (i), pt.on[i] == 'x' ? 1.0f : 0.0f);
            arpui::setParam (apvts, pid::arpStepVel (i), pt.vel[i] > 0.0f ? pt.vel[i] : 0.5f);
        }
        if (pt.rate >= 0)     arpui::setParam (apvts, pid::arpRate, (float) pt.rate);
        if (pt.gate >= 0.0f)  arpui::setParam (apvts, pid::arpGate, pt.gate);
        if (pt.swing >= 0.0f) arpui::setParam (apvts, pid::arpSwing, pt.swing);
        shownPattern = patIndex;
        apvts.state.setProperty ("arpPattern", pt.name, nullptr);
        patBox.setSelectedId (patIndex + 1, juce::dontSendNotification);
        seq.repaint();
    }

    void markCustom() { apvts.state.setProperty ("arpPattern", "Custom", nullptr); }

    void randomize()
    {
        juce::Random r;
        for (int i = 0; i < 16; ++i)
        {
            arpui::setParam (apvts, pid::arpStepOn (i), (i == 0 || r.nextFloat() < 0.72f) ? 1.0f : 0.0f);
            arpui::setParam (apvts, pid::arpStepVel (i), 0.35f + 0.65f * r.nextFloat());
        }
        apvts.state.setProperty ("arpPattern", "Random", nullptr);
        seq.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        arpui::card (g, top.toFloat());
        arpui::card (g, bottom.toFloat());
        arpui::caption (g, "MODE",    { modeBox.getX(), top.getY() + 10, 100, 14 });
        arpui::caption (g, "PATTERN", { patPrev.getX(), patPrev.getY() - 16, 100, 14 });
        arpui::caption (g, "TEMPO",   { tempo.getX(), top.getY() + 10, tempo.getWidth(), 14 }, juce::Justification::centred);
        arpui::caption (g, "STEPS",   { stepsDown.getX(), top.getY() + 10, stepsUp.getRight() - stepsDown.getX(), 14 }, juce::Justification::centred);
        g.setColour (juce::Colour (0xff5dcaa5)); g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::bold));
        g.drawText ("HELD CHORD", bottom.getX() + 16, bottom.getY() + 8, 200, 16, juce::Justification::centredLeft);
        arpui::caption (g, "NOTE ORDER", { orderBox.getX(), orderBox.getY() - 16, 120, 14 });
    }

    juce::Rectangle<int> top, bottom;

    void resized() override
    {
        auto r = getLocalBounds();
        top = r.removeFromTop (118); r.removeFromTop (10);
        bottom = r.removeFromBottom (112); r.removeFromBottom (10);
        seq.setBounds (r);

        // --- top card ---
        const int ty = top.getY();
        onBtn.setBounds (top.getX() + 16, ty + 18, 78, 30);
        modeBox.setBounds (top.getX() + 110, ty + 26, 170, 28);
        patPrev.setBounds (top.getX() + 16, ty + 80, 24, 26);
        patBox.setBounds (top.getX() + 44, ty + 80, 212, 26);
        patNext.setBounds (top.getX() + 260, ty + 80, 24, 26);
        for (int i = 0; i < topKnobs.size(); ++i) topKnobs[i]->setBounds (top.getX() + 298 + i * 76, ty + 8, 74, 104);
        const int bx = top.getX() + 298 + 5 * 76 + 8;
        syncBtn.setBounds  (bx, ty + 12, 76, 28);
        holdBtn.setBounds  (bx, ty + 46, 76, 28);
        latchBtn.setBounds (bx, ty + 80, 76, 28);
        const int tx = bx + 88;
        tempo.setBounds (tx, ty + 28, 100, 44);
        const int sx = tx + 114;
        stepsDown.setBounds (sx, ty + 28, 24, 28);
        stepsLbl.setBounds  (sx + 28, ty + 28, 40, 28);
        stepsUp.setBounds   (sx + 72, ty + 28, 24, 28);
        randomizeBtn.setBounds (sx, ty + 74, 96, 30);

        // --- bottom card ---
        keys.setBounds (bottom.getX() + 16, bottom.getY() + 30, 476, 70);
        orderBox.setBounds (bottom.getX() + 540, bottom.getY() + 50, 150, 28);
        for (int i = 0; i < botKnobs.size(); ++i) botKnobs[i]->setBounds (bottom.getRight() - 16 - (3 - i) * 92, bottom.getY() + 8, 88, 100);
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        refresh();
    }

    void refresh()
    {
        const auto name = apvts.state.getProperty ("arpPattern", arpPatterns()[(size_t) patIndex].name).toString();
        int found = -1;
        for (int i = 0; i < (int) arpPatterns().size(); ++i) if (arpPatterns()[(size_t) i].name == name) { found = i; break; }
        if (found != shownPattern || (found < 0 && patBox.getText() != name))
        {
            shownPattern = found;
            if (found >= 0) { patIndex = found; patBox.setSelectedId (found + 1, juce::dontSendNotification); }
            else            { patBox.setSelectedId (0, juce::dontSendNotification); patBox.setText (name, juce::dontSendNotification); }
        }
        const auto st = juce::String ((int) arpui::get (apvts, pid::arpSteps));
        if (stepsLbl.getText() != st) stepsLbl.setText (st, juce::dontSendNotification);
        const bool synced = arpui::get (apvts, pid::arpSync) > 0.5f;
        tempo.setAlpha (synced ? 0.8f : 1.0f);
        const float a = arpui::get (apvts, pid::arpOn) > 0.5f ? 1.0f : 0.45f;    // dim while the arp is off
        for (auto* k : topKnobs) if (std::abs (k->getAlpha() - a) > 0.01f) k->setAlpha (a);
        for (auto* k : botKnobs) if (std::abs (k->getAlpha() - a) > 0.01f) k->setAlpha (a);
        seq.setAlpha (a < 1.0f ? 0.7f : 1.0f);
        seq.repaint(); keys.repaint(); tempo.repaint();
    }
};
