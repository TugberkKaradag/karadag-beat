#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/Envelope.h"
#include "../Source/Presets.h"
#include "../Source/EnvelopeEditor.h"
#include "../Source/Theme.h"

namespace
{
    inline juce::Colour timeAccent()  { return Themes::current().timeAccent; }
    inline juce::Colour volAccent()   { return Themes::current().volumeAccent; }
    inline juce::Colour background()  { return Themes::current().background; }

    constexpr int kCellWidth   = 540;
    constexpr int kTitleHeight = 20;
    constexpr int kTimeHeight  = 96;
    constexpr int kVolHeight   = 64;
    constexpr int kGap         = 14;

    constexpr int kCellHeight = kTitleHeight + kTimeHeight + kVolHeight + kGap;

    void renderEnvelope (juce::Graphics& target,
                         juce::Rectangle<int> bounds,
                         Envelope& env,
                         bool volumeStyle,
                         juce::Colour accent,
                         bool showRuler)
    {
        EnvelopeEditor editor (env, volumeStyle);
        editor.setAccentColour (accent);
        editor.setShowRuler (showRuler);
        editor.setSize (bounds.getWidth(), bounds.getHeight());

        juce::Image image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
        {
            juce::Graphics g (image);
            editor.paintEntireComponent (g, false);
        }

        target.drawImageAt (image, bounds.getX(), bounds.getY());
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    Themes::loadPreference();

    const auto& presets = Presets::factory();
    const int count = (int) presets.size();

    const int columns = 2;
    const int rows    = (count + columns - 1) / columns;

    const int width  = columns * kCellWidth + kGap;
    const int height = rows * kCellHeight + kGap + 34;

    juce::Image sheet (juce::Image::RGB, width, height, true);

    {
    juce::Graphics g (sheet);

    g.fillAll (background());

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("KARADAG BEAT  -  factory patterns",
                juce::Rectangle<int> (kGap, 8, width, 22),
                juce::Justification::centredLeft);

    for (int i = 0; i < count; ++i)
    {
        const auto& preset = presets[(size_t) i];

        const int col = i / rows;
        const int row = i % rows;

        const int x = kGap + col * kCellWidth;
        const int y = 34 + kGap + row * kCellHeight;

        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (preset.name,
                    juce::Rectangle<int> (x, y, kCellWidth - kGap, kTitleHeight),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.32f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (preset.time.empty()   ? "" : "time ")
                      + juce::String (preset.volume.empty() ? "" : "volume"),
                    juce::Rectangle<int> (x, y, kCellWidth - kGap - 6, kTitleHeight),
                    juce::Justification::centredRight);

        Envelope timeEnv (0.0), volEnv (1.0);

        if (! preset.time.empty())   timeEnv.setPoints (preset.time);
        if (! preset.volume.empty()) volEnv .setPoints (preset.volume);

        renderEnvelope (g, { x, y + kTitleHeight, kCellWidth - kGap, kTimeHeight },
                        timeEnv, false, timeAccent(), true);

        renderEnvelope (g, { x, y + kTitleHeight + kTimeHeight + 2, kCellWidth - kGap, kVolHeight - 2 },
                        volEnv, true, volAccent(), false);
    }
    }

    const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile ("presets.png");

    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream (outFile.createOutputStream());

    if (stream == nullptr || ! png.writeImageToStream (sheet, *stream))
    {
        std::printf ("could not write the PNG\n");
        return 1;
    }

    std::printf ("%d patterns drawn -> %s  (%d x %d)\n",
                 count, outFile.getFullPathName().toRawUTF8(), width, height);
    return 0;
}
