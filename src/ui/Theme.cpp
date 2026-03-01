#include "Theme.hpp"
using namespace ampl;

namespace ampl
{

// Modern LookAndFeel for Ampl DAW
class AmplLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    AmplLookAndFeel()
    {
        setColourScheme(juce::LookAndFeel_V4::getDarkColourScheme());

        // Override with our modern color scheme
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(ampl::Theme::bgDeepest));
        setColour(juce::TextButton::buttonColourId, juce::Colour(ampl::Theme::bgSurface));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(ampl::Theme::accentBlue));
        setColour(juce::TextButton::textColourOffId, juce::Colour(ampl::Theme::textPrimary));
        setColour(juce::TextButton::textColourOnId, juce::Colour(ampl::Theme::textPrimary));
        setColour(juce::Slider::thumbColourId, juce::Colour(ampl::Theme::accentBlue));
        setColour(juce::Slider::trackColourId, juce::Colour(ampl::Theme::bgElevated));
        setColour(juce::Slider::backgroundColourId, juce::Colour(ampl::Theme::bgSurface));
        setColour(juce::Label::textColourId, juce::Colour(ampl::Theme::textPrimary));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(ampl::Theme::bgSurface));
        setColour(juce::ComboBox::textColourId, juce::Colour(ampl::Theme::textPrimary));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(ampl::Theme::textSecondary));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(ampl::Theme::bgSurface));
        setColour(juce::PopupMenu::textColourId, juce::Colour(ampl::Theme::textPrimary));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,
                  juce::Colour(ampl::Theme::bgHighlight));
        setColour(juce::ScrollBar::thumbColourId, juce::Colour(ampl::Theme::bgElevated));
        setColour(juce::ScrollBar::trackColourId, juce::Colour(ampl::Theme::bgBase));
        setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(ampl::Theme::bgSurface));
        setColour(juce::TooltipWindow::textColourId, juce::Colour(ampl::Theme::textPrimary));
    }

    void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                              const juce::Colour &backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();

        // Modern styling with subtle gradients and shadows
        juce::Colour buttonColour = backgroundColour;

        if (shouldDrawButtonAsDown)
        {
            buttonColour = juce::Colour(ampl::Theme::bgHighlight);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            buttonColour = juce::Colour(ampl::Theme::bgElevated);
        }

        // Draw subtle shadow
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle(bounds.reduced(0, 1).translated(0, 1),
                                       ampl::Theme::cornerRadius);
        g.fillPath(shadowPath);

        // Draw button with gradient
        juce::ColourGradient gradient(buttonColour.brighter(0.18f), 0, 0,
                                      buttonColour.darker(0.20f), 0, bounds.getHeight(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds, ampl::Theme::cornerRadius);

        // Ambient highlight for active/hover states
        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(ampl::Theme::accentBlue)
                            .withAlpha(shouldDrawButtonAsDown ? 0.20f : 0.12f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), ampl::Theme::cornerRadius, 1.2f);
        }

        // Modern border
        g.setColour(juce::Colour(ampl::Theme::borderLight));
        g.drawRoundedRectangle(bounds, ampl::Theme::cornerRadius, 1.0f);
    }
};

// Static instance of our look and feel
static AmplLookAndFeel amplLookAndFeel;

// Theme method implementations
void ampl::Theme::styleButton(juce::Button &button, bool isPrimary, bool isAccent)
{
    button.setLookAndFeel(&amplLookAndFeel);

    if (isPrimary)
    {
        button.setColour(juce::TextButton::buttonColourId, getAccent());
    }
    else if (isAccent)
    {
        button.setColour(juce::TextButton::buttonColourId, getWarning());
    }
    else
    {
        button.setColour(juce::TextButton::buttonColourId, getSurface());
    }

    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(ampl::Theme::bgHighlight));
    button.setColour(juce::TextButton::textColourOffId, getTextPrimary());
    button.setColour(juce::TextButton::textColourOnId, getTextPrimary());
}

void ampl::Theme::styleSlider(juce::Slider &slider)
{
    slider.setLookAndFeel(&amplLookAndFeel);
    slider.setColour(juce::Slider::thumbColourId, getAccent());
    slider.setColour(juce::Slider::trackColourId,
                     juce::Colour(ampl::Theme::accentBlue).withAlpha(0.45f));
    slider.setColour(juce::Slider::backgroundColourId, getSurface());
}

void ampl::Theme::styleLabel(juce::Label &label, bool isHeading)
{
    label.setLookAndFeel(&amplLookAndFeel);
    label.setColour(juce::Label::textColourId, getTextPrimary());

    if (isHeading)
    {
        label.setFont(headingFont());
    }
    else
    {
        label.setFont(bodyFont());
    }
}

void ampl::Theme::styleTextButton(juce::TextButton &button, bool isPrimary)
{
    styleButton(button, isPrimary);
}

void ampl::Theme::styleComboBox(juce::ComboBox &comboBox)
{
    comboBox.setLookAndFeel(&amplLookAndFeel);
    comboBox.setColour(juce::ComboBox::backgroundColourId, getSurface());
    comboBox.setColour(juce::ComboBox::textColourId, getTextPrimary());
    comboBox.setColour(juce::ComboBox::arrowColourId, getTextSecondary());
}

void ampl::Theme::styleProgressBar(juce::ProgressBar &progressBar)
{
    progressBar.setLookAndFeel(&amplLookAndFeel);
    progressBar.setColour(juce::ProgressBar::foregroundColourId, getAccent());
    progressBar.setColour(juce::ProgressBar::backgroundColourId, getElevated());
}

juce::LookAndFeel &ampl::Theme::getLookAndFeel()
{
    return amplLookAndFeel;
}

} // namespace ampl
