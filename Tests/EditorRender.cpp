/*
    Eklenti penceresini ekran disinda PNG'ye cizer - arayuz duzenini host acmadan
    ve ekrana hicbir pencere getirmeden kontrol etmek icin.  Once kisa bir davul
    dongusu isler ki arkadaki dalga formu ve calan kafa da gorunsun.

      editor.png       varsayilan boyut (1040 x 720), README'deki ekran goruntusu
      editor_min.png   en kucuk boyut (1000 x 600)
      chain.png        zincir paneli
*/

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Theme.h"

#include <cstdio>

/** KaradagBeatEditor'un friend'i: zamanlayiciyi ve paleti elle tetikler. */
struct EditorSnapshot
{
    static void tick (KaradagBeatEditor& e)          { e.timerCallback(); }
    static void applyTheme (KaradagBeatEditor& e)    { e.applyThemeColours(); }
};

namespace
{
    bool savePng (const juce::Image& image, const juce::String& name)
    {
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (name);
        file.deleteFile();

        juce::PNGImageFormat png;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

        if (stream == nullptr || ! png.writeImageToStream (image, *stream))
        {
            std::printf ("yazilamadi: %s\n", name.toRawUTF8());
            return false;
        }

        std::printf ("%s  (%d x %d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
        return true;
    }

    struct SteadyPlayHead  : juce::AudioPlayHead
    {
        double ppq = 0.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (120.0);
            info.setPpqPosition (ppq);
            info.setIsPlaying (true);
            info.setTimeSignature (TimeSignature { 4, 4 });
            return info;
        }
    };

    /** 120 BPM'de basit bir davul: 1 ve 3'te kick, 2 ve 4'te snare, 1/8 hi-hat. */
    float drumSample (juce::int64 n, juce::Random& noise)
    {
        const double sr = 48000.0, beat = 24000.0;
        const double inBeat = std::fmod ((double) n, beat) / sr;             // saniye
        const int beatIndex = (int) ((double) n / beat) % 4;

        double s = 0.0;

        if (beatIndex == 0 || beatIndex == 2)
        {
            const double f = 50.0 + 90.0 * std::exp (-inBeat / 0.03);
            s += 0.9 * std::sin (juce::MathConstants<double>::twoPi * f * inBeat) * std::exp (-inBeat / 0.16);
        }
        else
        {
            s += (noise.nextFloat() * 2.0 - 1.0) * 0.5 * std::exp (-inBeat / 0.07);
            s += 0.3 * std::sin (juce::MathConstants<double>::twoPi * 190.0 * inBeat) * std::exp (-inBeat / 0.05);
        }

        const double inEighth = std::fmod ((double) n, beat * 0.5) / sr;
        s += (noise.nextFloat() * 2.0 - 1.0) * 0.12 * std::exp (-inEighth / 0.015);

        return (float) juce::jlimit (-1.0, 1.0, s);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    KaradagBeatProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // Bir fabrika pattern'i secili, uzerine elle bir filtre egrisi cizilmis
    {
        const int preset = Presets::names().indexOf ("Stutter + Gate");

        auto* param = proc.apvts.getParameter ("preset");
        param->setValueNotifyingHost (param->convertTo0to1 ((float) preset));
        proc.loadSlotIntoEditor (preset);

        auto& f = proc.getEditableFilterEnvelope();
        f.clearTo (1.0);
        f.setPoints ({ { 0.0, 1.0 }, { 0.5, 0.15, 0.5, false }, { 0.75, 0.6, 0.0, true }, { 0.875, 1.0 } });

        proc.publishEnvelopes();
    }

    // 1.37 pattern boyunca davul: dalga formu dolar, calan kafa pattern'in ortasina gelir
    {
        SteadyPlayHead head;
        proc.setPlayHead (&head);

        juce::Random noise (7);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        const juce::int64 total = (juce::int64) (192000 * 1.37);

        for (juce::int64 start = 0; start < total; start += 512)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float s = 0.8f * drumSample (start + i, noise);
                buffer.setSample (0, i, s);
                buffer.setSample (1, i, s);
            }

            proc.processBlock (buffer, midi);
            head.ppq += 512.0 / 24000.0;
        }

        proc.setPlayHead (nullptr);
    }

    {
        std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
        auto& editor = dynamic_cast<KaradagBeatEditor&> (*base);

        // README icin her zaman varsayilan palet, kullanicinin sectigi degil
        Themes::setCurrent (0);
        EditorSnapshot::applyTheme (editor);
        EditorSnapshot::tick (editor);

        for (const auto& [w, h, name] : { std::tuple<int, int, const char*> { 1040, 720, "editor.png" },
                                          std::tuple<int, int, const char*> { 1000, 600, "editor_min.png" } })
        {
            editor.setSize (w, h);
            savePng (editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f), name);
        }

        // Zincir paneli editorun bakisiyla
        ChainPanel panel (proc);
        panel.setLookAndFeel (&editor.getLookAndFeel());
        savePng (panel.createComponentSnapshot (panel.getLocalBounds(), true, 1.0f), "chain.png");
        panel.setLookAndFeel (nullptr);
    }

    return 0;
}
