#include "Brand.h"

juce::Path Brand::glyphPath (juce::Rectangle<float> box)
{
    const float x = box.getX(), y = box.getY();
    const float w = box.getWidth(), h = box.getHeight();

    juce::Path mark;
    mark.startNewSubPath (x,             y + h * 0.70f);
    mark.lineTo          (x + w * 0.22f, y + h * 0.70f);
    mark.lineTo          (x + w * 0.22f, y + h * 0.26f);
    mark.lineTo          (x + w * 0.46f, y + h * 0.26f);
    mark.lineTo          (x + w * 0.46f, y + h * 0.90f);
    mark.lineTo          (x + w * 0.62f, y + h * 0.90f);
    mark.quadraticTo     (x + w,         y + h * 0.90f,
                          x + w,         y + h * 0.08f);
    return mark;
}

juce::Point<float> Brand::glyphDot (juce::Rectangle<float> box)
{
    return { box.getRight(), box.getY() + box.getHeight() * 0.08f };
}

void Brand::drawGlyph (juce::Graphics& g, juce::Rectangle<float> box, float strokeWidth,
                       juce::Colour lineColour, juce::Colour dotColour, float dotDiameter)
{
    g.setColour (lineColour);
    g.strokePath (glyphPath (box), juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

    g.setColour (dotColour);
    g.fillEllipse (juce::Rectangle<float> (dotDiameter, dotDiameter).withCentre (glyphDot (box)));
}
