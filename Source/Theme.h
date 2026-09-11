#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

/**
    Tum arayuz renkleri tek yerde.  Yeni bir palet denemek icin Themes::all()
    listesine ekleyip Themes::setCurrent() ile secmek yeterli - baska hicbir
    dosyada renk sabiti yok.
*/
struct Theme
{
    juce::String name;

    juce::Colour background;    // pencere zemini
    juce::Colour panel;         // ust bar, menuler
    juce::Colour plot;          // zarf cizim alaninin zemini
    juce::Colour edge;          // ince cerceveler

    juce::Colour timeAccent;    // zaman zarfi
    juce::Colour volumeAccent;  // ses zarfi

    juce::Colour text;
    juce::Colour textDim;

    juce::Colour playhead;

    juce::Colour filterAccent;  // filtre zarfi - diger iki aksandan ayrisan ucuncu renk
};

namespace Themes
{
    const std::vector<Theme>& all();

    /** Su an secili palet - tum cizim kodu bunu okur. */
    const Theme& current();

    void setCurrent (int index);
    int  getCurrentIndex();

    /** Secilen palet tum projelerde ayni kalsin diye diske yazilir. */
    void loadPreference();
    void savePreference();
}
