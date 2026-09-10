#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Envelope.h"
#include "GrossEngine.h"
#include "Presets.h"

class KaradagBeatProcessor  : public juce::AudioProcessor
{
public:
    KaradagBeatProcessor();
    ~KaradagBeatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return "Karadag Beat"; }
    bool acceptsMidi() const override                   { return true; }
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- editor arayuzu ----------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    Envelope& getEditableTimeEnvelope()   noexcept { return editTime; }
    Envelope& getEditableVolumeEnvelope() noexcept { return editVolume; }

    /** GUI bir zarfi degistirdikten sonra cagirir: degisikligi ses thread'ine yayinlar. */
    void publishEnvelopes();

    /** Slottaki pattern'i GUI zarflarina yukler ve yayinlar (mesaj thread'i). */
    void loadSlotIntoEditor (int index, bool alsoPublish = true);

    /** GUI'de cizili olan zarflari bir kullanici slotuna kaydeder ve diske yazar. */
    bool saveEditorToSlot (int index, const juce::String& name);

    /** Slotun gorunen adi - kullanici kaydettiyse verdigi ad, yoksa sabit slot adi. */
    juce::String getSlotName (int index) const;

    /** Kullanici bu slota bir pattern kaydetmis mi? */
    bool isSlotFilled (int index) const;

    /** MIDI ile o an tetiklenen preset, yoksa -1. */
    int  getMidiPreset() const noexcept { return midiActive.load() ? midiPreset.load() : -1; }

    /** Calan kafanin pattern icindeki konumu, 0..1 - GUI cizimi icin. */
    double getPlayheadPhase() const noexcept { return displayPhase.load(); }

    /** Pattern kac bar surer (1, 2 veya 4) - GUI izgarasi buna gore cizilir. */
    int getPatternBars() const noexcept;

    /** Cizili zarflari (ve pattern uzunlugunu) paylasilabilir bir dosyaya yazar. */
    bool exportPattern (const juce::File& file, const juce::String& name) const;

    /** Bir pattern dosyasini GUI zarflarina yukler ve sese yayinlar.
        Basarili olursa dosyadaki adi loadedName'e yazar. */
    bool importPattern (const juce::File& file, juce::String& loadedName);

    static constexpr const char* patternFileExtension = ".kbeat";

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void handleMidiTriggers (juce::MidiBuffer& midi);
    void applySlotToAudioEnvelopes (int index);

    static juce::File getUserPatternFile();
    void loadUserSlotsFromDisk();
    void saveUserSlotsToDisk() const;

    GrossEngine engine;

    // 36 slot: ilk kisim fabrika, kalani kullanici.  GUI yazar, ses thread'i
    // try-lock ile okur; kilit alinamazsa yukleme bir sonraki bloga kalir.
    mutable juce::SpinLock slotLock;
    std::vector<GrossPreset> slots { Presets::makeDefaultSlots() };

    // GUI tarafi (yalnizca mesaj thread'i)
    Envelope editTime   { 0.0 };
    Envelope editVolume { 1.0 };

    // yayin kutusu
    juce::SpinLock publishLock;
    Envelope pendingTime   { 0.0 };
    Envelope pendingVolume { 1.0 };
    std::atomic<bool> hasPending { false };

    // ses thread'inin kopyalari
    Envelope audioTime   { 0.0 };
    Envelope audioVolume { 1.0 };

    int lastRequestedPreset = -1;    // en son YUKLENEN preset (yalnizca ses thread'i)

    std::atomic<double> displayPhase { 0.0 };

    // parametreler
    std::atomic<float>* pPreset      = nullptr;
    std::atomic<float>* pPatternBars = nullptr;
    std::atomic<float>* pTimeOn      = nullptr;
    std::atomic<float>* pVolOn       = nullptr;
    std::atomic<float>* pMix         = nullptr;
    std::atomic<float>* pMidiTrigger = nullptr;
    std::atomic<float>* pMidiLatch   = nullptr;

    // MIDI tetikleme
    juce::SortedSet<int> heldNotes;
    int  latchedNote = -1;       // latch modunda acik tutulan nota (yalnizca ses thread'i)
    bool wasLatching = false;
    std::atomic<bool> midiActive { false };
    std::atomic<int>  midiPreset { 0 };
    static constexpr int midiBaseNote = 60;   // C4 -> ilk preset

    double sampleRateHz = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KaradagBeatProcessor)
};
