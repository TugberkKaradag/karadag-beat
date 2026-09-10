/*
    Hizli calmada aliasing olcumu.

    Okuma kafasi normalden hizli ilerledigi zaman (Double Time, Reverse, Scratch)
    frekanslar yukari kayar.  Nyquist'i asan bilesenler temsil edilemedigi icin
    spektrumda geri katlanir ve metalik bir tini birakir.

    Burada sabit hizli bir zarf kurup saf sinus geciriyoruz, cikisi FFT ile
    inceleyip beklenen ton disindaki butun enerjiyi olcuyoruz (THD+N).
    Referans olarak ayni olcum hiz 1.0'da da yapiliyor - o degerden ne kadar
    uzaklasildigi aliasing'in gercek boyutunu veriyor.
*/

#include <juce_dsp/juce_dsp.h>
#include "../Source/Envelope.h"
#include "../Source/GrossEngine.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kBpm        = 120.0;
    constexpr int    kBlockSize  = 512;
    constexpr int    kFftOrder   = 14;              // 16384 nokta
    constexpr int    kFftSize    = 1 << kFftOrder;

    const double patternLen = (8.0 * 60.0 / kBpm) * kSampleRate;   // 2 bar = 4 sn

    /** Pattern boyunca sabit okuma hizi veren zarf.  hiz = 1 - egim. */
    Envelope constantRate (double rate)
    {
        Envelope e (0.0);

        const double slope = 1.0 - rate;            // y'nin pattern boyunca degisimi
        const double start = (slope < 0.0) ? -slope : 0.0;

        e.setPoints ({ { 0.0, start }, { 1.0, start + slope } });
        return e;
    }

    /** Motoru calistirip cikisin ortasindan bir FFT penceresi dondurur. */
    std::vector<float> renderWindow (double rate, double inputFreq)
    {
        Envelope timeEnv = constantRate (rate);
        Envelope volEnv (1.0);

        GrossEngine engine;
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

        // Ucuncu pattern'in ortasindan al - sarma noktasindaki gecisten uzak
        const int start = (int) (patternLen * 2.4);

        std::vector<float> window (out.begin() + start, out.begin() + start + kFftSize);
        return window;
    }

    struct Analysis
    {
        double signalFreq   = 0.0;   // olculen en guclu ton
        double thdnDb       = 0.0;   // ton disindaki her seyin orani
        double loudestOther = 0.0;   // en guclu yabanci bilesen (Hz)
        double otherDb      = 0.0;   // onun seviyesi
        double toneDbfs     = 0.0;   // en guclu tonun mutlak seviyesi (tam olcekli sinus = 0)
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

        // en guclu bin = ton
        int peakBin = 1;

        for (int i = 2; i < bins; ++i)
            if (fftData[(size_t) i] > fftData[(size_t) peakBin])
                peakBin = i;

        // tonun eteklerini (pencere sizmasi) sinyale dahil et
        // Hann penceresinin ana lobu birkac bine yayilir; bu etek
        // sinyale ait, yabanci enerji sayilmamali.
        const int skirt = 12;

        double signalEnergy = 0.0, otherEnergy = 0.0;
        int    otherPeakBin = 0;
        double otherPeakMag = 0.0;

        for (int i = 2; i < bins; ++i)     // DC ve en alt bini atla
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

        // JUCE'nin pencere tablosu normalize ediliyor (tutarli kazanc 1), bu yuzden
        // tam olcekli bir sinusun tepe bini N/2 buyuklugunde
        result.toneDbfs     = 20.0 * std::log10 (juce::jmax (1.0e-20, (double) fftData[(size_t) peakBin] / (kFftSize / 2.0)));

        return result;
    }
}

//==============================================================================
int main()
{
    std::printf ("\nKaradag Beat - hizli calmada aliasing olcumu\n");
    std::printf ("%.0f Hz ornekleme, Nyquist %.0f Hz, FFT %d nokta\n\n",
                 kSampleRate, kSampleRate * 0.5, kFftSize);

    const double rates[] = { 1.0, 1.25, 1.5, 2.0 };
    const double wanted[] = { 1000.0, 4000.0, 8000.0, 11000.0, 14000.0, 17000.0, 20000.0 };

    // Test tonlarini FFT bin merkezine oturt - aksi halde pencere sizmasi
    // gercek bozulmanin ustunu ortuyor.  Hizlar rasyonel oldugu icin cikis
    // frekanslari da hizali kaliyor.
    const double binHz = kSampleRate / kFftSize;

    for (const double target : wanted)
    {
        // Bin indeksi 4'un kati: 1.25x ve 1.5x ile carpildiginda cikis tonu da
        // tam bir bin merkezine dusuyor, pencere sizmasi olcumu kirletmiyor.
        const double freq = std::round (target / binHz / 4.0) * 4.0 * binHz;

        std::printf ("giris %5.0f Hz\n", freq);

        for (const double rate : rates)
        {
            const double expected = freq * rate;
            const bool foldsOver  = expected > kSampleRate * 0.5;
            const double foldedTo = foldsOver ? (kSampleRate - expected) : expected;

            const auto a = analyse (renderWindow (rate, freq));

            if (foldsOver)
            {
                // Beklenen ton temsil edilemez; ne kadar az duyulursa o kadar iyi.
                // Olculen en guclu bilesen katlanan alias'in kendisi.
                std::printf ("   hiz %.2fx  ->  beklenen %6.0f Hz*  katlanan %6.0f Hz"
                             "   seviyesi %6.1f dBFS   (0 = hic bastirilmamis)\n",
                             rate, expected, foldedTo, a.toneDbfs);
            }
            else
            {
                std::printf ("   hiz %.2fx  ->  beklenen %6.0f Hz    olculen ton %6.0f Hz"
                             "   seviye %5.1f dBFS   THD+N %6.1f dB\n",
                             rate, expected, a.signalFreq, a.toneDbfs, a.thdnDb);
            }
        }

        std::printf ("\n");
    }

    std::printf ("Okuma: hiz 1.0 satiri referans taban.  Diger hizlarda THD+N o tabana\n"
                 "yakinsa aliasing pratikte yok demektir; 20-30 dB uzerine cikiyorsa duyulur.\n\n");

    return 0;
}
