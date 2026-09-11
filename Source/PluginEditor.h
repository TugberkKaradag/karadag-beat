#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "PluginProcessor.h"
#include "EnvelopeEditor.h"

class BeatLookAndFeel  : public juce::LookAndFeel_V4
{
public:
    BeatLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawHighlighted, bool shouldDrawDown) override;

    void refreshFromTheme();
};

class ThemeButton  : public juce::Button
{
public:
    ThemeButton();
    void paintButton (juce::Graphics&, bool shouldDrawHighlighted, bool shouldDrawDown) override;
};

class ChainPanel  : public juce::Component,
                    private juce::Timer
{
public:
    explicit ChainPanel (KaradagBeatProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshEnabledSteps();

    KaradagBeatProcessor& processor;

    juce::ToggleButton enableToggle { "CHAIN ON" };
    juce::ComboBox lengthBox;
    juce::Label title, lengthLabel;

    std::array<juce::ComboBox, KaradagBeatProcessor::kMaxChainSteps> stepBoxes;
    std::array<juce::Label,    KaradagBeatProcessor::kMaxChainSteps> stepLabels;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;
    int lastActiveStep = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainPanel)
};

class KaradagBeatEditor  : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit KaradagBeatEditor (KaradagBeatProcessor&);
    ~KaradagBeatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    friend struct EditorSnapshot;

    struct EnvelopeSnapshot
    {
        std::vector<EnvPoint> time, volume, filter;
    };

    struct LaneControls
    {
        juce::ToggleButton toggle;
        juce::Label        label;
        juce::Label        smoothLabel;
        juce::Slider       smooth { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
        juce::TextButton   shiftLeft { "<" }, shiftRight { ">" };
        juce::ToggleButton draw { "DRAW" };
    };

    void timerCallback() override;

    void drawSignature (juce::Graphics&, juce::Rectangle<int> area) const;

    bool keyPressed (const juce::KeyPress&) override;

    EnvelopeSnapshot captureSnapshot() const;
    void applySnapshot (const EnvelopeSnapshot&);
    void pushUndoStep();
    void undo();
    void redo();
    void updateUndoButtons();

    void refreshAllEditors();

    void refreshSlotNames();

    void promptSaveToSlot();

    void showThemeMenu();

    void showFileMenu();
    void exportPatternFile();
    void importPatternFile();

    void showChainPanel();

    void shiftLane (EnvelopeEditor& editor, Envelope& env, int direction);

    void setupLane (LaneControls&, EnvelopeEditor&, Envelope&,
                    const juce::String& toggleText, const juce::String& smoothTooltip);

    void layoutLane (LaneControls&, EnvelopeEditor&, juce::Rectangle<int> area, bool withFilterExtras);

    void applyThemeColours();

    void refreshGrid();

    void commitSave (int slot, const juce::String& name);

    int chooseTargetSlot() const;

    KaradagBeatProcessor& processor;
    BeatLookAndFeel lookAndFeel;

    juce::TooltipWindow tooltips { this, 700 };

    EnvelopeEditor timeEditor;
    EnvelopeEditor volumeEditor;
    EnvelopeEditor filterEditor;

    LaneControls timeLane, volLane, filterLane;

    juce::ComboBox filterTypeBox;
    juce::Slider   filterResoSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label    filterResoLabel;

    juce::ComboBox presetBox, snapBox, barsBox;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton undoButton { "UNDO" };
    juce::TextButton redoButton { "REDO" };
    juce::TextButton fileButton { "FILE" };
    ThemeButton themeButton;

    juce::Slider mixSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label  mixLabel;

    juce::ToggleButton midiToggle   { "MIDI" };
    juce::ToggleButton latchToggle  { "LATCH" };
    juce::ToggleButton retrigToggle { "RETRIG" };
    juce::ToggleButton chainToggle  { "CHAIN" };
    juce::TextButton   chainEditButton { "..." };
    juce::Slider swingSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label  swingLabel, hintLabel;

    using APVTS = juce::AudioProcessorValueTreeState;
    std::unique_ptr<APVTS::ComboBoxAttachment> presetAttach, barsAttach, filterTypeAttach;
    std::unique_ptr<APVTS::ButtonAttachment>   timeAttach, volAttach, filterAttach,
                                               midiAttach, latchAttach, retrigAttach, chainAttach;
    std::unique_ptr<APVTS::SliderAttachment>   mixAttach, swingAttach, filterResoAttach,
                                               timeSmoothAttach, volSmoothAttach, filterSmoothAttach;

    std::unique_ptr<juce::FileChooser> fileChooser;

    int lastSeenMidiPreset = -1;
    int lastSeenChainStep  = -2;

    std::vector<float> waveform = std::vector<float> ((size_t) KaradagBeatProcessor::waveformBins, 0.0f);

    std::vector<EnvelopeSnapshot> undoStack, redoStack;
    static constexpr int kMaxUndoSteps = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KaradagBeatEditor)
};
