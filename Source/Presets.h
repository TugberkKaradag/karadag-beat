#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "Envelope.h"

struct GrossPreset
{
    juce::String          name;
    std::vector<EnvPoint> time;
    std::vector<EnvPoint> volume;
    std::vector<EnvPoint> filter {};

    bool isEmpty() const noexcept { return time.empty() && volume.empty() && filter.empty(); }
};

namespace Presets
{
    const std::vector<GrossPreset>& factory();

    int numPresets();
    juce::StringArray names();

    constexpr int kNumSlots = 48;

    bool isUserSlot (int index);

    juce::StringArray slotNames();

    juce::String defaultDisplayName (int index);

    std::vector<GrossPreset> makeDefaultSlots();
}
