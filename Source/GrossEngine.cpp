#include "GrossEngine.h"
#include <cmath>
#include <vector>

namespace
{
    constexpr int kTaps   = GrossEngine::kSincTaps;
    constexpr int kPhases = 1024;       // kesirli konum cozunurlugu (aralar dogrusal)

    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;

        for (int k = 1; k < 64; ++k)
        {
            const double f = x / (2.0 * k);
            term *= f * f;
            sum  += term;

            if (term < 1.0e-14 * sum)
                break;
        }

        return sum;
    }

    /** Kaiser pencereli sinc cekirdekleri, her kesirli konum icin bir satir.
        Her satirin toplami 1'e normalize - DC kazanci konumdan bagimsiz kalsin.
        Kesim frekansi tam Nyquist: tam sample konumunda cekirdek saf bir darbe
        olur, boylece kesirli ve tam konumlar ayni frekans cevabini paylasir. */
    const std::vector<float>& buildSincTable()
    {
        static const std::vector<float> table = []
        {
            std::vector<float> t ((size_t) (kPhases + 1) * kTaps);

            const double beta = 8.0;
            const double half = kTaps / 2.0;
            const double norm = besselI0 (beta);
            const double pi   = juce::MathConstants<double>::pi;

            for (int p = 0; p <= kPhases; ++p)
            {
                const double frac = (double) p / kPhases;
                double w[kTaps];
                double sum = 0.0;

                for (int k = 0; k < kTaps; ++k)
                {
                    // k. nokta, okuma konumunun tabanina gore -(T/2-1) ... T/2 ofsetinde
                    const double x = (double) (k - (kTaps / 2 - 1)) - frac;
                    const double sinc = std::abs (x) < 1.0e-12 ? 1.0 : std::sin (pi * x) / (pi * x);
                    const double r = x / half;
                    const double win = std::abs (r) >= 1.0 ? 0.0
                                                           : besselI0 (beta * std::sqrt (1.0 - r * r)) / norm;
                    w[k] = sinc * win;
                    sum += w[k];
                }

                for (int k = 0; k < kTaps; ++k)
                    t[(size_t) (p * kTaps + k)] = (float) (w[k] / sum);
            }

            return t;
        }();

        return table;
    }
}

void GrossEngine::prepare (double sampleRate, int channels)
{
    sr          = sampleRate;
    numChannels = juce::jlimit (1, 8, channels);

    // Pattern 4 bar'a kadar cikabiliyor; 40 BPM 4/4'te 4 bar = 24 sn.
    ringLength = (int) (sampleRate * 24.0) + 8;
    ring.setSize (numChannels, ringLength, false, true, true);

    fadeLength    = juce::jmax (16, (int) (sampleRate * 0.004));   // ~4 ms
    jumpThreshold = juce::jmax (8.0, sampleRate * 0.001);          // ~1 ms
    gainStep      = 1.0 / juce::jmax (1.0, sampleRate * 0.002);    // tam salinim 2 ms

    smoothedMix.reset (sampleRate, 0.02);

    sincTable = buildSincTable().data();

    reset();
}

void GrossEngine::reset()
{
    ring.clear();
    writePos     = 0;
    phase        = 0.0;
    prevReadPos  = 0.0;
    havePrevRead = false;
    fadeCounter  = 0;
    fadeReadPos  = 0.0;
    currentGain  = 1.0;
    smoothedMix.setCurrentAndTargetValue (1.0);
}

void GrossEngine::setPatternLengthSamples (double lengthInSamples) noexcept
{
    const double maxLen = (double) (ringLength - 8);
    patternLen = juce::jlimit (1.0, juce::jmax (1.0, maxLen), lengthInSamples);
}

void GrossEngine::setPhase (double newPhase) noexcept
{
    phase = newPhase - std::floor (newPhase);
}

float GrossEngine::readHermite (int channel, double position) const noexcept
{
    const auto* data = ring.getReadPointer (channel);

    const double floored = std::floor (position);
    const double frac    = position - floored;

    int i1 = (int) std::fmod (floored, (double) ringLength);
    if (i1 < 0) i1 += ringLength;

    int i0 = i1 - 1; if (i0 < 0)           i0 += ringLength;
    int i2 = i1 + 1; if (i2 >= ringLength) i2 -= ringLength;
    int i3 = i2 + 1; if (i3 >= ringLength) i3 -= ringLength;

    const double y0 = data[i0];
    const double y1 = data[i1];
    const double y2 = data[i2];
    const double y3 = data[i3];

    // Catmull-Rom / 4 noktali Hermite
    const double c0 = y1;
    const double c1 = 0.5 * (y2 - y0);
    const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

    return (float) ((((c3 * frac) + c2) * frac + c1) * frac + c0);
}

float GrossEngine::readSinc (int channel, double position) const noexcept
{
    const auto* data = ring.getReadPointer (channel);

    const double floored = std::floor (position);
    const double frac    = position - floored;

    int base = (int) std::fmod (floored, (double) ringLength);
    if (base < 0) base += ringLength;

    base -= kTaps / 2 - 1;
    if (base < 0) base += ringLength;

    const double tablePos = frac * kPhases;
    const int    row      = juce::jmin ((int) tablePos, kPhases - 1);
    const float  t        = (float) (tablePos - row);

    const float* c0 = sincTable + row * kTaps;
    const float* c1 = c0 + kTaps;

    double acc = 0.0;
    int idx = base;

    for (int k = 0; k < kTaps; ++k)
    {
        acc += (double) (c0[k] + (c1[k] - c0[k]) * t) * data[idx];

        if (++idx >= ringLength)
            idx = 0;
    }

    return (float) acc;
}

float GrossEngine::readSample (int channel, double position) const noexcept
{
    const double floored = std::floor (position);

    // Tam sample konumu: interpolasyon yok, bypass bit-bit seffaf kalir
    if (position == floored)
    {
        int i = (int) std::fmod (floored, (double) ringLength);
        if (i < 0) i += ringLength;
        return ring.getReadPointer (channel)[i];
    }

    // Sinc cekirdegi okuma noktasindan T/2 sample ileriye bakar.  Yazma kafasina
    // o kadar yakin degilsek sinc, cok yakinsak (gecikme sifirdan yeni kalkiyorsa)
    // yalnizca 2 sample ileriye bakan Hermite'e dus.  Bu bolge birkac sample
    // surer ve ikisi de ayni sesi yakinsadigi icin gecis duyulmaz.
    double dist = (double) writePos - position;
    if (dist < 0.0) dist += (double) ringLength;

    return dist >= (double) (kTaps / 2) ? readSinc (channel, position)
                                        : readHermite (channel, position);
}

void GrossEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                const Envelope& timeEnv,
                                const Envelope& volEnv,
                                bool  timeEnabled,
                                bool  volEnabled,
                                float mix) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int channels   = juce::jmin (numChannels, buffer.getNumChannels());

    if (numSamples <= 0 || channels <= 0 || ringLength <= 0 || patternLen <= 0.0)
        return;

    float* ringPtr[8];
    float* bufPtr[8];

    for (int c = 0; c < channels; ++c)
    {
        ringPtr[c] = ring.getWritePointer (c);
        bufPtr[c]  = buffer.getWritePointer (c);
    }

    const double phaseInc = 1.0 / patternLen;
    const double halfRing = (double) ringLength * 0.5;

    smoothedMix.setTargetValue (juce::jlimit (0.0, 1.0, (double) mix));

    for (int i = 0; i < numSamples; ++i)
    {
        // 1) canli girisi halka buffer'a yaz
        for (int c = 0; c < channels; ++c)
            ringPtr[c][writePos] = bufPtr[c][i];

        // 2) zarflari bu faz noktasinda degerlendir
        const double timeVal    = timeEnabled ? timeEnv.valueAt (phase, timeSegment) : 0.0;
        const double targetGain = volEnabled  ? volEnv .valueAt (phase, volSegment)  : 1.0;

        currentGain += juce::jlimit (-gainStep, gainStep, targetGain - currentGain);

        const double gain = currentGain;
        const double wet  = smoothedMix.getNextValue();
        const double dry  = 1.0 - wet;

        // 3) kesirli okuma konumu
        //
        // Hermite 4 nokta kullanir ve okuma noktasindan 2 sample ILERIYE bakar.
        // Gecikme tam 0 iken frac de 0 olur, egri dogrudan yazilan sample'i verir
        // ve bypass bit-bit seffaf kalir.  Ama gecikme 0 ile 2 sample arasinda ve
        // kesirli oldugunda, ileri bakan noktalar halka buffer'da henuz
        // uzerine yazilmamis (bir tur onceki) sesi gosterir - bozulma ve tasma.
        // Bu dar araligi atlayarak her iki durumu da dogru tutuyoruz.
        double delaySamples = timeVal * patternLen;

        if (delaySamples > 0.0 && delaySamples < 2.0)
            delaySamples = 2.0;

        const double readPos = (double) writePos - delaySamples;

        // 4) zarfta ani sicrama var mi? varsa crossfade baslat
        if (havePrevRead)
        {
            const double expected = prevReadPos + 1.0;
            double diff = readPos - expected;

            while (diff >  halfRing) diff -= (double) ringLength;
            while (diff < -halfRing) diff += (double) ringLength;

            if (std::abs (diff) > jumpThreshold)
            {
                fadeReadPos = expected;      // eski kafa yoluna devam etsin
                fadeCounter = fadeLength;
            }
        }

        // 5) oku, gerekiyorsa iki kafayi esit guclu crossfade ile birlestir
        double gOld = 0.0, gNew = 1.0;

        if (fadeCounter > 0)
        {
            // Dogrusal gecis: iki kafa ayni sesi tasidiginda esit-guclu
            // egri +3 dB tasma yapardi, boyle tepe degeri hep <= 1 kalir.
            gNew = 1.0 - ((double) fadeCounter / (double) fadeLength);
            gOld = 1.0 - gNew;
        }

        for (int c = 0; c < channels; ++c)
        {
            double wetSample = readSample (c, readPos);

            if (fadeCounter > 0)
                wetSample = readSample (c, fadeReadPos) * gOld + wetSample * gNew;

            const double drySample = bufPtr[c][i];
            bufPtr[c][i] = (float) (drySample * dry + wetSample * gain * wet);
        }

        if (fadeCounter > 0)
        {
            fadeReadPos += 1.0;
            --fadeCounter;
        }

        prevReadPos  = readPos;
        havePrevRead = true;

        if (++writePos >= ringLength)
            writePos = 0;

        phase += phaseInc;
        if (phase >= 1.0)
            phase -= std::floor (phase);
    }
}
