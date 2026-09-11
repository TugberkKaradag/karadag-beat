#pragma once

#include <vector>
#include "Envelope.h"

/**
    Swing: pattern'deki her 1/16'lik cifti esit olmayan iki parcaya boler.
    amount = ciftin ilk yarisinin payi: 0.5 duz, 0.66 triole yakin, 0.75 en sert.

    Iki zaman ekseni var:
      pattern zamani : zarfin cizildigi duz izgara (editorde gorunen)
      gercek zaman   : sesin calindigi, cift sayili olmayan 1/16'liklarin
                       ileri itildigi eksen
    Cift sayili 1/16'liklar (her vurus, her 1/8) yerinden oynamaz.

    Zarflar calmadan once bir kez donusturulur, ornek basina ek is yoktur.
      Volume / filtre: noktalar yalnizca zamanda kayar.
      Time           : noktalar kayar VE gecikme, okunan kaynak konumu da ayni
                       sekilde kayacak bicimde yeniden hesaplanir.  Boylece bir
                       repeat basamagi, swing'li adimda bile kaynak adimin tam
                       basindan (transient'i kesmeden) ve normal hizda calar.
*/
namespace Swing
{
    constexpr double kStraight = 0.5;
    constexpr double kMax      = 0.75;

    /** Pattern zamanindaki bir noktanin gercek zamandaki yeri (0..1). */
    double patternToReal (double u, int cells, double amount) noexcept;

    /** Tersi: gercek zaman -> pattern zamani.  Calan kafayi editorde dogru yere cizmek icin. */
    double realToPattern (double t, int cells, double amount) noexcept;

    /** Swing etkin mi - duz ya da cift sayisi yetersizse donusum atlanir. */
    bool isActive (int cells, double amount) noexcept;

    /** src'yi swing'leyip dst'ye yazar.  dst onceden yer ayrilmissa bellek ayirmaz. */
    void apply (const std::vector<EnvPoint>& src, std::vector<EnvPoint>& dst,
                int cells, double amount, bool isTimeLane);
}
