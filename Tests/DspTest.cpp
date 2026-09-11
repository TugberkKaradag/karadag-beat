/*
    Karadag Beat - DSP dogrulama testleri.

    Motoru host olmadan calistirip cikisi olcer:
      - bypass gercekten seffaf mi
      - half time gercekten yarim hizda mi (perde bir oktav dusuyor mu)
      - stutter dogru dilimi mi tekrarliyor
      - gate gercekten susturuyor mu
      - basamakli pattern'lerde tik (klik) var mi
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/Envelope.h"
#include "../Source/GrossEngine.h"
#include "../Source/Presets.h"
#include "../Source/Swing.h"
#include "../Source/FilterMap.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kBpm        = 120.0;
    constexpr int    kBlockSize  = 256;

    // 120 BPM 4/4 -> 2 bar = 8 ceyrek nota = 4 saniye
    const double patternLenSamples = (8.0 * 60.0 / kBpm) * kSampleRate;

    int failures = 0;

    void check (bool condition, const juce::String& what, const juce::String& detail = {})
    {
        std::printf ("  [%s] %-46s %s\n",
                     condition ? "GECTI" : "KALDI",
                     what.toRawUTF8(),
                     detail.toRawUTF8());

        if (! condition)
            ++failures;
    }

    struct RunConfig
    {
        double sampleRate  = kSampleRate;
        double bpm         = kBpm;
        int    blockSize   = kBlockSize;
        double beatsPerBar = 4.0;      // ceyrek nota cinsinden

        double patternLength() const
        {
            return (2.0 * beatsPerBar * 60.0 / bpm) * sampleRate;
        }
    };

    /** Motoru verilen zarflarla n pattern boyunca calistirip cikisi dondurur. */
    std::vector<float> run (const Envelope& timeEnv,
                            const Envelope& volEnv,
                            double inputFreq,
                            int    numPatterns,
                            RunConfig cfg = {},
                            bool   timeOn = true,
                            bool   volOn  = true)
    {
        const double patLength = cfg.patternLength();

        GrossEngine engine;
        engine.prepare (cfg.sampleRate, 1);
        engine.setPatternLengthSamples (patLength);
        engine.setPhase (0.0);

        const int total = (int) (patLength * numPatterns);
        std::vector<float> out;
        out.reserve ((size_t) total + (size_t) cfg.blockSize);

        juce::AudioBuffer<float> block (1, cfg.blockSize);
        double phaseAcc = 0.0;
        const double inc = juce::MathConstants<double>::twoPi * inputFreq / cfg.sampleRate;

        for (int written = 0; written < total; written += cfg.blockSize)
        {
            auto* d = block.getWritePointer (0);

            for (int i = 0; i < cfg.blockSize; ++i)
            {
                d[i] = (float) std::sin (phaseAcc);
                phaseAcc += inc;
            }

            engine.processBlock (block, timeEnv, volEnv, timeOn, volOn, 1.0f);

            for (int i = 0; i < cfg.blockSize; ++i)
                out.push_back (d[i]);
        }

        return out;
    }

    /** Bir pencerede sifir gecislerinden frekans tahmini. */
    double estimateFrequency (const std::vector<float>& x, int start, int length,
                              double sampleRate = kSampleRate)
    {
        start  = juce::jlimit (0, (int) x.size() - 1, start);
        length = juce::jmin (length, (int) x.size() - start);

        if (length < 32)
            return 0.0;

        int crossings = 0;

        for (int i = start + 1; i < start + length; ++i)
            if ((x[(size_t) i - 1] <= 0.0f) != (x[(size_t) i] <= 0.0f))
                ++crossings;

        const double seconds = length / sampleRate;
        return (crossings / 2.0) / seconds;
    }

    double rms (const std::vector<float>& x, int start, int length)
    {
        start  = juce::jlimit (0, (int) x.size() - 1, start);
        length = juce::jmin (length, (int) x.size() - start);

        if (length <= 0)
            return 0.0;

        double sum = 0.0;

        for (int i = start; i < start + length; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];

        return std::sqrt (sum / length);
    }

    /** Ardisik sample'lar arasindaki en buyuk sicrama - klik olcusu. */
    float maxJump (const std::vector<float>& x, int start, int length)
    {
        start  = juce::jlimit (0, (int) x.size() - 1, start);
        length = juce::jmin (length, (int) x.size() - start);

        float worst = 0.0f;

        for (int i = start + 1; i < start + length; ++i)
            worst = juce::jmax (worst, std::abs (x[(size_t) i] - x[(size_t) i - 1]));

        return worst;
    }

    /** Eski (dogrusal arama) valueAt - yeni hizli yollarin referansi. */
    double referenceValueAt (const std::vector<EnvPoint>& pts, double phase)
    {
        if (pts.size() == 1)
            return pts[0].y;

        phase -= std::floor (phase);

        const EnvPoint* p0; const EnvPoint* p1;
        double x0, x1;

        if (phase < pts.front().x)
        {
            p0 = &pts.back();  x0 = p0->x - 1.0;
            p1 = &pts.front(); x1 = p1->x;
        }
        else
        {
            size_t i = pts.size() - 1;

            for (size_t n = 1; n < pts.size(); ++n)
                if (pts[n].x > phase) { i = n - 1; break; }

            p0 = &pts[i]; x0 = p0->x;

            if (i + 1 < pts.size()) { p1 = &pts[i + 1];  x1 = p1->x; }
            else                    { p1 = &pts.front(); x1 = p1->x + 1.0; }
        }

        if (p0->stepped)
            return p0->y;

        const double span = x1 - x0;

        if (span <= 1.0e-12)
            return p1->y;

        double t = juce::jlimit (0.0, 1.0, (phase - x0) / span);

        if (std::abs (p0->tension) >= 1.0e-9)
            t = std::pow (t, std::pow (2.0, -p0->tension * 4.0));

        return p0->y + (p1->y - p0->y) * t;
    }

    Envelope fromPoints (const std::vector<EnvPoint>& pts, double fallbackValue)
    {
        Envelope e (fallbackValue);

        if (! pts.empty())
            e.setPoints (pts);

        return e;
    }

    const GrossPreset& preset (const juce::String& name)
    {
        for (const auto& p : Presets::factory())
            if (p.name == name)
                return p;

        jassertfalse;
        return Presets::factory().front();
    }
}

//==============================================================================
int main()
{
    std::printf ("\nKaradag Beat - DSP testleri  (%.0f Hz, %.0f BPM, pattern = %.0f sample)\n\n",
                 kSampleRate, kBpm, patternLenSamples);

    const int patLen = (int) patternLenSamples;

    // ------------------------------------------------------------------
    std::printf ("Zarf matematigi\n");
    {
        Envelope e (0.0);
        e.setPoints ({ { 0.0, 0.0 }, { 1.0, 0.5 } });

        check (std::abs (e.valueAt (0.0) - 0.0)  < 1e-9, "rampa basi 0");
        check (std::abs (e.valueAt (0.5) - 0.25) < 1e-9, "rampa ortasi 0.25");
        check (std::abs (e.valueAt (0.999) - 0.4995) < 1e-3, "rampa sonu ~0.5");

        Envelope s (0.0);
        s.setPoints ({ { 0.0, 0.2, 0.0, true }, { 0.5, 0.8, 0.0, true } });

        check (std::abs (s.valueAt (0.3) - 0.2) < 1e-9, "basamak degeri sabit tutuyor");
        check (std::abs (s.valueAt (0.7) - 0.8) < 1e-9, "ikinci basamak dogru");

        Envelope w (0.0);
        w.setPoints ({ { 0.25, 1.0 }, { 0.75, 0.0 } });
        check (w.valueAt (0.0) > 0.4 && w.valueAt (0.0) < 0.6, "pattern sonu basa sariliyor");
    }

    // ------------------------------------------------------------------
    std::printf ("\nZarf kaydirma (shift)\n");
    {
        double worst = 0.0;
        int    cases = 0;

        for (const auto& pr : Presets::factory())
        {
            for (const auto* src : { &pr.time, &pr.volume })
            {
                if (src->empty())
                    continue;

                Envelope original (0.0);
                original.setPoints (*src);

                // 1/16, 1/8 ve uclemeli 1/24 adimlar, iki yonde
                for (const double dx : { 1.0 / 32.0, 5.0 / 16.0, -1.0 / 8.0, 1.0 / 24.0, -7.0 / 24.0 })
                {
                    Envelope shifted (0.0);
                    shifted.setPoints (*src);
                    shifted.shift (dx);

                    for (int i = 0; i < 997; ++i)
                    {
                        // sicrama noktalarinin tam ustune dusmeyen ornekler
                        const double phase = (i + 0.37) / 997.0;
                        worst = juce::jmax (worst, std::abs (shifted.valueAt (phase + dx)
                                                             - original.valueAt (phase)));
                    }

                    ++cases;
                }
            }
        }

        check (worst < 1.0e-6, "kaydirilmis zarf = zamanda kaymis orijinal",
               juce::String (cases) + " durum, en buyuk fark " + juce::String (worst, 9));

        // 24 kez 1/24 kaydirinca tam tur atip basa donmeli
        const auto& half = preset ("Half Time");
        Envelope e (0.0);
        e.setPoints (half.time);

        for (int i = 0; i < 24; ++i)
            e.shift (1.0 / 24.0);

        double drift = 0.0;

        for (int i = 0; i < 997; ++i)
        {
            const double phase = (i + 0.37) / 997.0;
            Envelope ref (0.0);
            ref.setPoints (half.time);
            drift = juce::jmax (drift, std::abs (e.valueAt (phase) - ref.valueAt (phase)));
        }

        check (drift < 1.0e-6, "tam tur kaydirma zarfi aynen geri getiriyor",
               "fark " + juce::String (drift, 9));
    }

    // ------------------------------------------------------------------
    std::printf ("\nCizim modu (hucre boyama)\n");
    {
        // duz zarf: yalnizca boyanan hucre degismeli, oncesi ve sonrasi ayni kalmali
        Envelope flat (0.0);
        flat.paintStep (0.25, 0.5, 0.8);

        check (std::abs (flat.valueAt (0.1)) < 1e-9
                 && std::abs (flat.valueAt (0.3) - 0.8) < 1e-9
                 && std::abs (flat.valueAt (0.6)) < 1e-9,
               "duz zarfta boyanan hucre disi dokunulmadan kaliyor",
               juce::String (flat.valueAt (0.1), 3) + " / " + juce::String (flat.valueAt (0.3), 3)
                 + " / " + juce::String (flat.valueAt (0.6), 3));

        // rampa: once ve sonrasi ayni rampa uzerinde kalmali
        Envelope ramp (0.0);
        ramp.setPoints ({ { 0.0, 0.0 }, { 1.0, 0.5 } });
        ramp.paintStep (0.5, 0.5625, 0.0);

        check (std::abs (ramp.valueAt (0.25) - 0.125) < 1e-9
                 && std::abs (ramp.valueAt (0.53)) < 1e-9
                 && std::abs (ramp.valueAt (0.75) - 0.375) < 1e-9,
               "rampada hucre oncesi ve sonrasi korunuyor",
               juce::String (ramp.valueAt (0.25), 4) + " / " + juce::String (ramp.valueAt (0.53), 4)
                 + " / " + juce::String (ramp.valueAt (0.75), 4));

        // ardisik hucreler: nokta sayisi sismemeli, her hucre kendi degerinde olmali
        Envelope gate (1.0);

        for (int k = 0; k < 16; ++k)
            gate.paintStep (k / 16.0, (k + 1) / 16.0, (k % 2) ? 0.0 : 1.0);

        bool ok = true;

        for (int k = 0; k < 16; ++k)
            ok = ok && std::abs (gate.valueAt ((k + 0.5) / 16.0) - ((k % 2) ? 0.0 : 1.0)) < 1e-9;

        // ayni hucreleri tekrar boyamak nokta biriktirmemeli
        const int before = gate.getNumPoints();

        for (int k = 0; k < 16; ++k)
            gate.paintStep (k / 16.0, (k + 1) / 16.0, (k % 2) ? 0.0 : 1.0);

        check (ok && gate.getNumPoints() == before,
               "boyanan gate dogru ve tekrar boyama nokta biriktirmiyor",
               juce::String (gate.getNumPoints()) + " nokta");
    }

    // ------------------------------------------------------------------
    std::printf ("\nHizli zarf aramasi eski algoritmayla birebir ayni\n");
    {
        std::vector<std::vector<EnvPoint>> cases;

        for (const auto& pr : Presets::factory())
        {
            if (! pr.time.empty())   cases.push_back (pr.time);
            if (! pr.volume.empty()) cases.push_back (pr.volume);
        }

        // rastgele zarflar - ayni x'te nokta ciftleri de dahil
        juce::Random rng (1234);

        for (int c = 0; c < 40; ++c)
        {
            std::vector<EnvPoint> pts;
            const int count = 1 + rng.nextInt (40);

            for (int i = 0; i < count; ++i)
            {
                const double x = (rng.nextInt (5) == 0 && ! pts.empty()) ? pts.back().x
                                                                         : rng.nextDouble();
                pts.emplace_back (x, rng.nextDouble(), rng.nextDouble() * 2.0 - 1.0, rng.nextBool());
            }

            Envelope e (0.0);
            e.setPoints (pts);            // siralar ve sinirlar
            cases.push_back (e.getPoints());
        }

        double worstPlain = 0.0, worstHinted = 0.0;

        for (const auto& pts : cases)
        {
            Envelope e (0.0);
            e.setPoints (pts);

            int hint = 0;

            // uc tur, dengesiz adimlarla ileri - arada bir geriye de sicra
            double phase = 0.0;

            for (int i = 0; i < 6000; ++i)
            {
                phase += (i % 500 == 499) ? -0.37 : 1.0 / 1997.0;

                const double ref = referenceValueAt (e.getPoints(), phase);
                worstPlain  = juce::jmax (worstPlain,  std::abs (e.valueAt (phase) - ref));
                worstHinted = juce::jmax (worstHinted, std::abs (e.valueAt (phase, hint) - ref));
            }
        }

        check (worstPlain < 1.0e-12, "ikili arama referansla ayni",
               juce::String ((int) cases.size()) + " zarf, en buyuk fark " + juce::String (worstPlain, 15));
        check (worstHinted < 1.0e-12, "ipuclu arama referansla ayni",
               "en buyuk fark " + juce::String (worstHinted, 15));
    }

    // ------------------------------------------------------------------
    std::printf ("\nBypass seffafligi\n");
    {
        Envelope t (0.0), v (1.0);
        const auto out = run (t, v, 500.0, 2);

        // ilk pattern buffer dolarken atlanir
        double worst = 0.0;
        double phaseAcc = 0.0;
        const double inc = juce::MathConstants<double>::twoPi * 500.0 / kSampleRate;

        for (int i = 0; i < (int) out.size(); ++i)
        {
            const double expected = std::sin (phaseAcc);
            phaseAcc += inc;

            if (i > patLen)
                worst = juce::jmax (worst, std::abs (expected - out[(size_t) i]));
        }

        check (worst < 1e-4, "gecikme 0 iken cikis = giris",
               juce::String ("en buyuk fark ") + juce::String (worst, 8));
    }

    // ------------------------------------------------------------------
    std::printf ("\nHalf Time (yarim hiz -> bir oktav asagi)\n");
    {
        const auto& pr = preset ("Half Time");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        const auto out = run (t, v, 400.0, 3);

        // uctuncu pattern'in ortasindan olc
        const double f = estimateFrequency (out, (int) (patLen * 2.3), patLen / 4);

        check (f > 180.0 && f < 220.0, "400 Hz girisi ~200 Hz'e dusuyor",
               juce::String ("olculen ") + juce::String (f, 1) + " Hz");
    }

    // ------------------------------------------------------------------
    std::printf ("\nQuarter Time (ceyrek hiz -> iki oktav asagi)\n");
    {
        const auto& pr = preset ("Quarter Time");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        const auto out = run (t, v, 400.0, 3);
        const double f = estimateFrequency (out, (int) (patLen * 2.3), patLen / 4);

        check (f > 85.0 && f < 115.0, "400 Hz girisi ~100 Hz'e dusuyor",
               juce::String ("olculen ") + juce::String (f, 1) + " Hz");
    }

    // ------------------------------------------------------------------
    std::printf ("\nStutter / Repeat (perde korunmali)\n");
    {
        const auto& pr = preset ("Repeat 1/8");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        const auto out = run (t, v, 400.0, 3);

        // tekrarlanan dilimde perde degismemeli - sadece kaynak konumu degisir
        const double f = estimateFrequency (out, (int) (patLen * 2.3), patLen / 8);

        check (f > 380.0 && f < 420.0, "tekrar sirasinda perde 400 Hz kaliyor",
               juce::String ("olculen ") + juce::String (f, 1) + " Hz");

        const float jump = maxJump (out, patLen * 2, patLen);
        check (jump < 0.35f, "basamak gecislerinde klik yok",
               juce::String ("en buyuk sicrama ") + juce::String (jump, 4));
    }

    // ------------------------------------------------------------------
    std::printf ("\nFreeze (dilim loop'u: DC degil, perde korunmali)\n");
    {
        const auto& pr = preset ("Freeze 2nd Half");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        const auto out = run (t, v, 400.0, 3);

        // pattern'in ikinci yarisi: dondurulmus bolge
        const int start = (int) (patLen * 2.6);
        const double f = estimateFrequency (out, start, patLen / 16);
        const double r = rms (out, start, patLen / 16);

        check (f > 380.0 && f < 420.0, "freeze sirasinda perde 400 Hz kaliyor",
               juce::String ("olculen ") + juce::String (f, 1) + " Hz");
        check (r > 0.5, "freeze DC'ye dusmuyor, ses devam ediyor",
               juce::String ("rms ") + juce::String (r, 4));
    }

    // ------------------------------------------------------------------
    std::printf ("\nReverse gercekten geri sariyor mu\n");
    {
        // Her adimda frekansi 300 -> 600 Hz tirmanan bir chirp veriyoruz.
        // Ses tersine akiyorsa cikista ayni adim 600 -> 300 olarak duyulmali.
        const auto& pr = preset ("Reverse 1/4");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        GrossEngine engine;
        engine.prepare (kSampleRate, 1);
        engine.setPatternLengthSamples (patternLenSamples);
        engine.setPhase (0.0);

        const int stepSamples = patLen / 8;          // Reverse 1/4 -> 8 adim
        const int total = patLen * 3;

        std::vector<float> out;
        out.reserve ((size_t) total + (size_t) kBlockSize);

        juce::AudioBuffer<float> block (1, kBlockSize);
        double acc = 0.0;
        int written = 0;

        while (written < total)
        {
            auto* d = block.getWritePointer (0);

            for (int i = 0; i < kBlockSize; ++i)
            {
                const double posInStep = (double) ((written + i) % stepSamples) / stepSamples;
                const double freq = 300.0 + 300.0 * posInStep;

                d[i] = (float) std::sin (acc);
                acc += juce::MathConstants<double>::twoPi * freq / kSampleRate;
            }

            engine.processBlock (block, t, v, true, true, 1.0f);

            for (int i = 0; i < kBlockSize; ++i)
                out.push_back (d[i]);

            written += kBlockSize;
        }

        // ucuncu pattern'in ikinci adimi: bastaki ve sondaki frekansi karsilastir
        const int stepStart = patLen * 2 + stepSamples;
        const int quarter   = stepSamples / 4;

        const double early = estimateFrequency (out, stepStart + quarter / 2, quarter);
        const double late  = estimateFrequency (out, stepStart + stepSamples - quarter - quarter / 2, quarter);

        check (early > late + 100.0,
               "adim basi tiz, adim sonu pes (ses tersine akiyor)",
               juce::String ("bas ") + juce::String (early, 0)
                 + " Hz  ->  son " + juce::String (late, 0) + " Hz");
    }

    // ------------------------------------------------------------------
    std::printf ("\nGate (volume zarfi)\n");
    {
        const auto& pr = preset ("Gate 1/8");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        const auto out = run (t, v, 400.0, 2);

        const int step = patLen / 16;
        const double openRms   = rms (out, patLen + (int) (step * 0.3), step / 3);
        const double closedRms = rms (out, patLen + step + (int) (step * 0.3), step / 3);

        check (openRms > 0.5,    "acik adimda ses var",
               juce::String ("rms ") + juce::String (openRms, 4));
        check (closedRms < 0.01, "kapali adimda ses yok",
               juce::String ("rms ") + juce::String (closedRms, 6));
    }

    // ------------------------------------------------------------------
    std::printf ("\nSidechain pump\n");
    {
        const auto& pr = preset ("Sidechain 1/4");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        const auto out = run (t, v, 400.0, 2);

        const int beat = patLen / 8;
        const double atBeat  = rms (out, patLen + 8, beat / 16);          // vurusun hemen basi
        const double atEnd   = rms (out, patLen + beat - beat / 8, beat / 16);

        check (atBeat < atEnd * 0.5, "vurus basinda ses kisiliyor, sonra geri geliyor",
               juce::String ("bas ") + juce::String (atBeat, 4)
                 + "  son " + juce::String (atEnd, 4));
    }

    // ------------------------------------------------------------------
    std::printf ("\nOrnekleme frekansindan bagimsizlik\n");
    {
        const auto& pr = preset ("Half Time");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        for (const double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            RunConfig cfg; cfg.sampleRate = sr;
            const int len = (int) cfg.patternLength();

            const auto out = run (t, v, 400.0, 3, cfg);
            const double f = estimateFrequency (out, (int) (len * 2.3), len / 4, sr);

            check (f > 190.0 && f < 210.0,
                   juce::String ((int) sr) + " Hz'de half time dogru",
                   juce::String ("olculen ") + juce::String (f, 1) + " Hz");
        }
    }

    // ------------------------------------------------------------------
    std::printf ("\nTempodan bagimsizlik (stutter perdeyi bozmamali)\n");
    {
        const auto& pr = preset ("Repeat 1/8");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        for (const double bpm : { 70.0, 90.0, 128.0, 174.0 })
        {
            RunConfig cfg; cfg.bpm = bpm;
            const int len = (int) cfg.patternLength();

            const auto out = run (t, v, 400.0, 3, cfg);
            const double f = estimateFrequency (out, (int) (len * 2.3), len / 8);

            check (f > 385.0 && f < 415.0,
                   juce::String (bpm, 0) + " BPM'de perde korunuyor",
                   juce::String ("olculen ") + juce::String (f, 1) + " Hz");
        }
    }

    // ------------------------------------------------------------------
    std::printf ("\nBlok boyutundan bagimsizlik (ornegi ornegine ayni cikis)\n");
    {
        const auto& pr = preset ("Stutter + Gate");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        RunConfig ref;  ref.blockSize = 256;
        const auto expected = run (t, v, 400.0, 2, ref);

        for (const int bs : { 32, 64, 512, 2048 })
        {
            RunConfig cfg; cfg.blockSize = bs;
            const auto out = run (t, v, 400.0, 2, cfg);

            const int n = juce::jmin ((int) out.size(), (int) expected.size());
            double worst = 0.0;

            for (int i = 0; i < n; ++i)
                worst = juce::jmax (worst, (double) std::abs (out[(size_t) i] - expected[(size_t) i]));

            check (worst < 1e-6,
                   juce::String (bs) + " sample'lik blokta ayni sonuc",
                   juce::String ("en buyuk fark ") + juce::String (worst, 9));
        }
    }

    // ------------------------------------------------------------------
    std::printf ("\n3/4 olcu (pattern 2 bar = 6 ceyrek nota)\n");
    {
        const auto& pr = preset ("Gate 1/8");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        RunConfig cfg; cfg.beatsPerBar = 3.0;
        const int len = (int) cfg.patternLength();

        const auto out = run (t, v, 400.0, 2, cfg);

        // gate 16 adima bolunur; adim uzunlugu pattern'e gore olculur
        const int step = len / 16;
        const double openRms   = rms (out, len + (int) (step * 0.3), step / 3);
        const double closedRms = rms (out, len + step + (int) (step * 0.3), step / 3);

        check (openRms > 0.5 && closedRms < 0.01,
               "3/4'te gate adimlari dogru yerde",
               juce::String ("acik ") + juce::String (openRms, 3)
                 + "  kapali " + juce::String (closedRms, 5));
    }

    // ------------------------------------------------------------------
    std::printf ("\nTempo degisimine dayaniklilik\n");
    {
        // Pattern uzunlugu calarken degisirse okuma kafasi patlamamali
        const auto& pr = preset ("Half Time");
        auto t = fromPoints (pr.time, 0.0);
        auto v = fromPoints (pr.volume, 1.0);

        GrossEngine engine;
        engine.prepare (kSampleRate, 1);
        engine.setPhase (0.0);

        juce::AudioBuffer<float> block (1, kBlockSize);
        double acc = 0.0;
        const double inc = juce::MathConstants<double>::twoPi * 400.0 / kSampleRate;

        float peak = 0.0f, worstJump = 0.0f, previous = 0.0f;
        bool finite = true;

        for (int b = 0; b < 900; ++b)
        {
            // her 100 blokta bir tempoyu degistir
            const double bpm = 60.0 + 30.0 * (b / 100);
            engine.setPatternLengthSamples ((8.0 * 60.0 / bpm) * kSampleRate);

            auto* d = block.getWritePointer (0);

            for (int i = 0; i < kBlockSize; ++i)
            {
                d[i] = (float) std::sin (acc);
                acc += inc;
            }

            engine.processBlock (block, t, v, true, true, 1.0f);

            for (int i = 0; i < kBlockSize; ++i)
            {
                const float s = d[i];

                if (! std::isfinite (s)) { finite = false; break; }

                peak = juce::jmax (peak, std::abs (s));

                if (b > 2)
                    worstJump = juce::jmax (worstJump, std::abs (s - previous));

                previous = s;
            }
        }

        check (finite && peak < 1.05f && worstJump < 0.5f,
               "tempo degisirken tasma ve tik yok",
               juce::String ("tepe ") + juce::String (peak, 3)
                 + "  sicrama " + juce::String (worstJump, 3));
    }

    // ------------------------------------------------------------------
    std::printf ("\nIslem suresi (bilgi amacli)\n");
    {
        // Yogun cizilmis bir zarf: 128 nokta, her iki lane'de.  60 saniyelik
        // stereo sesi isleyip gercek zamanin kac kati hizli oldugunu olcuyoruz.
        std::vector<EnvPoint> dense;

        for (int i = 0; i < 128; ++i)
            dense.emplace_back ((double) i / 128.0, (i % 3) * 0.1, 0.3, (i % 2) == 0);

        Envelope t (0.0), v (1.0);
        t.setPoints (dense);
        v.setPoints (dense);

        GrossEngine engine;
        engine.prepare (kSampleRate, 2);
        engine.setPatternLengthSamples (patternLenSamples);

        juce::AudioBuffer<float> block (2, kBlockSize);
        block.clear();

        const int blocks = (int) (kSampleRate * 60.0 / kBlockSize);
        const auto start = juce::Time::getHighResolutionTicks();

        for (int b = 0; b < blocks; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < kBlockSize; ++i)
                    block.setSample (c, i, (float) ((i * 7 + b) % 100) * 0.01f - 0.5f);

            engine.processBlock (block, t, v, true, true, 1.0f);
        }

        const double seconds = juce::Time::highResolutionTicksToSeconds (
                                   juce::Time::getHighResolutionTicks() - start);
        const double realtime = 60.0 / seconds;

        check (realtime > 20.0, "128 noktali zarflarla gercek zamanin ustunde",
               juce::String (juce::roundToInt (realtime)) + "x gercek zaman  (60 sn ses "
                 + juce::String (juce::roundToInt (seconds * 1000.0)) + " ms'de islendi)");
    }

    // ------------------------------------------------------------------
    std::printf ("\nAnahtar ve mix degisimlerinde klik\n");
    {
        // Her birkac blokta VOLUME acilip kapaniyor / mix 1 ile 0 arasinda gidip
        // geliyor.  Gain veya mix'in tek sample'da sicramasi burada gorunur.
        const auto& gate = preset ("Gate 1/8");
        const auto& half = preset ("Half Time");

        for (int scenario = 0; scenario < 2; ++scenario)
        {
            auto t = fromPoints (scenario == 0 ? gate.time   : half.time,   0.0);
            auto v = fromPoints (scenario == 0 ? gate.volume : half.volume, 1.0);

            GrossEngine engine;
            engine.prepare (kSampleRate, 1);
            engine.setPatternLengthSamples (patternLenSamples);
            engine.setPhase (0.0);

            juce::AudioBuffer<float> block (1, kBlockSize);
            double acc = 0.0;
            const double inc = juce::MathConstants<double>::twoPi * 437.0 / kSampleRate;

            std::vector<float> out;

            for (int b = 0; b < 3000; ++b)
            {
                auto* d = block.getWritePointer (0);

                for (int i = 0; i < kBlockSize; ++i)
                {
                    d[i] = (float) std::sin (acc);
                    acc += inc;
                }

                const bool flip = (b / 7) % 2 == 0;

                if (scenario == 0) engine.processBlock (block, t, v, true, flip, 1.0f);
                else               engine.processBlock (block, t, v, true, true, flip ? 1.0f : 0.0f);

                for (int i = 0; i < kBlockSize; ++i)
                    out.push_back (d[i]);
            }

            const float jump = maxJump (out, patLen, (int) out.size() - patLen);

            check (jump < 0.2f,
                   scenario == 0 ? "VOLUME anahtari tik yapmiyor" : "mix degisimi tik yapmiyor",
                   juce::String ("en buyuk sicrama ") + juce::String (jump, 3));
        }
    }

    // ------------------------------------------------------------------
    std::printf ("\nDalga formu dilimleri\n");
    {
        // Pattern'in yalnizca ilk ceyreginde 0.5 genlikli ses, gerisi sessiz.
        // Iki tur sonra ilk ceyregin dilimleri ~0.5, kalanlar 0 olmali.
        Envelope t (0.0), v (1.0);

        GrossEngine engine;
        engine.prepare (kSampleRate, 1);
        engine.setPatternLengthSamples (patternLenSamples);
        engine.setPhase (0.0);

        juce::AudioBuffer<float> block (1, kBlockSize);
        const int total = patLen * 2;

        for (int n = 0; n < total; n += kBlockSize)
        {
            for (int i = 0; i < kBlockSize; ++i)
            {
                const double phase = std::fmod ((double) (n + i) / patternLenSamples, 1.0);
                block.setSample (0, i, phase < 0.25 ? 0.5f : 0.0f);
            }

            engine.processBlock (block, t, v, true, true, 1.0f);
        }

        const int bins    = GrossEngine::kWaveBins;
        const int quarter = bins / 4;

        float loudMin = 1.0f, quietMax = 0.0f;

        for (int b = 1; b < quarter - 1; ++b)      loudMin  = juce::jmin (loudMin,  engine.getWavePeak (b));
        for (int b = quarter + 1; b < bins - 1; ++b) quietMax = juce::jmax (quietMax, engine.getWavePeak (b));

        check (std::abs (loudMin - 0.5f) < 1.0e-4f && quietMax < 1.0e-6f,
               "sesin dustugu dilimler dolu, digerleri bos",
               "ilk ceyrek en az " + juce::String (loudMin, 3)
                 + ", kalan en fazla " + juce::String (quietMax, 5));
    }

    // ------------------------------------------------------------------
    std::printf ("\nSMOOTH sureleri\n");
    {
        // Volume: DC giris + Gate 1/8.  Ilk kapanma kenari 12000. sample'da;
        // cikisin 0.9'dan 0.1'e inmesi slew suresinin %80'i kadar surmeli.
        const auto& gate = preset ("Gate 1/8");

        for (const double ms : { 2.0, 20.0, 60.0 })
        {
            auto t = fromPoints (gate.time, 0.0);
            auto v = fromPoints (gate.volume, 1.0);

            GrossEngine engine;
            engine.prepare (kSampleRate, 1);
            engine.setPatternLengthSamples (patternLenSamples);
            engine.setSmoothing (4.0, ms);
            engine.setPhase (0.0);

            juce::AudioBuffer<float> block (1, kBlockSize);
            std::vector<float> out;

            for (int b = 0; b < 60; ++b)
            {
                for (int i = 0; i < kBlockSize; ++i)
                    block.setSample (0, i, 1.0f);

                engine.processBlock (block, t, v, true, true, 1.0f);

                for (int i = 0; i < kBlockSize; ++i)
                    out.push_back (block.getSample (0, i));
            }

            int hi = -1, lo = -1;

            for (int i = 12000; i < (int) out.size(); ++i)
            {
                if (hi < 0 && out[(size_t) i] < 0.9f) hi = i;
                if (lo < 0 && out[(size_t) i] < 0.1f) { lo = i; break; }
            }

            const double measured = (lo - hi) / kSampleRate * 1000.0;
            const double expected = 0.8 * ms;

            check (hi > 0 && lo > 0 && std::abs (measured - expected) < expected * 0.05 + 0.05,
                   "volume smooth " + juce::String (ms, 0) + " ms",
                   "%90 -> %10: " + juce::String (measured, 2) + " ms, beklenen "
                     + juce::String (expected, 2) + " ms");
        }

        // Time: yavasca yukselen bir rampa girisi.  Repeat 1/8'de 12000. sample'da
        // gecikme 0'dan 12000'e sicriyor; eski kafa x[n], yeni kafa x[n-12000].
        // Cikisin eski kafadan yeni kafaya gecisi capraz gecis suresi kadar surmeli.
        const auto& repeat = preset ("Repeat 1/8");

        for (const double ms : { 4.0, 30.0 })
        {
            auto t = fromPoints (repeat.time, 0.0);
            auto v = fromPoints (repeat.volume, 1.0);

            GrossEngine engine;
            engine.prepare (kSampleRate, 1);
            engine.setPatternLengthSamples (patternLenSamples);
            engine.setSmoothing (ms, 2.0);
            engine.setPhase (0.0);

            const double slope = 1.0e-5;
            juce::AudioBuffer<float> block (1, kBlockSize);
            std::vector<float> out;
            int n = 0;

            for (int b = 0; b < 60; ++b)
            {
                for (int i = 0; i < kBlockSize; ++i)
                    block.setSample (0, i, (float) (slope * n++));

                engine.processBlock (block, t, v, true, true, 1.0f);

                for (int i = 0; i < kBlockSize; ++i)
                    out.push_back (block.getSample (0, i));
            }

            // cikisin yeni kafaya gore kalan payi: 1 = tamamen eski, 0 = tamamen yeni
            auto oldShare = [&] (int i) { return (out[(size_t) i] - slope * (i - 12000)) / (slope * 12000.0); };

            int hi = -1, lo = -1;

            for (int i = 12000; i < 20000; ++i)
            {
                if (hi < 0 && oldShare (i) < 0.9) hi = i;
                if (lo < 0 && oldShare (i) < 0.1) { lo = i; break; }
            }

            const double measured = (lo - hi) / kSampleRate * 1000.0;
            const double expected = 0.8 * ms;

            check (hi > 0 && lo > 0 && std::abs (measured - expected) < expected * 0.05 + 0.05,
                   "time smooth " + juce::String (ms, 0) + " ms",
                   "capraz gecis %90 -> %10: " + juce::String (measured, 2) + " ms, beklenen "
                     + juce::String (expected, 2) + " ms");
        }
    }

    // ------------------------------------------------------------------
    std::printf ("\nTum fabrika pattern'lerinde saglik kontrolu (437 Hz)\n");
    {
        for (const auto& pr : Presets::factory())
        {
            auto t = fromPoints (pr.time, 0.0);
            auto v = fromPoints (pr.volume, 1.0);

            // Eskiden 300 Hz kullaniyorduk; 120 BPM'de bir 1/8'lik adim tam 75 periyot
            // ettigi icin her gate kenari sinusun sifir gecisine denk gelip kliki
            // gizliyordu.  437 Hz adim sinirlarina hicbir zaman hizalanmiyor.
            const auto out = run (t, v, 437.0, 2);

            bool finite = true;
            float peak = 0.0f;
            int   peakIndex = 0;

            for (int i = 0; i < (int) out.size(); ++i)
            {
                const float s = out[(size_t) i];

                if (! std::isfinite (s)) { finite = false; break; }

                if (std::abs (s) > peak)
                {
                    peak = std::abs (s);
                    peakIndex = i;
                }
            }

            const float jump = maxJump (out, patLen, patLen);
            const double peakPhase = std::fmod ((double) peakIndex / patternLenSamples, 1.0);

            // 437 Hz sinusun dogal adimi ~0.057; en hizli pattern (Scratch 2.25x) ile ~0.13.
            // Bunun ustu ani bir sicrama, yani duyulan bir tik.
            check (finite && peak < 1.05f && jump < 0.2f,
                   pr.name,
                   juce::String ("tepe ") + juce::String (peak, 3)
                     + " @faz " + juce::String (peakPhase, 3)
                     + "  sicrama " + juce::String (jump, 3));
        }
    }

    // ------------------------------------------------------------------
    std::printf ("\nSwing zaman bukmesi\n");
    {
        const int cells = 32;       // 2 bar 4/4 = 32 onaltilik

        double worstRoundTrip = 0.0;

        for (int i = 0; i <= 1000; ++i)
        {
            const double t = i / 1000.0;
            const double back = Swing::patternToReal (Swing::realToPattern (t, cells, 0.66), cells, 0.66);
            worstRoundTrip = juce::jmax (worstRoundTrip, std::abs (back - t));
        }

        check (worstRoundTrip < 1.0e-12, "gercek -> pattern -> gercek ayni yere donuyor",
               juce::String (worstRoundTrip, 15));

        bool evenFixed = true;

        for (int k = 0; k <= cells; k += 2)
            evenFixed = evenFixed && std::abs (Swing::patternToReal (k / (double) cells, cells, 0.7)
                                               - k / (double) cells) < 1.0e-12;

        const double oddStart = Swing::patternToReal (1.0 / cells, cells, 0.66);

        check (evenFixed && std::abs (oddStart - 2.0 * 0.66 / cells) < 1.0e-12,
               "vuruslar yerinde, ara onaltiliklar itiliyor",
               "1. onaltilik " + juce::String (oddStart * cells, 3) + " hucreye kaydi");

        check (Swing::patternToReal (0.3, cells, 0.5) == 0.3 && ! Swing::isActive (cells, 0.5),
               "%50 swing duz demek");
    }

    // ------------------------------------------------------------------
    std::printf ("\nSwing'li repeat: kaynak adimin basindan, normal hizda\n");
    {
        // Giris testere dis: her sample'in degeri kendi gercek fazi (0..1).
        // Cikis = okunan kaynak konumu.  Swing'li bir repeat adimi kaynak adimin
        // BASINDAN (0) baslamali ve normal hizda (egim 1) ilerlemeli.
        const int cells = 32;
        const double amount = 0.66;

        std::vector<EnvPoint> swung;
        Swing::apply (preset ("Repeat 1/16").time, swung, cells, amount, true);

        Envelope t (0.0), v (1.0);
        t.setPoints (swung);

        GrossEngine engine;
        engine.prepare (kSampleRate, 1);
        engine.setPatternLengthSamples (patternLenSamples);
        engine.setPhase (0.0);

        std::vector<float> out;
        juce::AudioBuffer<float> block (1, kBlockSize);

        for (int n = 0; n < patLen * 2; n += kBlockSize)
        {
            for (int i = 0; i < kBlockSize; ++i)
                block.setSample (0, i, (float) std::fmod ((double) (n + i) / patternLenSamples, 1.0));

            engine.processBlock (block, t, v, true, true, 1.0f);

            for (int i = 0; i < kBlockSize; ++i)
                out.push_back (block.getSample (0, i));
        }

        double worstStart = 0.0, worstSlope = 0.0;

        for (int pair = 0; pair < cells / 2; ++pair)
        {
            // ikinci (tekrarlanan) onaltiligin gercek baslangici ve bitisi
            const double start = Swing::patternToReal ((2 * pair + 1) / (double) cells, cells, amount);
            const double end   = Swing::patternToReal ((2 * pair + 2) / (double) cells, cells, amount);
            const double first = (2 * pair) / (double) cells;     // kaynak adimin basi

            // capraz gecisi (~4 ms) atla, adimin ortasindan olc
            const int a = patLen + (int) ((start + 0.25 * (end - start)) * patternLenSamples);
            const int b = patLen + (int) ((start + 0.75 * (end - start)) * patternLenSamples);

            const double expectedA = first + (a - patLen) / patternLenSamples - start;
            worstStart = juce::jmax (worstStart, std::abs (out[(size_t) a] - expectedA));

            const double slope = (out[(size_t) b] - out[(size_t) a]) / ((b - a) / patternLenSamples);
            worstSlope = juce::jmax (worstSlope, std::abs (slope - 1.0));
        }

        check (worstStart < 1.0e-3, "tekrar kaynak adimin basindan basliyor",
               "en buyuk kayma " + juce::String (worstStart * patternLenSamples, 1) + " sample");
        check (worstSlope < 1.0e-3, "tekrar normal hizda (perde bozulmuyor)",
               "egim hatasi " + juce::String (worstSlope, 5));
    }

    // ------------------------------------------------------------------
    std::printf ("\nSwing'li gate\n");
    {
        const int cells = 32;
        const double amount = 0.66;

        std::vector<EnvPoint> swung;
        Swing::apply (preset ("Gate 1/16").volume, swung, cells, amount, false);

        Envelope t (0.0), v (1.0);
        v.setPoints (swung);

        GrossEngine engine;
        engine.prepare (kSampleRate, 1);
        engine.setPatternLengthSamples (patternLenSamples);
        engine.setPhase (0.0);

        juce::AudioBuffer<float> block (1, kBlockSize);
        std::vector<float> out;

        for (int n = 0; n < patLen; n += kBlockSize)
        {
            for (int i = 0; i < kBlockSize; ++i)
                block.setSample (0, i, 1.0f);

            engine.processBlock (block, t, v, true, true, 1.0f);

            for (int i = 0; i < kBlockSize; ++i)
                out.push_back (block.getSample (0, i));
        }

        // Her ciftte gate'in kapandigi ilk sample (cikis 0.5'in altina indiginde)
        double worst = 0.0;

        for (int pair = 0; pair < cells / 2; ++pair)
        {
            const double expected = Swing::patternToReal ((2 * pair + 1) / (double) cells, cells, amount)
                                      * patternLenSamples;
            int found = -1;

            // ciftin basinda gate yeni aciliyor (2 ms rampa) - onu atla
            for (int i = (int) (pair * 2.0 / cells * patternLenSamples) + 400; i < (int) out.size(); ++i)
                if (out[(size_t) i] < 0.5f) { found = i; break; }

            // 2 ms'lik slew'in ortasi 0.5'e denk geliyor: ~1 ms gecikme beklenir
            worst = juce::jmax (worst, std::abs (found - expected - kSampleRate * 0.001));
        }

        check (worst < 2.0, "gate kenarlari swing'li konumda",
               "en buyuk sapma " + juce::String (worst, 1) + " sample");
    }

    // ------------------------------------------------------------------
    std::printf ("\nFiltre lane'i\n");
    {
        auto runFiltered = [&] (const Envelope& filterEnv, bool filterOn, bool highPass,
                                double freq, double reso, int patterns, bool toggleHalfway = false)
        {
            Envelope t (0.0), v (1.0);

            GrossEngine engine;
            engine.prepare (kSampleRate, 1);
            engine.setPatternLengthSamples (patternLenSamples);
            engine.setPhase (0.0);
            engine.setFilter (highPass, reso, 5.0);

            std::vector<float> out;
            juce::AudioBuffer<float> block (1, kBlockSize);
            const int total = patLen * patterns;

            for (int n = 0; n < total; n += kBlockSize)
            {
                for (int i = 0; i < kBlockSize; ++i)
                    block.setSample (0, i, (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi
                                                                    * freq * (n + i) / kSampleRate)));

                const bool on = toggleHalfway ? (n < total / 2) : filterOn;
                engine.processBlock (block, t, v, filterEnv, true, true, on, 1.0f);

                for (int i = 0; i < kBlockSize; ++i)
                    out.push_back (block.getSample (0, i));
            }

            return out;
        };

        // 1) Tamamen acik lane: filtresiz yolla bit-bit ayni
        {
            Envelope open (1.0);
            const auto a = runFiltered (open, true,  false, 437.0, 0.8, 1);
            const auto b = runFiltered (open, false, false, 437.0, 0.8, 1);

            float diff = 0.0f;

            for (size_t i = 0; i < a.size(); ++i)
                diff = juce::jmax (diff, std::abs (a[i] - b[i]));

            check (diff == 0.0f, "acik filtre bit-bit seffaf",
                   "en buyuk fark " + juce::String (diff, 9));
        }

        auto levelDb = [] (const std::vector<float>& x)
        {
            const int n = (int) x.size();
            return 20.0 * std::log10 (juce::jmax (1.0e-9, rms (x, n / 2, n / 2) / (0.5 / std::sqrt (2.0))));
        };

        // 2) Kapali (%25 acik) low-pass: ~190 Hz kesim
        {
            Envelope closed (0.25);
            closed.clearTo (0.25);

            const double lowDb  = levelDb (runFiltered (closed, true, false,   50.0, 0.0, 1));
            const double highDb = levelDb (runFiltered (closed, true, false, 5000.0, 0.0, 1));

            check (lowDb > -1.5 && highDb < -40.0, "low-pass bas gecirir, tizi keser",
                   "50 Hz " + juce::String (lowDb, 1) + " dB, 5 kHz " + juce::String (highDb, 1) + " dB  (kesim "
                     + juce::String (FilterMap::cutoffHz (0.25, false), 0) + " Hz)");

            const double hpLow  = levelDb (runFiltered (closed, true, true,   50.0, 0.0, 1));
            const double hpHigh = levelDb (runFiltered (closed, true, true, 12000.0, 0.0, 1));

            check (hpLow < -40.0 && hpHigh > -1.5, "high-pass tizi gecirir, basi keser",
                   "50 Hz " + juce::String (hpLow, 1) + " dB, 12 kHz " + juce::String (hpHigh, 1) + " dB  (kesim "
                     + juce::String (FilterMap::cutoffHz (0.25, true), 0) + " Hz)");
        }

        // 3) Basamakli filtre gate'i: kenarlarda tik yok.  Rezonanssiz olculuyor:
        //    rezonans varken hizli tarama kesim frekansinda kisa bir "zap" cinlamasi
        //    uretir (rezonansli filtrenin sesi) ve ardisik sample farki bunu da
        //    sayar.  Burada gecisin kendisinin sureksiz olmadigini dogruluyoruz.
        {
            Envelope steps (1.0);
            steps.setPoints (preset ("Gate 1/16").volume);

            const auto out = runFiltered (steps, true, false, 437.0, 0.0, 1);
            const float jump = maxJump (out, 0, (int) out.size());

            // 437 Hz, 0.5 genlik sinusun kendi en buyuk adimi ~0.029
            check (jump < 0.06f, "basamakli filtre zarfi tiksiz",
                   "en buyuk sicrama " + juce::String (jump, 4));
        }

        // 4) En yuksek rezonansta hizli tarama: patlamiyor
        {
            Envelope sweep (1.0);
            sweep.setPoints ({ { 0.0, 1.0 }, { 0.5, 0.0 }, { 1.0, 1.0 } });

            const auto out = runFiltered (sweep, true, false, 180.0, 1.0, 1);

            bool finite = true;
            float peak = 0.0f;

            for (float x : out)
            {
                finite = finite && std::isfinite (x);
                peak = juce::jmax (peak, std::abs (x));
            }

            check (finite && peak < 4.0f, "tam rezonansta tarama kararli",
                   "tepe " + juce::String (peak, 2));
        }

        // 5) Kapaliyken lane'i kapatmak: filtre rampayla acilir, tik yok
        {
            Envelope closed (0.2);
            closed.clearTo (0.2);

            const auto out = runFiltered (closed, true, false, 437.0, 0.3, 1, true);
            const int half = (int) out.size() / 2;

            check (maxJump (out, half - 2000, 4000) < 0.06f, "lane kapatilinca tik yok",
                   "en buyuk sicrama " + juce::String (maxJump (out, half - 2000, 4000), 4));
        }
    }

    std::printf ("\n%s  (%d basarisiz)\n\n",
                 failures == 0 ? "TUM TESTLER GECTI" : "BASARISIZ TESTLER VAR",
                 failures);

    return failures == 0 ? 0 : 1;
}
