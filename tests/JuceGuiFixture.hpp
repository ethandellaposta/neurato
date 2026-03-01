#pragma once

#include <gtest/gtest.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace ampl
{

class JuceGuiFixture : public ::testing::Test
{
  protected:
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
};

} // namespace ampl
