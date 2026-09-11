#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Envelope.h"
#include "BeatEngine.h"
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
    double getTailLengthSeconds() const override        { return tailSeconds.load(); }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    Envelope& getEditableTimeEnvelope()   noexcept { return editTime; }
    Envelope& getEditableVolumeEnvelope() noexcept { return editVolume; }
    Envelope& getEditableFilterEnvelope() noexcept { return editFilter; }

    void publishEnvelopes();

    void loadSlotIntoEditor (int index);

    bool saveEditorToSlot (int index, const juce::String& name);

    void reloadUserSlots();

    static void setUserPatternFileForTesting (const juce::File& file);

    juce::String getSlotName (int index) const;

    bool isSlotFilled (int index) const;

    void showSlotInEditor (int index);

    void restoreDrawingInEditor();

    bool isEditorShowingSlot() const noexcept { return editorShowsSlot.load(); }

    int  getMidiPreset() const noexcept { return midiActive.load() ? midiPreset.load() : -1; }

    double getPlayheadPhase() const noexcept { return displayPhase.load(); }

    static constexpr int waveformBins = BeatEngine::kWaveBins;
    float getWaveformPeak (int bin) const noexcept { return engine.getWavePeak (bin); }

    double patternToRealPhase (double patternPhase) const noexcept;

    int getPatternBars() const noexcept;

    static constexpr int kMaxChainSteps = 8;
    static constexpr int kChainDrawing  = -1;

    int  getChainStep (int step) const noexcept;
    void setChainStep (int step, int slotOrDrawing) noexcept;
    int  getChainLength() const noexcept           { return chainLength.load(); }
    void setChainLength (int steps) noexcept;

    int  getActiveChainStep() const noexcept       { return activeChainStep.load(); }

    bool exportPattern (const juce::File& file, const juce::String& name) const;

    bool importPattern (const juce::File& file, juce::String& loadedName);

    static constexpr const char* patternFileExtension = ".kbeat";

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    bool beginMidiBlock();

    bool handleMidiMessage (const juce::MidiMessage& m);

    void updatePatternSources();

    int resolveOverride() noexcept;

    void refreshPlayEnvelopes();

    void copySlot (int index, Envelope& time, Envelope& volume, Envelope& filter) const;

    static juce::File getUserPatternFile();
    void loadUserSlotsFromDisk();
    void saveUserSlotsToDisk() const;

    BeatEngine engine;

    mutable juce::SpinLock slotLock;
    std::vector<Pattern> slots { Presets::makeDefaultSlots() };

    Envelope editTime   { 0.0 };
    Envelope editVolume { 1.0 };
    Envelope editFilter { 1.0 };

    Envelope stashTime   { 0.0 };
    Envelope stashVolume { 1.0 };
    Envelope stashFilter { 1.0 };
    std::atomic<bool> editorShowsSlot { false };

    juce::SpinLock publishLock;
    Envelope pendingTime   { 0.0 };
    Envelope pendingVolume { 1.0 };
    Envelope pendingFilter { 1.0 };
    bool pendingIsTweak = false;
    std::atomic<bool> hasPending { false };

    Envelope baseTime   { 0.0 };
    Envelope baseVolume { 1.0 };
    Envelope baseFilter { 1.0 };

    Envelope audioTime   { 0.0 };
    Envelope audioVolume { 1.0 };
    Envelope audioFilter { 1.0 };

    Envelope playTime   { 0.0 };
    Envelope playVolume { 1.0 };
    Envelope playFilter { 1.0 };
    std::vector<EnvPoint> swingScratch;

    int  lastParamPreset = -1;
    int  activeOverride  = -1;
    bool baseChanged     = false;
    bool audioChanged    = true;
    bool swingOn         = false;
    double appliedSwing  = -1.0;
    int    appliedCells  = -1;

    std::atomic<double> displayPhase { 0.0 };
    std::atomic<double> tailSeconds { 4.0 };
    std::atomic<double> swingAmountForGui { 0.5 };
    std::atomic<int>    swingCellsForGui  { 32 };

    std::atomic<float>* pPreset       = nullptr;
    std::atomic<float>* pPatternBars  = nullptr;
    std::atomic<float>* pTimeOn       = nullptr;
    std::atomic<float>* pVolOn        = nullptr;
    std::atomic<float>* pFilterOn     = nullptr;
    std::atomic<float>* pMix          = nullptr;
    std::atomic<float>* pMidiTrigger  = nullptr;
    std::atomic<float>* pMidiLatch    = nullptr;
    std::atomic<float>* pMidiRetrig   = nullptr;
    std::atomic<float>* pTimeSmooth   = nullptr;
    std::atomic<float>* pVolSmooth    = nullptr;
    std::atomic<float>* pFilterSmooth = nullptr;
    std::atomic<float>* pFilterType   = nullptr;
    std::atomic<float>* pFilterReso   = nullptr;
    std::atomic<float>* pSwing        = nullptr;
    std::atomic<float>* pChainOn      = nullptr;

    juce::SortedSet<int> heldNotes;
    int  latchedNote = -1;
    bool wasLatching = false;
    bool retriggered = false;
    std::atomic<bool> midiActive { false };
    std::atomic<int>  midiPreset { 0 };
    static constexpr int midiBaseNote = 60;

    std::array<std::atomic<int>, kMaxChainSteps> chainSteps;
    std::atomic<int>  chainLength { 4 };
    std::atomic<int>  activeChainStep { -1 };
    juce::int64 chainCycle = 0;

    double sampleRateHz = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KaradagBeatProcessor)
};
