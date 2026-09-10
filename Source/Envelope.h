#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/**
    Tek bir zarf (envelope) egrisi: 2 bar'lik pattern boyunca 0..1 arasi bir deger uretir.

    x  : pattern icindeki konum, 0..1  (0 = pattern basi, 1 = 2 bar sonrasi / pattern sonu)
    y  : degerin kendisi, 0..1
         - Time zarfinda  : 0 = canli ses (gecikme yok), 1 = 2 bar geride
         - Volume zarfinda: 0 = sessiz, 1 = tam ses

    Her nokta, KENDISINDEN SONRAKI segmentin seklini tasir:
      stepped == true  -> bir sonraki noktaya kadar degeri sabit tutar (basamak)
      tension          -> -1..1 arasi egri bukumu (0 = duz cizgi)
*/
struct EnvPoint
{
    double x       = 0.0;
    double y       = 0.0;
    double tension = 0.0;
    bool   stepped = false;

    EnvPoint() = default;
    EnvPoint (double xx, double yy, double t = 0.0, bool step = false)
        : x (xx), y (yy), tension (t), stepped (step) {}
};

class Envelope
{
public:
    explicit Envelope (double defaultValue = 0.0);

    /** Pattern fazina (0..1, disinda kalirsa sarilir) karsilik gelen degeri dondurur.
        Segment ikili aramayla bulunur. */
    double valueAt (double phase) const noexcept;

    /** Ayni zarfi sirayla ilerleyen fazlarla sorgulayan cagiranlar (ses thread'i) icin.
        segmentHint en son bulunan segmenti tutar; faz ilerledikce ya ayni segmentte
        kalinir ya bir sonrakine gecilir, yani arama neredeyse hic yapilmaz.
        Ipucu cagirana ait oldugu icin ayni zarf farkli thread'lerden guvenle okunur. */
    double valueAt (double phase, int& segmentHint) const noexcept;

    /** Zarfin sabit (duz cizgi) olup olmadigi - DSP'de bypass kestirmesi icin. */
    bool isFlatAt (double value) const noexcept;

    // --- duzenleme (GUI asamasinda kullanilacak) ---
    void  clearTo (double value);

    /** Zarfi kendi varsayilan degerine dondurur (time -> canli, volume -> tam ses). */
    void  resetToDefault()                           { clearTo (fallback); }

    /** Tum noktalari dikeyde aynalar: y -> 1 - y. */
    void  flipVertical();

    /** Cizim modu: [start, end) hucresini y degerinde bir basamakla doldurur.
        Hucrenin icindeki eski noktalar silinir; hucreden sonra zarf eski seklinden
        devam etsin diye gerekirse hucre sonuna bir donus noktasi konur. */
    void  paintStep (double start, double end, double y);

    /** Butun zarfi zamanda dx kadar kaydirir (pattern sarmali, groove icin).
        Ani sicramalari olusturan ayni-x nokta ciftlerinin sirasi korunur. */
    void  shift (double dx);
    int   addPoint (double x, double y, double tension = 0.0, bool stepped = false);
    void  removePoint (int index);
    void  movePoint (int index, double newX, double newY);
    void  setTension (int index, double tension);
    void  setStepped (int index, bool stepped);

    int   getNumPoints() const noexcept              { return (int) points.size(); }
    const EnvPoint& getPoint (int i) const noexcept  { return points[(size_t) i]; }
    const std::vector<EnvPoint>& getPoints() const noexcept { return points; }
    void  reserve (int numPoints)                    { points.reserve ((size_t) numPoints); }

    void  setPoints (const std::vector<EnvPoint>& newPoints);
    void  setPoints (std::vector<EnvPoint>&& newPoints);

    // --- kalici hale getirme ---
    juce::String toString() const;
    bool fromString (const juce::String& text);

private:
    /** Fazi iceren segmentin baslangic noktasi; -1 = ilk noktadan onceki sarilan bolum. */
    int findSegment (double wrappedPhase) const noexcept;
    bool segmentContains (int index, double wrappedPhase) const noexcept;
    double evaluateSegment (int index, double wrappedPhase) const noexcept;

    void sortPoints();
    void finalisePoints();

    std::vector<EnvPoint> points;
    double fallback = 0.0;
};
