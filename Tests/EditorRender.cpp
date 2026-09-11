/*
    Eklenti penceresini ekran disinda PNG'ye cizer - arayuz duzenini host acmadan
    ve ekrana hicbir pencere getirmeden kontrol etmek icin.

      editor.png       varsayilan boyut (1040 x 720)
      editor_min.png   en kucuk boyut (1000 x 600)
      chain.png        zincir paneli
*/

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Theme.h"

#include <cstdio>

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
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    KaradagBeatProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // Uc lane'de de bir seyler gorunsun
    {
        const int repeat = Presets::names().indexOf ("Repeat 1/8");
        const int pump   = Presets::names().indexOf ("Sidechain 1/8");

        auto& t = proc.getEditableTimeEnvelope();
        t.setPoints (Presets::factory()[(size_t) repeat].time);

        auto& v = proc.getEditableVolumeEnvelope();
        v.setPoints (Presets::factory()[(size_t) pump].volume);

        auto& f = proc.getEditableFilterEnvelope();
        f.clearTo (1.0);
        f.setPoints ({ { 0.0, 1.0 }, { 0.5, 0.15, 0.5, false }, { 0.75, 0.6, 0.0, true }, { 0.875, 1.0 } });

        proc.publishEnvelopes();
    }

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

        for (const auto& [w, h, name] : { std::tuple<int, int, const char*> { 1040, 720, "editor.png" },
                                          std::tuple<int, int, const char*> { 1000, 600, "editor_min.png" } })
        {
            editor->setSize (w, h);
            savePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f), name);
        }

        // Zincir paneli editorun bakisiyla
        ChainPanel panel (proc);
        panel.setLookAndFeel (&editor->getLookAndFeel());
        panel.setSize (panel.getWidth(), panel.getHeight());
        savePng (panel.createComponentSnapshot (panel.getLocalBounds(), true, 1.0f), "chain.png");
        panel.setLookAndFeel (nullptr);
    }

    return 0;
}
