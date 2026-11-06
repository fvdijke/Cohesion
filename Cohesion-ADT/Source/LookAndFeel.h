#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Shared dark / teal house style (matches the Cohesion suite).
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
        setColour (juce::ComboBox::outlineColourId,         juce::Colour (0xff39ffb0).withAlpha (0.40f));
        setColour (juce::ComboBox::arrowColourId,           juce::Colour (0xff9fe1cb));
        setColour (juce::PopupMenu::backgroundColourId,     juce::Colour (0xff15181c));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff0f6e56));
        setColour (juce::PopupMenu::textColourId,           juce::Colour (0xffdfe6e2));
        setColour (juce::TextButton::buttonColourId,        juce::Colour (0xff0e1013));
        setColour (juce::TextButton::textColourOffId,       juce::Colour (0xff9fe1cb));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float, float,
                           const juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float trackY = y + h * 0.5f;
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle ((float) x, trackY - 2.0f, (float) w, 4.0f, 2.0f);
        g.setColour (juce::Colour (0xff39ffb0));
        g.fillRoundedRectangle ((float) x, trackY - 2.0f, juce::jmax (0.0f, sliderPos - (float) x), 4.0f, 2.0f);
        g.setColour (juce::Colour (0xffeef4f1));
        g.fillEllipse (sliderPos - 7.0f, trackY - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawEllipse (sliderPos - 7.0f, trackY - 7.0f, 14.0f, 14.0f, 1.0f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float rotStart, float rotEnd, juce::Slider&) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float cx = r.getCentreX(), cy = r.getCentreY();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const float ang = rotStart + sliderPos * (rotEnd - rotStart);
        g.setColour (juce::Colour (0xff101317)); g.fillEllipse (r);
        g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawEllipse (r, 1.0f);
        juce::Path arc;
        arc.addCentredArc (cx, cy, rad - 2.5f, rad - 2.5f, 0.0f, rotStart, ang, true);
        g.setColour (juce::Colour (0xff39ffb0));
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float pr = rad - 7.0f;
        g.setColour (juce::Colour (0xffdfe6e2));
        g.drawLine (cx, cy, cx + pr * std::sin (ang), cy - pr * std::cos (ang), 2.5f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        g.setColour (on ? juce::Colour (0xff0f6e56) : juce::Colour (0xff0e1013));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? juce::Colour (0xff39ffb0) : juce::Colour (0xff39ffb0).withAlpha (0.30f));
        g.drawRoundedRectangle (r.reduced (0.6f), 6.0f, 1.0f);
        auto led = r.removeFromLeft (juce::jmin (r.getHeight(), 20.0f));
        g.setColour (on ? juce::Colour (0xff39ffb0) : juce::Colours::white.withAlpha (0.20f));
        g.fillEllipse (led.withSizeKeepingCentre (8.0f, 8.0f));
        g.setColour (on ? juce::Colour (0xffeafff6) : juce::Colour (0xff9fb0aa));
        g.setFont (juce::Font ("Segoe UI", 12.0f, juce::Font::plain));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (2.0f), juce::Justification::centred);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::backgroundColourId,            juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId,               juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff0e1013));
        l->setColour (juce::Label::outlineWhenEditingColourId,    juce::Colour (0xff39ffb0));
        l->setColour (juce::Label::textColourId,                  juce::Colour (0xffdfe6e2));
        l->setFont (juce::Font ("Segoe UI", 11.0f, juce::Font::plain));
        return l;
    }
};
