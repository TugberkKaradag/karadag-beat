#include <juce_dsp/juce_dsp.h>
#include "../Source/Envelope.h"
#include "../Source/BeatEngine.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kBpm        = 120.0;
    constexpr int    kBlockSize  = 512;
    constexpr int    kFftOrder   = 14;
    constexpr int    kFftSize    = 1 << kFftOrder;

    const double patternLen = (8.0 * 60.0 / kBpm) * kSampleRate;

    Envelope constantRate (double rate)
    {
        Envelope e (0.0);

        const double slope = 1.0 - rate;
        const double start = (slope < 0.0) ? -slope : 0.0;

        e.setPoints ({ { 0.0, start }, { 1.0, start + slope } });
        return e;
    }

    std::vector<float> renderWindow (double rate, double inputFreq)
    {
        Envelope timeEnv = constantRate (rate);
        Envelope volEnv (1.0);

        BeatEngine engine;
        engine.prepare (kSampleRate, 1);
        engine.setPatternLengthSamples (patternLen);
        engine.setPhase (0.0);

        const int total = (int) (patternLen * 3.0);

        std::vector<float> out;
        out.reserve ((size_t) total + kBlockSize);

        juce::AudioBuffer<float> block (1, kBlockSize);
        double phase = 0.0;
        const double inc = juce::MathConstants<double>::twoPi * inputFreq / kSampleRate;

        for (int written = 0; written < total; written += kBlockSize)
        {
            auto* d = block.getWritePointer (0);

            for (int i = 0; i < kBlockSize; ++i)
            {
                d[i] = (float) std::sin (phase);
                phase += inc;
            }

            engine.processBlock (block, timeEnv, volEnv, true, true, 1.0f);

            for (int i = 0; i < kBlockSize; ++i)
                out.push_back (d[i]);
        }

        const int start = (int) (patternLen * 2.4);

        std::vector<float> window (out.begin() + start, out.begin() + start + kFftSize);
        return window;
    }

    struct Analysis
    {
        double signalFreq   = 0.0;
        double thdnDb       = 0.0;
        double loudestOther = 0.0;
        double otherDb      = 0.0;
        double toneDbfs     = 0.0;
    };

    Analysis analyse (std::vector<float> samples)
    {
        juce::dsp::FFT fft (kFftOrder);
        juce::dsp::WindowingFunction<float> window (kFftSize,
                                                    juce::dsp::WindowingFunction<float>::hann);

        window.multiplyWithWindowingTable (samples.data(), (size_t) kFftSize);

        std::vector<float> fftData ((size_t) kFftSize * 2, 0.0f);
        std::copy (samples.begin(), samples.end(), fftData.begin());

        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const int bins = kFftSize / 2;
        const double binHz = kSampleRate / kFftSize;

        int peakBin = 1;

        for (int i = 2; i < bins; ++i)
            if (fftData[(size_t) i] > fftData[(size_t) peakBin])
                peakBin = i;

        const int skirt = 12;

        double signalEnergy = 0.0, otherEnergy = 0.0;
        int    otherPeakBin = 0;
        double otherPeakMag = 0.0;

        for (int i = 2; i < bins; ++i)
        {
            const double mag = fftData[(size_t) i];
            const double energy = mag * mag;

            if (std::abs (i - peakBin) <= skirt)
            {
                signalEnergy += energy;
            }
            else
            {
                otherEnergy += energy;

                if (mag > otherPeakMag)
                {
                    otherPeakMag = mag;
                    otherPeakBin = i;
                }
            }
        }

        Analysis result;
        result.signalFreq   = peakBin * binHz;
        result.thdnDb       = 10.0 * std::log10 (juce::jmax (1.0e-20, otherEnergy / juce::jmax (1.0e-20, signalEnergy)));
        result.loudestOther = otherPeakBin * binHz;
        result.otherDb      = 20.0 * std::log10 (juce::jmax (1.0e-20, otherPeakMag / juce::jmax (1.0e-20, (double) fftData[(size_t) peakBin])));

        result.toneDbfs     = 20.0 * std::log10 (juce::jmax (1.0e-20, (double) fftData[(size_t) peakBin] / (kFftSize / 2.0)));

        return result;
    }
}

int main()
{
    std::printf ("\nKaradag Beat - aliasing when playing fast\n");
    std::printf ("%.0f Hz sample rate, Nyquist %.0f Hz, FFT %d points\n\n",
                 kSampleRate, kSampleRate * 0.5, kFftSize);

    const double rates[] = { 1.0, 1.25, 1.5, 2.0 };
    const double wanted[] = { 1000.0, 4000.0, 8000.0, 11000.0, 14000.0, 17000.0, 20000.0 };

    const double binHz = kSampleRate / kFftSize;

    for (const double target : wanted)
    {
        const double freq = std::round (target / binHz / 4.0) * 4.0 * binHz;

        std::printf ("input %5.0f Hz\n", freq);

        for (const double rate : rates)
        {
            const double expected = freq * rate;
            const bool foldsOver  = expected > kSampleRate * 0.5;
            const double foldedTo = foldsOver ? (kSampleRate - expected) : expected;

            const auto a = analyse (renderWindow (rate, freq));

            if (foldsOver)
            {
                std::printf ("   speed %.2fx  ->  expected %6.0f Hz*  folded to %6.0f Hz"
                             "   level %6.1f dBFS   (0 = not suppressed at all)\n",
                             rate, expected, foldedTo, a.toneDbfs);
            }
            else
            {
                std::printf ("   speed %.2fx  ->  expected %6.0f Hz    measured tone %6.0f Hz"
                             "   level %5.1f dBFS   THD+N %6.1f dB\n",
                             rate, expected, a.signalFreq, a.toneDbfs, a.thdnDb);
            }
        }

        std::printf ("\n");
    }

    std::printf ("Reading: the 1.0x line is the reference floor.  At other speeds, THD+N close to\n"
                 "that floor means no practical aliasing; 20-30 dB above it is audible.\n\n");

    return 0;
}
