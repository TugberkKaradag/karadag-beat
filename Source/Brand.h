#pragma once

#include <juce_graphics/juce_graphics.h>

/**
    Karadag Beat logosu: once bir basamak, sonra bir egri - eklentinin iki karakteri
    (time lane'inin basamaklari ve volume lane'inin pump egrisi).  Egrinin tepesinde
    volume renginde bir nokta.

    Eklentinin ust bari, uygulama simgesi ve kurulum sihirbazinin gorselleri ayni
    cizimi kullanir.
*/
namespace Brand
{
    /** Glifin cizgisi, verilen kutuya (en-boy orani ~6:5) oturtulmus. */
    juce::Path glyphPath (juce::Rectangle<float> box);

    /** Egrinin tepesindeki noktanin merkezi. */
    juce::Point<float> glyphDot (juce::Rectangle<float> box);

    /** Glifi cizer: cizgi lineColour ile, nokta dotColour ile. */
    void drawGlyph (juce::Graphics&, juce::Rectangle<float> box, float strokeWidth,
                    juce::Colour lineColour, juce::Colour dotColour, float dotDiameter);
}
