#include "Presets.h"

namespace
{
    std::vector<EnvPoint> makeRepeat (int steps)
    {
        std::vector<EnvPoint> pts;
        const double stepPhase = 1.0 / (double) steps;

        for (int i = 0; i < steps; ++i)
        {
            const double back = (i % 2 == 0) ? 0.0 : stepPhase;
            pts.emplace_back ((double) i * stepPhase, back, 0.0, true);
        }

        return pts;
    }

    std::vector<EnvPoint> makeGate (int steps, double openAmount = 1.0, double closedAmount = 0.0)
    {
        std::vector<EnvPoint> pts;
        const double stepPhase = 1.0 / (double) steps;

        for (int i = 0; i < steps; ++i)
            pts.emplace_back ((double) i * stepPhase,
                              (i % 2 == 0) ? openAmount : closedAmount,
                              0.0, true);

        return pts;
    }

    std::vector<EnvPoint> makeLoopFrom (double startPhase, double sliceLen)
    {
        std::vector<EnvPoint> pts;
        pts.emplace_back (0.0, 0.0, 0.0, true);

        int k = 0;

        for (double x = startPhase; x < 1.0 - 1.0e-9; x += sliceLen, ++k)
            pts.emplace_back (x, (double) (k + 1) * sliceLen, 0.0, true);

        return pts;
    }

    std::vector<EnvPoint> makeReverse (int steps)
    {
        std::vector<EnvPoint> pts;
        const double step = 1.0 / (double) steps;

        for (int i = 0; i < steps; ++i)
        {
            pts.emplace_back ((double) i * step, 0.0, 0.0, false);

            pts.emplace_back ((double) (i + 1) * step, 2.0 * step, 0.0, false);
        }

        return pts;
    }

    std::vector<EnvPoint> makePump (int beats, double tension = 0.55, double holdRatio = 0.92)
    {
        std::vector<EnvPoint> pts;
        const double beatPhase = 1.0 / (double) beats;

        for (int i = 0; i < beats; ++i)
        {
            const double start = (double) i * beatPhase;
            pts.emplace_back (start, 0.0, tension, false);
            pts.emplace_back (start + beatPhase * holdRatio, 1.0, 0.0, true);
        }

        return pts;
    }

    std::vector<GrossPreset> buildFactory()
    {
        std::vector<GrossPreset> p;

        p.push_back ({ "Off", {}, {} });

        p.push_back ({ "Half Time",
                       { { 0.0, 0.0 }, { 1.0, 0.5 } },
                       {} });

        p.push_back ({ "Quarter Time",
                       { { 0.0, 0.0 }, { 1.0, 0.75 } },
                       {} });

        p.push_back ({ "Freeze 2nd Half",  makeLoopFrom (0.5,   1.0 / 16.0), {} });
        p.push_back ({ "Freeze Last Beat", makeLoopFrom (0.875, 1.0 / 32.0), {} });

        p.push_back ({ "Tape Stop",
                       { { 0.0, 0.0, -0.25, false }, { 0.8, 0.4, 0.0, true } },
                       { { 0.0, 1.0 }, { 0.72, 1.0 }, { 0.8, 0.0, 0.0, true } } });

        p.push_back ({ "Scratch",
                       { { 0.0,   0.0    },
                         { 0.25,  0.125  },
                         { 0.375, 0.375  },
                         { 0.625, 0.25   },
                         { 0.75,  0.5625 },
                         { 1.0,   0.25   } },
                       {} });

        p.push_back ({ "Repeat 1/4",  makeRepeat (8),  {} });
        p.push_back ({ "Repeat 1/8",  makeRepeat (16), {} });
        p.push_back ({ "Repeat 1/16", makeRepeat (32), {} });

        p.push_back ({ "Gate 1/8",  {}, makeGate (16) });
        p.push_back ({ "Gate 1/16", {}, makeGate (32) });
        p.push_back ({ "Sidechain 1/4", {}, makePump (8) });
        p.push_back ({ "Sidechain 1/8", {}, makePump (16) });

        p.push_back ({ "Reverse 1/4", makeReverse (8),  {} });
        p.push_back ({ "Reverse 1/8", makeReverse (16), {} });

        p.push_back ({ "Double Time",
                       { { 0.0, 1.0 }, { 1.0, 0.0 } },
                       {} });

        p.push_back ({ "Backspin",
                       { { 0.0, 0.0 }, { 0.75, 0.0 }, { 1.0, 0.6, -0.5, false } },
                       { { 0.0, 1.0 }, { 0.75, 1.0 }, { 1.0, 0.0 } } });

        p.push_back ({ "Gate 1/4", {}, makeGate (8) });

        p.push_back ({ "Stutter + Gate", makeRepeat (16), makeGate (32, 1.0, 0.25) });
        p.push_back ({ "Half Time + Pump",
                       { { 0.0, 0.0 }, { 1.0, 0.5 } },
                       makePump (8) });

        return p;
    }
}

const std::vector<GrossPreset>& Presets::factory()
{
    static const std::vector<GrossPreset> presets = buildFactory();
    return presets;
}

int Presets::numPresets()
{
    return (int) factory().size();
}

juce::StringArray Presets::names()
{
    juce::StringArray n;

    for (const auto& p : factory())
        n.add (p.name);

    return n;
}

bool Presets::isUserSlot (int index)
{
    return index >= numPresets() && index < kNumSlots;
}

juce::StringArray Presets::slotNames()
{
    juce::StringArray result;

    const auto factoryNames = names();

    for (int i = 0; i < kNumSlots; ++i)
    {
        const juce::String prefix (i + 1);

        if (i < factoryNames.size())
            result.add (prefix + " " + factoryNames[i]);
        else
            result.add (prefix + " User " + juce::String (i - factoryNames.size() + 1));
    }

    return result;
}

juce::String Presets::defaultDisplayName (int index)
{
    const auto& all = factory();

    if (juce::isPositiveAndBelow (index, (int) all.size()))
        return all[(size_t) index].name;

    return "User " + juce::String (index - numPresets() + 1);
}

std::vector<GrossPreset> Presets::makeDefaultSlots()
{
    std::vector<GrossPreset> slots = factory();
    slots.reserve ((size_t) kNumSlots);

    while ((int) slots.size() < kNumSlots)
        slots.push_back ({ defaultDisplayName ((int) slots.size()), {}, {}, {} });

    return slots;
}
