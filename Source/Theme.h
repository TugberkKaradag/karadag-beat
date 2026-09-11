#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

struct Theme
{
    juce::String name;

    juce::Colour background;
    juce::Colour panel;
    juce::Colour plot;
    juce::Colour edge;

    juce::Colour timeAccent;
    juce::Colour volumeAccent;

    juce::Colour text;
    juce::Colour textDim;

    juce::Colour playhead;

    juce::Colour filterAccent;
};

namespace Themes
{
    const std::vector<Theme>& all();

    const Theme& current();

    void setCurrent (int index);
    int  getCurrentIndex();

    void loadPreference();
    void savePreference();
}
