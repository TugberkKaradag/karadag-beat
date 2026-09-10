#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Envelope.h"

/**
    Gross Beat tarzi zaman/ses manipulasyonu motoru.

    Calisma mantigi:
      - Gelen ses surekli olarak bir halka (ring) buffer'a yazilir.
      - Her sample icin time zarfi "ne kadar geriden okuyalim" degerini verir
        (0 = canli, 1 = tam 2 bar geride).
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

    /** 2 bar'lik pattern'in sample cinsinden uzunlugu (tempoya gore her blokta guncellenir). */
    void setPatternLengthSamples (double lengthInSamples) noexcept;

    /** Host'un transport konumundan faz senkronu. */
    void setPhase (double newPhase) noexcept;
    double getPhase() const noexcept { return phase; }

    void processBlock (juce::AudioBuffer<float>& buffer,
                       const Envelope& timeEnv,
                       const Envelope& volEnv,
                       bool  timeEnabled,
                       bool  volEnabled,
                       float mix) noexcept;

    /** Sinc cekirdeginin nokta sayisi.  Okuma noktasinin yarisi kadar ileriye bakar. */
    static constexpr int kSincTaps = 16;

private:
    /** Konuma gore en uygun interpolasyonu secer (tam sample / sinc / Hermite). */
    float readSample  (int channel, double position) const noexcept;
    float readSinc    (int channel, double position) const noexcept;
    float readHermite (int channel, double position) const noexcept;

    const float* sincTable = nullptr;

    juce::AudioBuffer<float> ring;
    int    ringLength  = 0;
    int    writePos    = 0;
    int    numChannels = 0;
    double sr          = 44100.0;

    double phase      = 0.0;
    double patternLen = 0.0;

    // Volume tarafi icin tik onleme.  Gain bir slew limiter'dan geciyor: yumusak
    // egrilere (pump, fade) dokunmuyor, yalnizca ~2 ms'den hizli sicramalari -
    // gate kenarlari, VOLUME anahtari, MIDI ile pattern degisimi - rampaya ceviriyor.
    // Zarf aramasi icin segment ipuclari (bkz. Envelope::valueAt)
    int timeSegment = 0;
    int volSegment  = 0;

    double currentGain = 1.0;
    double gainStep    = 0.0;

    // Mix blok basina bir kez geliyor; ornek basina rampa ile uygulaniyor.
    juce::SmoothedValue<double> smoothedMix;

    // kesintisiz okuma / crossfade durumu
    double prevReadPos  = 0.0;
    bool   havePrevRead = false;
    double fadeReadPos  = 0.0;
    int    fadeCounter  = 0;
    int    fadeLength   = 0;
    double jumpThreshold = 0.0;
};
