#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "Envelope.h"

struct GrossPreset
{
    juce::String          name;
    std::vector<EnvPoint> time;      // bos ise duz 0 (canli)
    std::vector<EnvPoint> volume;    // bos ise duz 1 (tam ses)
    std::vector<EnvPoint> filter {}; // bos ise duz 1 (filtre acik)

    bool isEmpty() const noexcept { return time.empty() && volume.empty() && filter.empty(); }
};

namespace Presets
{
    /** Kalici olarak yuklenmis fabrika pattern'leri. */
    const std::vector<GrossPreset>& factory();

    int numPresets();
    juce::StringArray names();

    /** Toplam slot sayisi: fabrika pattern'leri + kullanicinin kaydedebilecegi bos slotlar.
        Sabit tutulmali - VST parametresinin secenek listesi calisma aninda degisemez. */
    constexpr int kNumSlots = 48;

    /** Bir slotun kullanici tarafindan yazilabilir olup olmadigi. */
    bool isUserSlot (int index);

    /** Parametre icin sabit slot isimleri ("1 Off" ... "48 User 27").
        VST parametresinin secenek listesi bu - asla degismemeli. */
    juce::StringArray slotNames();

    /** Slotun numarasiz varsayilan gorunen adi ("Off", "User 2").
        GUI numarayi kendisi ekledigi icin burada numara olmamali. */
    juce::String defaultDisplayName (int index);

    /** Fabrika slotlari + bos kullanici slotlari - baslangic durumu. */
    std::vector<GrossPreset> makeDefaultSlots();
}
