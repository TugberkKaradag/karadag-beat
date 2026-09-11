#pragma once

#include <cmath>

/**
    Filtre lane'inin degerini kesim frekansina cevirir.  DSP ve arayuzun olcek
    etiketleri ayni fonksiyonu kullansin diye ayri, bagimsiz bir baslik.

    Lane degeri 0..1: ust (1) = filtre acik, alt (0) = tamamen kapali.
      Low-pass : acikken 20 kHz, kapandikca 40 Hz'e iner
      High-pass: acikken 20 Hz,  kapandikca 12 kHz'e cikar
    Aralik logaritmik - kulak frekansi boyle duyuyor.
*/
namespace FilterMap
{
    inline double cutoffHz (double laneValue, bool highPass) noexcept
    {
        const double v = laneValue < 0.0 ? 0.0 : (laneValue > 1.0 ? 1.0 : laneValue);
        const double closed = 1.0 - v;

        return highPass ? 20.0    * std::pow (12000.0 / 20.0, closed)
                        : 20000.0 * std::pow (40.0 / 20000.0, closed);
    }

    /** "12k", "2.4k", "380" gibi kisa etiket metni icin sayi. */
    inline double roundedForLabel (double hz) noexcept
    {
        if (hz >= 1000.0)
            return std::round (hz / 100.0) / 10.0;     // kHz, bir ondalik

        return std::round (hz);
    }
}
