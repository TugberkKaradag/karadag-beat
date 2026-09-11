#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include "Envelope.h"

class BeatEngine
{
public:
    BeatEngine() = default;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setPatternLengthSamples (double lengthInSamples) noexcept;
    double getPatternLengthSamples() const noexcept { return patternLen; }

    void setSmoothing (double timeMs, double volumeMs) noexcept;

    void setPhase (double newPhase) noexcept;
    double getPhase() const noexcept { return phase; }

    void setFilter (bool highPass, double resonance, double smoothMs) noexcept;

    void processBlock (juce::AudioBuffer<float>& buffer,
                       const Envelope& timeEnv,
                       const Envelope& volEnv,
                       bool  timeEnabled,
                       bool  volEnabled,
                       float mix) noexcept;

    void processBlock (juce::AudioBuffer<float>& buffer,
                       const Envelope& timeEnv,
                       const Envelope& volEnv,
                       const Envelope& filterEnv,
                       bool  timeEnabled,
                       bool  volEnabled,
                       bool  filterEnabled,
                       float mix) noexcept;

    static constexpr int kWaveBins = 512;
    float getWavePeak (int bin) const noexcept
    {
        return wavePeaks[(size_t) juce::jlimit (0, kWaveBins - 1, bin)].load (std::memory_order_relaxed);
    }

    static constexpr int kSincTaps = 32;

    static constexpr int kSincBands = 13;

private:
    float readSample  (int channel, double position, int band) const noexcept;
    float readSinc    (int channel, double position, int band) const noexcept;
    float readHermite (int channel, double position) const noexcept;

    const float* sincTable = nullptr;

    juce::AudioBuffer<float> ring;
    int    ringLength  = 0;
    int    writePos    = 0;
    int    numChannels = 0;
    double sr          = 44100.0;

    void updateFilterCoefficients() noexcept;

    double phase      = 0.0;
    double patternLen = 0.0;

    std::array<std::atomic<float>, kWaveBins> wavePeaks {};
    int   waveBin  = -1;
    float wavePeak = 0.0f;

    int timeSegment   = 0;
    int volSegment    = 0;
    int filterSegment = 0;

    double currentGain = 1.0;
    double gainStep    = 0.0;

    bool   filterHighPass  = false;
    double filterK         = 1.414;
    double filterClosed    = 0.0;
    double filterStep      = 0.0;
    double coefClosed      = -1.0;
    double fa1 = 0.0, fa2 = 0.0, fa3 = 0.0;
    double fIc1[8] {}, fIc2[8] {};
    const Envelope openFilter { 1.0 };

    juce::SmoothedValue<double> smoothedMix;

    double prevReadPos  = 0.0;
    double readSpeed    = 1.0;
    bool   havePrevRead = false;
    double fadeReadPos  = 0.0;
    int    fadeCounter  = 0;
    int    fadeLength   = 0;
    double jumpThreshold = 0.0;
};
