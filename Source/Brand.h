#pragma once

#include <juce_graphics/juce_graphics.h>

namespace Brand
{
    juce::Path glyphPath (juce::Rectangle<float> box);

    juce::Point<float> glyphDot (juce::Rectangle<float> box);

    void drawGlyph (juce::Graphics&, juce::Rectangle<float> box, float strokeWidth,
                    juce::Colour lineColour, juce::Colour dotColour, float dotDiameter);
}
