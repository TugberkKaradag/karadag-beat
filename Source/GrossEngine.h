#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include "Envelope.h"

/**
    Gross Beat tarzi zaman/ses manipulasyonu motoru.

    Calisma mantigi:
      - Gelen ses surekli olarak bir halka (ring) buffer'a yazilir.
      - Her sample icin time zarfi "ne kadar geriden okuyalim" degerini verir
        (0 = canli, 1 = tam bir pattern boyu geride).
      - Okuma kafasi kesirli konumda gezdigi icin zarfin EGIMI hizi/perdeyi belirler:
            egim  0   -> sabit gecikme, normal hiz
            egim +1   -> okuma kafasi yerinde sayar  -> FREEZE
            egim >0   -> yavaslar / perde duser
            egim <0   -> hizlanir  / perde yukselir
      - Zarfta ani sicrama olursa iki okuma kafasi arasinda kisa bir
        crossfade yapilir, boylece basamakli pattern'lerde tik sesi olmaz.
*/
class GrossEngine
{
public:
    GrossEngine() = default;

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Pattern'in sample cinsinden uzunlugu (tempoya gore her blokta guncellenir). */
    void setPatternLengthSamples (double lengthInSamples) noexcept;
    double getPatternLengthSamples() const noexcept { return patternLen; }

    /** Lane basina yumusatma suresi (ms).
        timeMs   : time zarfindaki sicramalarda iki okuma kafasi arasindaki capraz gecis.
        volumeMs : gain'in 0'dan 1'e en hizli gecis suresi (slew limiter). */
    void setSmoothing (double timeMs, double volumeMs) noexcept;

    /** Host'un transport konumundan faz senkronu. */
    void setPhase (double newPhase) noexcept;
    double getPhase() const noexcept { return phase; }

    /** Filtre lane'inin ayarlari.
        highPass  : false = low-pass, true = high-pass
        resonance : 0..1  (Q 0.7 ... ~8.5)
        smoothMs  : kesim frekansinin tamamen acik ile tamamen kapali arasindaki
                    en hizli gecis suresi - basamakli filtre zarflarinda tiki onler. */
    void setFilter (bool highPass, double resonance, double smoothMs) noexcept;

    /** Filtresiz calma: filtre lane'i kapali sayilir. */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       const Envelope& timeEnv,
                       const Envelope& volEnv,
                       bool  timeEnabled,
                       bool  volEnabled,
                       float mix) noexcept;

    /** Uc lane birlikte.  Filtre, islenmis (wet) sese gain'den sonra uygulanir;
        lane tamamen acikken cikis filtresiz halle bit-bit aynidir. */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       const Envelope& timeEnv,
                       const Envelope& volEnv,
                       const Envelope& filterEnv,
                       bool  timeEnabled,
                       bool  volEnabled,
                       bool  filterEnabled,
                       float mix) noexcept;

    /** Gelen sesin pattern boyunca dagilimi: pattern kWaveBins dilime bolunur,
        her dilimde girisin tepe degeri tutulur.  Arayuz bunu lane'lerin arkasina
        cizer - neyi stutter'ladigini, kick'in nereye dustugunu gormek icin.
        Ses thread'i yazar, GUI okur; ikisi de kilitsiz (atomik). */
    static constexpr int kWaveBins = 512;
    float getWavePeak (int bin) const noexcept
    {
        return wavePeaks[(size_t) juce::jlimit (0, kWaveBins - 1, bin)].load (std::memory_order_relaxed);
    }

    /** Sinc cekirdeginin nokta sayisi.  Okuma noktasinin yarisi kadar ileriye bakar. */
    static constexpr int kSincTaps = 32;

    /** Hiza bagli kesim bantlari: bant b, okuma hizi 1 + b/4 icin (1.0x ... 4.0x).
        Hizli okurken frekanslar yukari kayar; Nyquist'i asacak bilesenler, cekirdegin
        kesim frekansi 1/hiz'a daraltilarak okuma sirasinda suzuluyor. */
    static constexpr int kSincBands = 13;

private:
    /** Konuma gore en uygun interpolasyonu secer (tam sample / sinc / Hermite). */
    float readSample  (int channel, double position, int band) const noexcept;
    float readSinc    (int channel, double position, int band) const noexcept;
    float readHermite (int channel, double position) const noexcept;

    const float* sincTable = nullptr;

    juce::AudioBuffer<float> ring;
    int    ringLength  = 0;
    int    writePos    = 0;
    int    numChannels = 0;
    double sr          = 44100.0;

    /** Filtre katsayilarini o anki kapanma miktarina gore yeniden hesaplar. */
    void updateFilterCoefficients() noexcept;

    double phase      = 0.0;
    double patternLen = 0.0;

    std::array<std::atomic<float>, kWaveBins> wavePeaks {};
    int   waveBin  = -1;
    float wavePeak = 0.0f;

    // Zarf aramasi icin segment ipuclari (bkz. Envelope::valueAt)
    int timeSegment   = 0;
    int volSegment    = 0;
    int filterSegment = 0;

    // Volume tarafi icin tik onleme.  Gain bir slew limiter'dan geciyor: yumusak
    // egrilere (pump, fade) dokunmuyor, yalnizca ~2 ms'den hizli sicramalari -
    // gate kenarlari, VOLUME anahtari, MIDI ile pattern degisimi - rampaya ceviriyor.
    double currentGain = 1.0;
    double gainStep    = 0.0;

    // Filtre: TPT state-variable (Zavalishin).  "closed" 0 = acik, 1 = kapali;
    // gain gibi dogrusal bir slew limiter'dan gecer.  Tam acikken filtre cikisi
    // karisima hic girmez, bypass bit-bit seffaf kalir.
    bool   filterHighPass  = false;
    double filterK         = 1.414;     // 1/Q
    double filterClosed    = 0.0;
    double filterStep      = 0.0;
    double coefClosed      = -1.0;      // katsayilarin hesaplandigi kapanma
    double fa1 = 0.0, fa2 = 0.0, fa3 = 0.0;
    double fIc1[8] {}, fIc2[8] {};
    const Envelope openFilter { 1.0 };

    // Mix blok basina bir kez geliyor; ornek basina rampa ile uygulaniyor.
    juce::SmoothedValue<double> smoothedMix;

    // kesintisiz okuma / crossfade durumu
    double prevReadPos  = 0.0;
    double readSpeed    = 1.0;      // okuma kafasinin yumusatilmis hizi (sample/sample)
    bool   havePrevRead = false;
    double fadeReadPos  = 0.0;
    int    fadeCounter  = 0;
    int    fadeLength   = 0;
    double jumpThreshold = 0.0;
};
