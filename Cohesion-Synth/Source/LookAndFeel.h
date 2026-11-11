#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Shared dark / teal house style (matches the Cohesion suite), with a vertical
// mixer-fader style added for the synth.
class CohesionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CohesionLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffdfe6e2));
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId,      juce::Colour (0xff0e1013));
        setColour (juce::ComboBox::textColourId,            juce::Colour (0xffdfe6e2));
        setColour (juce::ComboBox::outlineColourId,         juce::Colours::transparentBlack);   // no frame around values
        setColour (juce::ComboBox::arrowColourId,           juce::Colour (0xff9fe1cb));
        setColour (juce::PopupMenu::backgroundColourId,     juce::Colour (0xff15181c));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff0f6e56));
        setColour (juce::PopupMenu::textColourId,           juce::Colour (0xffdfe6e2));
        setColour (juce::TextButton::buttonColourId,        juce::Colour (0xff0e1013));
        setColour (juce::TextButton::textColourOffId,       juce::Colour (0xff9fe1cb));
        setColour (juce::ScrollBar::thumbColourId,          juce::Colour (0xff39ffb0).withAlpha (0.45f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float rotStart, float rotEnd, juce::Slider&) override
    {
        const float side = (float) juce::jmin (w, h);   // always round, whatever the box shape
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).withSizeKeepingCentre (side, side).reduced (4.0f);
        const float cx = r.getCentreX(), cy = r.getCentreY();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const float ang = rotStart + sliderPos * (rotEnd - rotStart);
        g.setColour (juce::Colour (0xff101317)); g.fillEllipse (r);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawEllipse (r, 1.0f);
        juce::Path track;
        track.addCentredArc (cx, cy, rad - 3.0f, rad - 3.0f, 0.0f, rotStart, rotEnd, true);
        g.setColour (juce::Colour (0xff2a2f35));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path arc;
        arc.addCentredArc (cx, cy, rad - 3.0f, rad - 3.0f, 0.0f, rotStart, ang, true);
        g.setColour (juce::Colour (0xff39ffb0));
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float pr = rad - 9.0f;
        g.setColour (juce::Colour (0xffdfe6e2));
        g.drawLine (cx, cy, cx + pr * std::sin (ang), cy - pr * std::cos (ang), 2.5f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           const juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            const float cx = (float) x + (float) w * 0.5f;
            juce::Rectangle<float> track (cx - 5.0f, (float) y, 10.0f, (float) h);
            g.setColour (juce::Colour (0xff13181d)); g.fillRoundedRectangle (track, 5.0f);
            const float thumbY = sliderPos;
            juce::Rectangle<float> fill (cx - 5.0f, thumbY, 10.0f, (float) (y + h) - thumbY);
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.8f)); g.fillRoundedRectangle (fill, 5.0f);
            g.setColour (juce::Colour (0xff3a4149)); g.fillEllipse (cx - 7.0f, thumbY - 7.0f, 14.0f, 14.0f);
            g.setColour (juce::Colour (0xff11161b)); g.drawEllipse (cx - 7.0f, thumbY - 7.0f, 14.0f, 14.0f, 1.5f);
            return;
        }
        if (style == juce::Slider::LinearHorizontal)
        {
            const float cy = (float) y + (float) h * 0.5f;
            juce::Rectangle<float> track ((float) x, cy - 3.0f, (float) w, 6.0f);
            g.setColour (juce::Colour (0xff13181d)); g.fillRoundedRectangle (track, 3.0f);
            // bipolar ranges fill from zero, others from the left edge
            float from = (float) x;
            if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0) from = (float) s.getPositionOfValue (0.0);
            const float a = juce::jmin (from, sliderPos), b = juce::jmax (from, sliderPos);
            g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.8f)); g.fillRoundedRectangle (a, cy - 3.0f, juce::jmax (2.0f, b - a), 6.0f, 3.0f);
            g.setColour (juce::Colour (0xff3a4149)); g.fillEllipse (sliderPos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
            g.setColour (juce::Colour (0xff11161b)); g.drawEllipse (sliderPos - 6.0f, cy - 6.0f, 12.0f, 12.0f, 1.5f);
            return;
        }
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
    }

    // Buttons keep their own subtle teal frame (V4 would borrow the combo-box outline colour).
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = bg.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f);
        if (down || over) c = c.contrasting (down ? 0.2f : 0.05f);
        g.setColour (c); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (0xff39ffb0).withAlpha (0.30f)); g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        g.setColour (on ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? juce::Colour (0xff39ffb0) : juce::Colour (0xff2b3138));
        g.drawRoundedRectangle (r.reduced (0.6f), 5.0f, 1.0f);
        g.setColour (on ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        g.setFont (juce::Font ("Segoe UI", 10.0f, juce::Font::plain));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::backgroundColourId,            juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId,               juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff0e1013));
        l->setColour (juce::Label::outlineWhenEditingColourId,    juce::Colour (0xff39ffb0));
        l->setColour (juce::Label::textColourId,                  juce::Colour (0xff9fe1cb));
        l->setFont (juce::Font ("Segoe UI", 12.5f, juce::Font::plain));
        return l;
    }
};
