#include "BeatEngine.h"
#include "FilterMap.h"
#include <cmath>
#include <vector>

namespace
{
    constexpr int kTaps   = BeatEngine::kSincTaps;
    constexpr int kBands  = BeatEngine::kSincBands;
    constexpr int kPhases = 1024;
    constexpr int kBandStride = (kPhases + 1) * kTaps;

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

    const std::vector<float>& buildSincTable()
    {
        static const std::vector<float> table = []
        {
            std::vector<float> t ((size_t) kBands * kBandStride);

            const double beta = 8.0;
            const double half = kTaps / 2.0;
            const double norm = besselI0 (beta);
            const double pi   = juce::MathConstants<double>::pi;

            for (int b = 0; b < kBands; ++b)
            {
                const double cutoff = 1.0 / (1.0 + 0.25 * b);

                for (int p = 0; p <= kPhases; ++p)
                {
                    const double frac = (double) p / kPhases;
                    double w[kTaps];
                    double sum = 0.0;

                    for (int k = 0; k < kTaps; ++k)
                    {
                        const double x  = (double) (k - (kTaps / 2 - 1)) - frac;
                        const double cx = cutoff * x;
                        const double sinc = std::abs (cx) < 1.0e-12 ? 1.0 : std::sin (pi * cx) / (pi * cx);
                        const double r = x / half;
                        const double win = std::abs (r) >= 1.0 ? 0.0
                                                               : besselI0 (beta * std::sqrt (1.0 - r * r)) / norm;
                        w[k] = sinc * win;
                        sum += w[k];
                    }

                    for (int k = 0; k < kTaps; ++k)
                        t[(size_t) (b * kBandStride + p * kTaps + k)] = (float) (w[k] / sum);
                }
            }

            return t;
        }();

        return table;
    }
}

void BeatEngine::prepare (double sampleRate, int channels)
{
    sr          = sampleRate;
    numChannels = juce::jlimit (1, 8, channels);

    ringLength = (int) (sampleRate * 24.0) + 8;
    ring.setSize (numChannels, ringLength, false, true, true);

    fadeLength    = juce::jmax (16, (int) (sampleRate * 0.004));
    jumpThreshold = juce::jmax (8.0, sampleRate * 0.001);
    gainStep      = 1.0 / juce::jmax (1.0, sampleRate * 0.002);
    filterStep    = 1.0 / juce::jmax (1.0, sampleRate * 0.005);

    smoothedMix.reset (sampleRate, 0.02);

    sincTable = buildSincTable().data();

    reset();
}

void BeatEngine::reset()
{
    ring.clear();
    writePos     = 0;
    phase        = 0.0;
    prevReadPos  = 0.0;
    readSpeed    = 1.0;
    havePrevRead = false;
    fadeCounter  = 0;
    fadeReadPos  = 0.0;
    currentGain  = 1.0;
    smoothedMix.setCurrentAndTargetValue (1.0);

    filterClosed = 0.0;
    coefClosed   = -1.0;

    for (int c = 0; c < 8; ++c)
        fIc1[c] = fIc2[c] = 0.0;

    for (auto& p : wavePeaks)
        p.store (0.0f, std::memory_order_relaxed);

    waveBin  = -1;
    wavePeak = 0.0f;
}

void BeatEngine::setPatternLengthSamples (double lengthInSamples) noexcept
{
    const double maxLen = (double) (ringLength - 8);
    patternLen = juce::jlimit (1.0, juce::jmax (1.0, maxLen), lengthInSamples);
}

void BeatEngine::setSmoothing (double timeMs, double volumeMs) noexcept
{
    fadeLength = juce::jmax (16, (int) (sr * juce::jlimit (0.5, 200.0, timeMs) * 0.001));
    gainStep   = 1.0 / juce::jmax (1.0, sr * juce::jlimit (0.5, 200.0, volumeMs) * 0.001);

    fadeCounter = juce::jmin (fadeCounter, fadeLength);
}

void BeatEngine::setFilter (bool highPass, double resonance, double smoothMs) noexcept
{
    const double q = 0.7071 * std::pow (12.0, juce::jlimit (0.0, 1.0, resonance));
    const double k = 1.0 / q;

    if (highPass != filterHighPass || std::abs (k - filterK) > 1.0e-9)
    {
        filterHighPass = highPass;
        filterK        = k;
        coefClosed     = -1.0;
    }

    filterStep = 1.0 / juce::jmax (1.0, sr * juce::jlimit (0.5, 200.0, smoothMs) * 0.001);
}

void BeatEngine::updateFilterCoefficients() noexcept
{
    const double hz = juce::jmin (FilterMap::cutoffHz (1.0 - filterClosed, filterHighPass), sr * 0.45);
    const double g  = std::tan (juce::MathConstants<double>::pi * hz / sr);

    fa1 = 1.0 / (1.0 + g * (g + filterK));
    fa2 = g * fa1;
    fa3 = g * fa2;

    coefClosed = filterClosed;
}

void BeatEngine::setPhase (double newPhase) noexcept
{
    phase = newPhase - std::floor (newPhase);
}

float BeatEngine::readHermite (int channel, double position) const noexcept
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

    const double c0 = y1;
    const double c1 = 0.5 * (y2 - y0);
    const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

    return (float) ((((c3 * frac) + c2) * frac + c1) * frac + c0);
}

float BeatEngine::readSinc (int channel, double position, int band) const noexcept
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

    const float* c0 = sincTable + band * kBandStride + row * kTaps;
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

float BeatEngine::readSample (int channel, double position, int band) const noexcept
{
    const double floored = std::floor (position);

    if (band == 0 && position == floored)
    {
        int i = (int) std::fmod (floored, (double) ringLength);
        if (i < 0) i += ringLength;
        return ring.getReadPointer (channel)[i];
    }

    double dist = (double) writePos - position;
    if (dist < 0.0) dist += (double) ringLength;

    return dist >= (double) (kTaps / 2) ? readSinc (channel, position, band)
                                        : readHermite (channel, position);
}

void BeatEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                const Envelope& timeEnv,
                                const Envelope& volEnv,
                                bool  timeEnabled,
                                bool  volEnabled,
                                float mix) noexcept
{
    processBlock (buffer, timeEnv, volEnv, openFilter, timeEnabled, volEnabled, false, mix);
}

void BeatEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                const Envelope& timeEnv,
                                const Envelope& volEnv,
                                const Envelope& filterEnv,
                                bool  timeEnabled,
                                bool  volEnabled,
                                bool  filterEnabled,
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
        float inputPeak = 0.0f;

        for (int c = 0; c < channels; ++c)
        {
            ringPtr[c][writePos] = bufPtr[c][i];
            inputPeak = juce::jmax (inputPeak, std::abs (bufPtr[c][i]));
        }

        const int bin = juce::jlimit (0, kWaveBins - 1, (int) (phase * kWaveBins));

        if (bin != waveBin)
        {
            if (waveBin >= 0)
                wavePeaks[(size_t) waveBin].store (wavePeak, std::memory_order_relaxed);

            waveBin  = bin;
            wavePeak = 0.0f;
        }

        wavePeak = juce::jmax (wavePeak, inputPeak);

        const double timeVal    = timeEnabled ? timeEnv.valueAt (phase, timeSegment) : 0.0;
        const double targetGain = volEnabled  ? volEnv .valueAt (phase, volSegment)  : 1.0;

        currentGain += juce::jlimit (-gainStep, gainStep, targetGain - currentGain);

        const double gain = currentGain;
        const double wet  = smoothedMix.getNextValue();
        const double dry  = 1.0 - wet;

        const double targetClosed = filterEnabled ? 1.0 - filterEnv.valueAt (phase, filterSegment) : 0.0;
        filterClosed += juce::jlimit (-filterStep, filterStep, targetClosed - filterClosed);

        const bool runFilter = filterEnabled || filterClosed > 0.0;

        double filterBlend = 0.0;

        if (runFilter)
        {
            if (filterClosed != coefClosed)
                updateFilterCoefficients();

            filterBlend = juce::jmin (1.0, filterClosed / 0.03);
        }
        else if (coefClosed >= 0.0)
        {
            for (int c = 0; c < channels; ++c)
                fIc1[c] = fIc2[c] = 0.0;

            coefClosed = -1.0;
        }

        double delaySamples = timeVal * patternLen;

        if (delaySamples > 0.0 && delaySamples < 2.0)
            delaySamples = 2.0;

        const double readPos = (double) writePos - delaySamples;

        if (havePrevRead)
        {
            const double expected = prevReadPos + 1.0;
            double diff = readPos - expected;

            while (diff >  halfRing) diff -= (double) ringLength;
            while (diff < -halfRing) diff += (double) ringLength;

            if (std::abs (diff) > jumpThreshold)
            {
                fadeReadPos = expected;
                fadeCounter = fadeLength;
            }
            else
            {
                readSpeed += 0.02 * (std::abs (1.0 + diff) - readSpeed);
            }
        }

        const int band = juce::jlimit (0, kSincBands - 1,
                                       (int) std::lround ((readSpeed - 1.0) * 4.0));

        double gOld = 0.0, gNew = 1.0;

        if (fadeCounter > 0)
        {
            gNew = 1.0 - ((double) fadeCounter / (double) fadeLength);
            gOld = 1.0 - gNew;
        }

        for (int c = 0; c < channels; ++c)
        {
            double wetSample = readSample (c, readPos, band);

            if (fadeCounter > 0)
                wetSample = readSample (c, fadeReadPos, band) * gOld + wetSample * gNew;

            wetSample *= gain;

            if (runFilter)
            {
                const double v3 = wetSample - fIc2[c];
                const double v1 = fa1 * fIc1[c] + fa2 * v3;
                const double v2 = fIc2[c] + fa2 * fIc1[c] + fa3 * v3;

                fIc1[c] = 2.0 * v1 - fIc1[c];
                fIc2[c] = 2.0 * v2 - fIc2[c];

                const double filtered = filterHighPass ? wetSample - filterK * v1 - v2 : v2;
                wetSample += filterBlend * (filtered - wetSample);
            }

            const double drySample = bufPtr[c][i];
            bufPtr[c][i] = (float) (drySample * dry + wetSample * wet);
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
