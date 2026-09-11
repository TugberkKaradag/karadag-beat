/*
    Proje kaydetme / acma dogrulamasi.

    FL'de en can sikici hata sinifi sudur: projeyi kaydedip tekrar acinca cizdigin
    pattern kaybolur.  Bu test host olmadan ayni yolu izler:
    durumu yaz, yeni bir ornege yukle, her seyin ayni gelip gelmedigine bak.
*/

#include "../Source/PluginProcessor.h"
#include "../Source/Swing.h"

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

    /** Host transport'u taklidi: tempo, konum, caliyor mu. */
    struct FakePlayHead  : juce::AudioPlayHead
    {
        double bpm = 120.0;
        double ppq = 0.0;
        bool   playing = true;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setIsPlaying (playing);
            info.setTimeSignature (TimeSignature { 4, 4 });
            return info;
        }
    };

    /** Sabit (DC) girisle blok blok calistirir; notalar mutlak sample konumunda.
        Playhead verilmisse her blokta ilerletilir. */
    std::vector<float> runDC (KaradagBeatProcessor& proc, int totalSamples, int blockSize,
                              std::vector<std::pair<int, juce::MidiMessage>> events = {},
                              FakePlayHead* playHead = nullptr, double sampleRate = 48000.0)
    {
        std::vector<float> out;
        out.reserve ((size_t) totalSamples);

        juce::AudioBuffer<float> buffer (1, blockSize);
        juce::MidiBuffer midi;

        for (int start = 0; start < totalSamples; start += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (0, i, 1.0f);

            midi.clear();

            for (const auto& e : events)
                if (e.first >= start && e.first < start + blockSize)
                    midi.addEvent (e.second, e.first - start);

            proc.processBlock (buffer, midi);

            for (int i = 0; i < blockSize; ++i)
                out.push_back (buffer.getSample (0, i));

            if (playHead != nullptr)
                playHead->ppq += blockSize / (60.0 / playHead->bpm * sampleRate);
        }

        return out;
    }

    void setParam (KaradagBeatProcessor& proc, const char* id, float plainValue)
    {
        auto* param = proc.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
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

        auto& f = src.getEditableFilterEnvelope();
        f.clearTo (1.0);
        f.addPoint (0.5, 0.2, 0.4, false);

        auto* bars = src.apvts.getParameter ("patternBars");
        bars->setValueNotifyingHost (bars->convertTo0to1 (2.0f));     // 4 bar

        const bool written = src.exportPattern (file, "Test Deseni");

        KaradagBeatProcessor dst;
        dst.prepareToPlay (48000.0, 512);

        juce::String name;
        const bool read = dst.importPattern (file, name);

        check (written && read
                 && samePoints (dst.getEditableTimeEnvelope().getPoints(),   t.getPoints())
                 && samePoints (dst.getEditableVolumeEnvelope().getPoints(), v.getPoints())
                 && samePoints (dst.getEditableFilterEnvelope().getPoints(), f.getPoints()),
               "uc zarf da dosya uzerinden aynen tasiniyor");

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
    std::printf ("\nCizim MIDI notasindan ve yeniden hazirlanmadan sonra geri geliyor\n");
    {
        KaradagBeatProcessor proc;
        proc.prepareToPlay (48000.0, 512);

        // Cizim: sabit %25 ses.  Secili preset "Off" (tam ses) - farki duyulur.
        proc.getEditableVolumeEnvelope().clearTo (0.25);
        proc.publishEnvelopes();

        const auto drawn = runDC (proc, 2048, 512);
        const auto during = runDC (proc, 2048, 512,
                                   { { 0, juce::MidiMessage::noteOn (1, 60, 0.8f) } });   // C4 = "Off"
        const auto after = runDC (proc, 2048, 512,
                                  { { 0, juce::MidiMessage::noteOff (1, 60) } });

        check (std::abs (drawn.back() - 0.25f) < 1.0e-4f && std::abs (during.back() - 1.0f) < 1.0e-4f
                 && std::abs (after.back() - 0.25f) < 1.0e-4f,
               "nota birakilinca cizim geri geliyor",
               juce::String (drawn.back(), 3) + " -> " + juce::String (during.back(), 3)
                 + " -> " + juce::String (after.back(), 3));

        // Host buffer boyunu degistirince prepareToPlay yeniden cagrilir
        proc.prepareToPlay (48000.0, 256);
        const auto reprepared = runDC (proc, 1024, 256);

        check (std::abs (reprepared.back() - 0.25f) < 1.0e-4f,
               "yeniden hazirlama cizimi silmiyor",
               juce::String (reprepared.back(), 3));
    }

    // ------------------------------------------------------------------
    std::printf ("\nMIDI slotu editorde gosterilirken cizim bekliyor\n");
    {
        const int gate = Presets::names().indexOf ("Gate 1/8");

        KaradagBeatProcessor proc;
        proc.prepareToPlay (48000.0, 512);

        auto& vol = proc.getEditableVolumeEnvelope();
        vol.clearTo (1.0);
        vol.addPoint (0.5, 0.3, 0.2, false);
        const auto drawing = vol.getPoints();
        proc.publishEnvelopes();

        proc.showSlotInEditor (gate);

        const bool shows = proc.isEditorShowingSlot()
                             && samePoints (vol.getPoints(), Presets::factory()[(size_t) gate].volume);

        // Gosterim sirasinda kaydedilen proje cizimi tasimali, gosterilen slotu degil
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        KaradagBeatProcessor reopened;
        reopened.setStateInformation (state.getData(), (int) state.getSize());

        proc.restoreDrawingInEditor();

        check (shows, "editor calan slotu gosteriyor");
        check (samePoints (reopened.getEditableVolumeEnvelope().getPoints(), drawing),
               "gosterim sirasinda kaydedilen proje cizimi tasiyor");
        check (! proc.isEditorShowingSlot() && samePoints (vol.getPoints(), drawing),
               "gosterim bitince cizim aynen geri geliyor");
    }

    // ------------------------------------------------------------------
    std::printf ("\nEklenti penceresi acilinca cizim yerinde kaliyor\n");
    {
        // Kullanici bir slot secip ustune kendi cizimini yapti, pencereyi kapatti
        // ve tekrar acti.  Pencere acilirken slot editore yeniden yuklenmemeli.
        KaradagBeatProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        setParam (proc, "preset", (float) Presets::names().indexOf ("Gate 1/8"));
        runDC (proc, 512, 512);

        auto& vol = proc.getEditableVolumeEnvelope();
        vol.clearTo (0.4);
        const auto drawing = vol.getPoints();
        proc.publishEnvelopes();

        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            editor->setSize (1040, 720);
        }

        const auto out = runDC (proc, 2048, 512);

        check (samePoints (vol.getPoints(), drawing) && std::abs (out.back() - 0.4f) < 1.0e-4f,
               "pencere acilinca slot cizimi ezmiyor",
               "cikis " + juce::String (out.back(), 3));
    }

    // ------------------------------------------------------------------
    std::printf ("\nRetrigger: nota pattern'i bastan baslatiyor\n");
    {
        const int pump = Presets::names().indexOf ("Sidechain 1/4");
        const int noteAt = 100;

        auto runWith = [&] (bool retrigger, double hostPpq, FakePlayHead& head, KaradagBeatProcessor& proc)
        {
            proc.prepareToPlay (48000.0, 512);
            setParam (proc, "midiRetrigger", retrigger ? 1.0f : 0.0f);
            head.ppq = hostPpq;
            proc.setPlayHead (&head);

            return runDC (proc, 4096, 512,
                          { { noteAt, juce::MidiMessage::noteOn (1, 60 + pump, 0.8f) } }, &head);
        };

        // Host iki farkli yerde; retrigger ile ikisi de notada bastan baslamali
        KaradagBeatProcessor retrig, other, plain;
        FakePlayHead head, otherHead, plainHead;

        const auto out      = runWith (true,  1.5,  head,      retrig);
        const auto otherOut = runWith (true,  5.25, otherHead, other);
        const auto plainOut = runWith (false, 1.5,  plainHead, plain);

        float diff = 0.0f, plainDiff = 0.0f;

        for (int i = noteAt; i < 4096; ++i)
        {
            diff      = juce::jmax (diff,      std::abs (out[(size_t) i] - otherOut[(size_t) i]));
            plainDiff = juce::jmax (plainDiff, std::abs (out[(size_t) i] - plainOut[(size_t) i]));
        }

        // Pattern'in basindan calindigini zarfin kendisiyle dogrula
        Envelope pumpEnv (1.0);
        pumpEnv.setPoints (Presets::factory()[(size_t) pump].volume);

        const int k = 3000;
        const double expected = pumpEnv.valueAt (k / 192000.0);
        const double shapeError = std::abs (out[(size_t) (noteAt + k)] - expected);

        check (diff < 1.0e-6f, "retrigger: host konumundan bagimsiz",
               "iki konum arasi en buyuk fark " + juce::String (diff, 7));
        check (shapeError < 0.01, "retrigger: pattern notanin sample'inda bastan",
               "notadan 3000 sample sonra " + juce::String (out[(size_t) (noteAt + k)], 3)
                 + ", zarf " + juce::String (expected, 3));
        check (plainDiff > 0.1f, "retrigger kapaliyken host konumu korunuyor",
               "fark " + juce::String (plainDiff, 3));

        // Nota birakilinca faz host'a geri doner
        runDC (retrig, 512, 512, { { 50, juce::MidiMessage::noteOff (1, 60 + pump) } }, &head);

        const double patternBeats = 8.0;      // 2 bar 4/4
        const double hostPhase = std::fmod (head.ppq / patternBeats, 1.0);
        const double shown = retrig.getPlayheadPhase();

        check (std::abs (shown - hostPhase) < 1.0e-6, "nota birakilinca host fazina donuyor",
               "faz " + juce::String (shown, 6) + ", host " + juce::String (hostPhase, 6));
    }

    // ------------------------------------------------------------------
    std::printf ("\nSlot zinciri\n");
    {
        const int pump = Presets::names().indexOf ("Sidechain 1/4");

        KaradagBeatProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        setParam (proc, "patternBars", 0.0f);         // 1 bar = 96000 sample
        setParam (proc, "chainOn", 1.0f);

        proc.setChainLength (3);
        proc.setChainStep (0, KaradagBeatProcessor::kChainDrawing);
        proc.setChainStep (1, pump);
        proc.setChainStep (2, KaradagBeatProcessor::kChainDrawing);

        FakePlayHead head;
        proc.setPlayHead (&head);

        const int len = 96000;
        const auto out = runDC (proc, len * 3 + 512, 512, {}, &head);

        auto minOver = [&] (int from, int to)
        {
            float m = 1.0f;
            for (int i = from; i < to; ++i) m = juce::jmin (m, out[(size_t) i]);
            return m;
        };

        // 96000, 512'nin kati degil: tur siniri bir blogun ortasina dusuyor
        check (minOver (0, len) > 0.9999f, "1. adim: cizim (tam ses)");
        // Pump turun basinda sifirdan baslar; gain 2 ms'lik slew ile iner.
        // Tur siniri kayan noktali fazdan bulundugu icin en fazla 1 sample kayabilir.
        check (std::abs (out[(size_t) len - 1] - 1.0f) < 1.0e-6f && out[(size_t) len + 1] < 0.995f,
               "2. adim tam tur sinirinda basliyor",
               "sinirdan once " + juce::String (out[(size_t) len - 1], 4)
                 + ", 1 sample sonra " + juce::String (out[(size_t) len + 1], 4));
        check (minOver (len, 2 * len) < 0.5f, "2. adim: sidechain pump",
               "en dusuk " + juce::String (minOver (len, 2 * len), 3));
        check (minOver (2 * len + 200, 3 * len) > 0.9999f, "3. adim: yine cizim");
        check (proc.getActiveChainStep() == 0, "zincir basa sariyor",
               "adim " + juce::String (proc.getActiveChainStep() + 1));

        // Zincir proje ile birlikte kaydedilip aciliyor
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        KaradagBeatProcessor reopened;
        reopened.setStateInformation (state.getData(), (int) state.getSize());

        check (reopened.getChainLength() == 3 && reopened.getChainStep (1) == pump
                 && reopened.getChainStep (0) == KaradagBeatProcessor::kChainDrawing,
               "zincir projeyle kaydediliyor");

        // Transport durukken de zincir kendi turlarini sayar
        KaradagBeatProcessor freeRun;
        freeRun.prepareToPlay (48000.0, 512);
        setParam (freeRun, "chainOn", 1.0f);
        freeRun.setChainLength (2);

        runDC (freeRun, (int) (192000 * 1.5), 512);      // 2 bar @ 120 BPM = 192000 sample

        check (freeRun.getActiveChainStep() == 1, "transport durukken de ilerliyor",
               "adim " + juce::String (freeRun.getActiveChainStep() + 1));
    }

    // ------------------------------------------------------------------
    std::printf ("\nSwing: calan kafa editordeki izgaraya esleniyor\n");
    {
        KaradagBeatProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        setParam (proc, "swing", 66.0f);

        FakePlayHead head;
        head.ppq = 8.0 * 0.3;       // gercek faz 0.3
        proc.setPlayHead (&head);

        runDC (proc, 512, 512, {}, &head);

        const double realPhase = 0.3 + 512.0 / 192000.0;
        const double expected  = Swing::realToPattern (realPhase, 32, 0.66);

        check (std::abs (proc.getPlayheadPhase() - expected) < 1.0e-6
                 && std::abs (expected - realPhase) > 1.0e-3,
               "calan kafa swing'e gore cizim ekseninde",
               "gercek " + juce::String (realPhase, 4) + " -> editor " + juce::String (proc.getPlayheadPhase(), 4));
    }

    // ------------------------------------------------------------------
    std::printf ("\nFiltre zarfi projeyle kaydediliyor\n");
    {
        KaradagBeatProcessor src;
        auto& f = src.getEditableFilterEnvelope();
        f.clearTo (1.0);
        f.addPoint (0.25, 0.1, -0.3, true);
        f.addPoint (0.75, 0.6, 0.0, false);

        juce::MemoryBlock state;
        src.getStateInformation (state);

        KaradagBeatProcessor dst;
        dst.setStateInformation (state.getData(), (int) state.getSize());

        check (samePoints (dst.getEditableFilterEnvelope().getPoints(), f.getPoints()),
               "filtre zarfi aynen geri geldi");
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
