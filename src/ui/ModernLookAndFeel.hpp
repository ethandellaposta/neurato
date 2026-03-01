#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ModernLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    ModernLookAndFeel()
    {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D2D2D));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFF4A90E2));
        setColour(juce::Slider::trackColourId, juce::Colour(0xFFB0B0B0));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xFFF5F5F5));
        setColour(juce::Label::textColourId, juce::Colour(0xFF222222));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFFF5F5F5));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xFF222222));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFFF5F5F5));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xFF222222));
    }

    void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                              const juce::Colour &backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        float cornerSize = 8.0f;
        juce::Colour baseColour = backgroundColour;
        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.2f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.1f);
        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, cornerSize);
        // Subtle shadow
        g.setColour(juce::Colours::black.withAlpha(0.08f));
        g.drawRoundedRectangle(bounds, cornerSize, 2.0f);
    }

    void drawButtonText(juce::Graphics &g, juce::TextButton &button,
                        bool /*shouldDrawButtonAsHighlighted*/,
                        bool /*shouldDrawButtonAsDown*/) override
    {
        auto font = juce::Font(juce::FontOptions(16.0f));
        g.setFont(font);
        g.setColour(button.findColour(juce::TextButton::textColourOnId));
        auto bounds = button.getLocalBounds();
        g.drawFittedText(button.getButtonText(), bounds, juce::Justification::centred, 2);
    }

    void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider &slider) override
    {
        auto trackBounds = juce::Rectangle<float>(x, y + height / 2 - 3, width, 6);
        g.setColour(slider.findColour(juce::Slider::backgroundColourId));
        g.fillRoundedRectangle(trackBounds, 3.0f);
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        auto valueWidth = sliderPos - x;
        g.fillRoundedRectangle(juce::Rectangle<float>(x, y + height / 2 - 3, valueWidth, 6), 3.0f);
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse(sliderPos - 8, y + height / 2 - 8, 16, 16);
    }

    void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider &slider) override
    {
        auto radius = juce::jmin(width, height) / 2.0f - 8.0f;
        auto centreX = x + width / 2.0f;
        auto centreY = y + height / 2.0f;
        float angle =
            rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        // Track
        g.setColour(slider.findColour(juce::Slider::backgroundColourId));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        juce::Path valueArc;
        valueArc.addArc(centreX - radius, centreY - radius, radius * 2, radius * 2,
                        rotaryStartAngle, angle, true);
        g.strokePath(valueArc, juce::PathStrokeType(6.0f));
        // Thumb
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        float thumbX = centreX + radius * std::cos(angle - juce::MathConstants<float>::halfPi);
        float thumbY = centreY + radius * std::sin(angle - juce::MathConstants<float>::halfPi);
        g.fillEllipse(thumbX - 8, thumbY - 8, 16, 16);
    }

    juce::Font getLabelFont(juce::Label &) override
    {
        return juce::Font("Inter", 15.0f, juce::Font::plain);
    }
};
