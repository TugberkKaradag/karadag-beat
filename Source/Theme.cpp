#include "Theme.h"
#include <juce_core/juce_core.h>

namespace
{
    std::vector<Theme> buildThemes()
    {
        std::vector<Theme> themes;

        // Asit yesili + sicak magenta, patlican moru zemin.
        themes.push_back ({
            "Toxic Bloom",
            juce::Colour (0xff15101d),   // background
            juce::Colour (0xff1e1729),   // panel
            juce::Colour (0xff110d18),   // plot
            juce::Colour (0x1affffff),   // edge
            juce::Colour (0xffc8f230),   // time   - asit lime
            juce::Colour (0xffff3d9a),   // volume - sicak magenta
            juce::Colour (0xffe9e3f2),   // text
            juce::Colour (0xff8b829c),   // textDim
            juce::Colour (0xfffff2c4)    // playhead
        });

        // Sicak zemin, soguk aksan - cogu eklentinin tam tersi.
        themes.push_back ({
            "Ember",
            juce::Colour (0xff1a100c),
            juce::Colour (0xff261911),
            juce::Colour (0xff150c08),
            juce::Colour (0x1affffff),
            juce::Colour (0xff5eead4),   // time   - mint
            juce::Colour (0xffffb020),   // volume - amber
            juce::Colour (0xfff0e2d6),
            juce::Colour (0xff9c8577),
            juce::Colour (0xffffffff)
        });

        // Gece mavisi zemin, elektrik mor + asit sari.
        themes.push_back ({
            "Ultraviolet",
            juce::Colour (0xff0a0a16),
            juce::Colour (0xff13132b),
            juce::Colour (0xff08080f),
            juce::Colour (0x1affffff),
            juce::Colour (0xffc77dff),   // time   - elektrik mor
            juce::Colour (0xffffe14d),   // volume - asit sari
            juce::Colour (0xffdfe0f5),
            juce::Colour (0xff7a7ca0),
            juce::Colour (0xff00e5ff)
        });

        // Klor mavisi + neon turuncu, koyu petrol zemin.
        themes.push_back ({
            "Chlorine",
            juce::Colour (0xff071417),
            juce::Colour (0xff0d1f23),
            juce::Colour (0xff050f11),
            juce::Colour (0x1affffff),
            juce::Colour (0xff7ff0e8),   // time   - klor
            juce::Colour (0xffff8c42),   // volume - neon turuncu
            juce::Colour (0xffdff0ee),
            juce::Colour (0xff6f8a8c),
            juce::Colour (0xffffffff)
        });

        // Kan portakali + soluk yesil, koyu bordo zemin.
        themes.push_back ({
            "Blood Orange",
            juce::Colour (0xff140609),
            juce::Colour (0xff1f0b10),
            juce::Colour (0xff0f0407),
            juce::Colour (0x1affffff),
            juce::Colour (0xffa8e6a1),   // time   - soluk yesil
            juce::Colour (0xffff4d2e),   // volume - kan portakali
            juce::Colour (0xfff2dfd9),
            juce::Colour (0xff97706a),
            juce::Colour (0xffffe9b0)
        });

        // Kirli lime + toz pembe, yosun zemin.
        themes.push_back ({
            "Moss",
            juce::Colour (0xff0f1410),
            juce::Colour (0xff181f18),
            juce::Colour (0xff0a0f0b),
            juce::Colour (0x1affffff),
            juce::Colour (0xffd4e04a),   // time   - kirli lime
            juce::Colour (0xffe88ba0),   // volume - toz pembe
            juce::Colour (0xffe4ead9),
            juce::Colour (0xff7f8a72),
            juce::Colour (0xfffff5d6)
        });

        // Soluk lila + asit sari, morarmis zemin.
        themes.push_back ({
            "Bruise",
            juce::Colour (0xff120e1a),
            juce::Colour (0xff1b1528),
            juce::Colour (0xff0d0a14),
            juce::Colour (0x1affffff),
            juce::Colour (0xffb8a6ff),   // time   - soluk lila
            juce::Colour (0xffd4e04a),   // volume - asit sari
            juce::Colour (0xffe6e0f0),
            juce::Colour (0xff7d7391),
            juce::Colour (0xffff9ecd)
        });

        // Sari + murekkep mavisi, notr beton zemin - brutalist.
        themes.push_back ({
            "Concrete",
            juce::Colour (0xff17181a),
            juce::Colour (0xff212327),
            juce::Colour (0xff101113),
            juce::Colour (0x22ffffff),
            juce::Colour (0xffffd23f),   // time   - sari
            juce::Colour (0xff5b8cff),   // volume - murekkep mavisi
            juce::Colour (0xffe8e9ea),
            juce::Colour (0xff8a8d92),
            juce::Colour (0xffffffff)
        });

        return themes;
    }

    int currentIndex = 0;
}

const std::vector<Theme>& Themes::all()
{
    static const std::vector<Theme> themes = buildThemes();
    return themes;
}

const Theme& Themes::current()
{
    const auto& themes = all();
    return themes[(size_t) juce::jlimit (0, (int) themes.size() - 1, currentIndex)];
}

void Themes::setCurrent (int index)
{
    currentIndex = juce::jlimit (0, (int) all().size() - 1, index);
}

int Themes::getCurrentIndex()
{
    return currentIndex;
}

//==============================================================================
namespace
{
    juce::File preferenceFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("Karadag")
                 .getChildFile ("KaradagBeat")
                 .getChildFile ("settings.xml");
    }
}

void Themes::loadPreference()
{
    const auto file = preferenceFile();

    if (! file.existsAsFile())
        return;

    if (const auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName ("KaradagBeatSettings"))
            setCurrent (xml->getIntAttribute ("theme", 0));
}

void Themes::savePreference()
{
    juce::XmlElement xml ("KaradagBeatSettings");
    xml.setAttribute ("theme", getCurrentIndex());

    const auto file = preferenceFile();
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}
