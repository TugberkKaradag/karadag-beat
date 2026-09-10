/*
    Proje kaydetme / acma dogrulamasi.

    FL'de en can sikici hata sinifi sudur: projeyi kaydedip tekrar acinca cizdigin
    pattern kaybolur.  Bu test host olmadan ayni yolu izler:
    durumu yaz, yeni bir ornege yukle, her seyin ayni gelip gelmedigine bak.
*/

#include "../Source/PluginProcessor.h"

#include <cstdio>
#include <cmath>

namespace
{
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

    bool samePoints (const std::vector<EnvPoint>& a, const std::vector<EnvPoint>& b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
        {
            const auto& p = a[i];
            const auto& q = b[i];

            if (std::abs (p.x - q.x) > 1.0e-5 || std::abs (p.y - q.y) > 1.0e-5
                || std::abs (p.tension - q.tension) > 1.0e-3 || p.stepped != q.stepped)
                return false;
        }

        return true;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("\nKaradag Beat - proje durumu testleri\n\n");

    // ------------------------------------------------------------------
    std::printf ("Elle cizilmis zarf kaydedilip geri yukleniyor\n");

    juce::MemoryBlock savedState;
    std::vector<EnvPoint> originalTime, originalVolume;

    {
        KaradagBeatProcessor source;
        source.prepareToPlay (48000.0, 512);

        // Fabrika pattern'lerinin hicbirine benzemeyen bir zarf ciz
        auto& timeEnv = source.getEditableTimeEnvelope();
        timeEnv.clearTo (0.0);
        timeEnv.addPoint (0.125, 0.375, 0.42, false);
        timeEnv.addPoint (0.500, 0.125, 0.00, true);
        timeEnv.addPoint (0.875, 0.750, -0.31, false);

        auto& volEnv = source.getEditableVolumeEnvelope();
        volEnv.clearTo (1.0);
        volEnv.addPoint (0.25, 0.0, 0.0, true);
        volEnv.addPoint (0.75, 0.5, 0.6, false);

        source.publishEnvelopes();

        // parametreleri de varsayilandan uzaklastir
        source.apvts.getParameter ("mix")   ->setValueNotifyingHost (0.42f);
        source.apvts.getParameter ("timeOn")->setValueNotifyingHost (0.0f);

        originalTime   = timeEnv.getPoints();
        originalVolume = volEnv.getPoints();

        source.getStateInformation (savedState);
    }

    check (savedState.getSize() > 0, "durum yazildi",
           juce::String ((int) savedState.getSize()) + " bayt");

    {
        KaradagBeatProcessor restored;
        restored.prepareToPlay (48000.0, 512);
        restored.setStateInformation (savedState.getData(), (int) savedState.getSize());

        check (samePoints (restored.getEditableTimeEnvelope().getPoints(), originalTime),
               "time zarfi aynen geri geldi",
               juce::String ((int) restored.getEditableTimeEnvelope().getNumPoints()) + " nokta");

        check (samePoints (restored.getEditableVolumeEnvelope().getPoints(), originalVolume),
               "volume zarfi aynen geri geldi",
               juce::String ((int) restored.getEditableVolumeEnvelope().getNumPoints()) + " nokta");

        const float mix = restored.apvts.getParameter ("mix")->getValue();
        check (std::abs (mix - 0.42f) < 1.0e-3f, "mix degeri korundu",
               juce::String (mix, 3));

        const float timeOn = restored.apvts.getParameter ("timeOn")->getValue();
        check (timeOn < 0.5f, "time anahtari korundu");
    }

    // ------------------------------------------------------------------
    std::printf ("\nGeri yuklenen zarf gercekten sese uygulaniyor\n");
    {
        // Durumu yukledikten sonra ses islenince cizilen zarfin
        // preset tarafindan ezilmedigini dogrula
        KaradagBeatProcessor restored;
        restored.prepareToPlay (48000.0, 512);
        restored.setStateInformation (savedState.getData(), (int) savedState.getSize());

        // time kapali kaydedilmisti, geri acalim ki etkisi duyulsun
        restored.apvts.getParameter ("timeOn")->setValueNotifyingHost (1.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int block = 0; block < 200; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);

                for (int i = 0; i < 512; ++i)
                    d[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                             * 300.0 * (block * 512 + i) / 48000.0);
            }

            restored.processBlock (buffer, midi);
        }

        check (samePoints (restored.getEditableTimeEnvelope().getPoints(), originalTime),
               "200 blok ses sonrasi zarf hala yerinde");

        bool finite = true;
        float peak = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float s = buffer.getSample (ch, i);

                if (! std::isfinite (s))
                    finite = false;

                peak = juce::jmax (peak, std::abs (s));
            }

        check (finite && peak <= 1.05f, "cikis saglikli",
               juce::String ("tepe ") + juce::String (peak, 3));
    }

    // ------------------------------------------------------------------
    std::printf ("\nBos / bozuk durum eklentiyi cokertmiyor\n");
    {
        KaradagBeatProcessor p;
        p.prepareToPlay (48000.0, 512);

        p.setStateInformation (nullptr, 0);

        const char junk[] = "bu gecerli bir durum blogu degil";
        p.setStateInformation (junk, (int) sizeof (junk));

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        juce::MidiBuffer midi;
        p.processBlock (buffer, midi);

        check (true, "bozuk durum guvenle yok sayildi");
    }

    // ------------------------------------------------------------------
    std::printf ("\nPattern uzunlugu 1 / 2 / 4 bar\n");
    {
        // Gate 1/8 pattern'i her uzunlukta 16 adima bolunur.  Pattern uzadikca
        // adimlar da uzar, yani ayni surede daha az kapanma duyulmali.
        const int gateIndex = Presets::names().indexOf ("Gate 1/8");
        jassert (gateIndex >= 0);

        const int expected[] = { 16, 8, 4 };   // 4 saniyede kapanma sayisi

        for (int choice = 0; choice < 3; ++choice)
        {
            KaradagBeatProcessor proc;
            proc.prepareToPlay (48000.0, 512);

            auto* presetParam = proc.apvts.getParameter ("preset");
            auto* barsParam   = proc.apvts.getParameter ("patternBars");

            presetParam->setValueNotifyingHost (presetParam->convertTo0to1 ((float) gateIndex));
            barsParam  ->setValueNotifyingHost (barsParam  ->convertTo0to1 ((float) choice));

            // Sabit (DC) giris: time zarfi duz oldugu icin cikis dogrudan
            // gain degerini verir.  Sinus kullansaydik kendi sifir gecislerini
            // gate kapanmasi sanardik.
            juce::AudioBuffer<float> buffer (1, 512);
            juce::MidiBuffer midi;

            int closings = 0;
            bool wasOpen = true;

            const int blocks = (int) (48000.0 * 4.0 / 512.0);

            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < 512; ++i)
                    buffer.setSample (0, i, 1.0f);

                proc.processBlock (buffer, midi);

                for (int i = 0; i < 512; ++i)
                {
                    const bool open = buffer.getSample (0, i) > 0.5f;

                    if (wasOpen && ! open)
                        ++closings;

                    wasOpen = open;
                }
            }

            static const int barCount[] = { 1, 2, 4 };
            const int want = expected[choice];
            const bool ok = closings >= want - 1 && closings <= want + 1;

            check (ok, juce::String (barCount[choice]) + " bar'da gate periyodu dogru",
                   juce::String (closings) + " kapanma, beklenen ~" + juce::String (want));
        }
    }

    // ------------------------------------------------------------------
    std::printf ("\nMIDI tetikleme: basili tut ve latch\n");
    {
        auto send = [] (KaradagBeatProcessor& proc, const juce::MidiMessage& m)
        {
            juce::AudioBuffer<float> buffer (2, 256);
            buffer.clear();
            juce::MidiBuffer midi;
            midi.addEvent (m, 0);
            proc.processBlock (buffer, midi);
        };

        const int slot = 5;
        const int note = 60 + slot;

        // --- basili tut (varsayilan) ---
        KaradagBeatProcessor held;
        held.prepareToPlay (48000.0, 256);

        send (held, juce::MidiMessage::noteOn (1, note, 0.8f));
        const bool onWhileHeld = held.getMidiPreset() == slot;
        send (held, juce::MidiMessage::noteOff (1, note));
        const bool offOnRelease = held.getMidiPreset() == -1;

        check (onWhileHeld && offOnRelease, "basili tut: nota birakilinca pattern kapaniyor");

        send (held, juce::MidiMessage::noteOn (1, note, 0.8f));
        send (held, juce::MidiMessage::noteOn (1, 127, 0.8f));      // slot araliginin disi

        check (held.getMidiPreset() == slot, "aralik disi tiz nota gecerli notayi gizlemiyor",
               "calan slot " + juce::String (held.getMidiPreset()));

        // --- latch ---
        KaradagBeatProcessor latch;
        latch.prepareToPlay (48000.0, 256);
        latch.apvts.getParameter ("midiLatch")->setValueNotifyingHost (1.0f);

        send (latch, juce::MidiMessage::noteOn (1, note, 0.8f));
        const bool opened = latch.getMidiPreset() == slot;

        send (latch, juce::MidiMessage::noteOff (1, note));
        const bool staysOpen = latch.getMidiPreset() == slot;

        send (latch, juce::MidiMessage::noteOn (1, note + 1, 0.8f));
        const bool switched = latch.getMidiPreset() == slot + 1;

        send (latch, juce::MidiMessage::noteOn (1, note + 1, 0.8f));
        const bool closed = latch.getMidiPreset() == -1;

        check (opened && staysOpen, "latch: nota birakilsa da pattern acik kaliyor");
        check (switched && closed,  "latch: baska nota degistiriyor, ayni nota kapatiyor");
    }

    // ------------------------------------------------------------------
    std::printf ("\nMIDI olayi blok icinde tam yerinde uygulaniyor\n");
    {
        // Sabit (DC) giris, secili pattern Off: cikis = 1.0.  Blogun 300. sample'inda
        // Sidechain 1/4 notasi geliyor; pump pattern'in basi sifira yakin oldugu icin
        // degisim cikista hemen gorunur.  Notadan onceki 300 sample'a dokunulmamali.
        const int slot = Presets::names().indexOf ("Sidechain 1/4");
        jassert (slot >= 0);

        KaradagBeatProcessor proc;
        proc.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (1, 512);
        juce::MidiBuffer midi;

        auto runBlock = [&] (const juce::MidiMessage& m, int position)
        {
            for (int i = 0; i < 512; ++i)
                buffer.setSample (0, i, 1.0f);

            midi.clear();
            midi.addEvent (m, position);
            proc.processBlock (buffer, midi);
        };

        runBlock (juce::MidiMessage::noteOn (1, 60 + slot, 0.8f), 300);

        float worstBefore = 0.0f;

        for (int i = 0; i < 300; ++i)
            worstBefore = juce::jmax (worstBefore, std::abs (buffer.getSample (0, i) - 1.0f));

        const float after = buffer.getSample (0, 480);

        check (worstBefore < 1.0e-6f && after < 0.6f,
               "nota oncesi dokunulmadan, nota sonrasi pattern devrede",
               "nota oncesi en buyuk sapma " + juce::String (worstBefore, 4)
                 + ", 480. sample " + juce::String (after, 3));

        // Bir sonraki blogun 200. sample'inda nota birakiliyor: o ana kadar pump
        // devam etmeli, sonra (2 ms'lik rampayla) tekrar tam sese donmeli.
        runBlock (juce::MidiMessage::noteOff (1, 60 + slot), 200);

        const float stillPumping = buffer.getSample (0, 190);
        const float released     = buffer.getSample (0, 500);

        check (stillPumping < 0.9f && released > 0.99f,
               "nota birakildigi sample'da pattern kapaniyor",
               "190. sample " + juce::String (stillPumping, 3)
                 + ", 500. sample " + juce::String (released, 3));
    }

    // ------------------------------------------------------------------
    std::printf ("\nPattern disa / ice aktarma\n");
    {
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("karadag_beat_test.kbeat");

        KaradagBeatProcessor src;
        src.prepareToPlay (48000.0, 512);

        auto& t = src.getEditableTimeEnvelope();
        t.clearTo (0.0);
        t.addPoint (0.25, 0.5, 0.3, false);
        t.addPoint (0.75, 0.125, 0.0, true);

        auto& v = src.getEditableVolumeEnvelope();
        v.clearTo (1.0);
        v.paintStep (0.5, 0.5625, 0.0);

        auto* bars = src.apvts.getParameter ("patternBars");
        bars->setValueNotifyingHost (bars->convertTo0to1 (2.0f));     // 4 bar

        const bool written = src.exportPattern (file, "Test Deseni");

        KaradagBeatProcessor dst;
        dst.prepareToPlay (48000.0, 512);

        juce::String name;
        const bool read = dst.importPattern (file, name);

        check (written && read
                 && samePoints (dst.getEditableTimeEnvelope().getPoints(),   t.getPoints())
                 && samePoints (dst.getEditableVolumeEnvelope().getPoints(), v.getPoints()),
               "zarflar dosya uzerinden aynen tasiniyor");

        check (name == "Test Deseni" && dst.getPatternBars() == 4,
               "ad ve pattern uzunlugu da tasiniyor",
               "'" + name + "', " + juce::String (dst.getPatternBars()) + " bar");

        // bozuk dosya: reddedilmeli ve mevcut cizim bozulmamali
        const auto before = dst.getEditableTimeEnvelope().getPoints();
        file.replaceWithText ("<KaradagBeatPattern time=\"cop\" volume=\"\"/>");

        const bool rejected = ! dst.importPattern (file, name);

        check (rejected && samePoints (dst.getEditableTimeEnvelope().getPoints(), before),
               "bozuk dosya reddediliyor, cizim korunuyor");

        file.deleteFile();
    }

    // ------------------------------------------------------------------
    std::printf ("\nMono ve stereo kanal duzenleri\n");
    {
        for (const int channels : { 1, 2 })
        {
            KaradagBeatProcessor p;
            p.prepareToPlay (48000.0, 256);

            juce::AudioBuffer<float> buffer (channels, 256);
            juce::MidiBuffer midi;

            bool finite = true;

            for (int block = 0; block < 100; ++block)
            {
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto* d = buffer.getWritePointer (ch);

                    for (int i = 0; i < 256; ++i)
                        d[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                 * 220.0 * (block * 256 + i) / 48000.0);
                }

                p.processBlock (buffer, midi);

                for (int ch = 0; ch < channels; ++ch)
                    for (int i = 0; i < 256; ++i)
                        if (! std::isfinite (buffer.getSample (ch, i)))
                            finite = false;
            }

            check (finite, juce::String (channels) + " kanalda saglikli calisiyor");
        }
    }

    std::printf ("\n%s  (%d basarisiz)\n\n",
                 failures == 0 ? "TUM TESTLER GECTI" : "BASARISIZ TESTLER VAR",
                 failures);

    return failures == 0 ? 0 : 1;
}
