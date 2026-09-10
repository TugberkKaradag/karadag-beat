#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "EnvelopeEditor.h"

/** Koyu, duz bir tema. */
class BeatLookAndFeel  : public juce::LookAndFeel_V4
{
public:
    BeatLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawHighlighted, bool shouldDrawDown) override;

    /** Palet degistiginde renkleri yeniden okur. */
    void refreshFromTheme();
};

/** Kompakt palet dugmesi: o anki temanin iki aksan rengini nokta olarak gosterir. */
class ThemeButton  : public juce::Button
{
public:
    ThemeButton();
    void paintButton (juce::Graphics&, bool shouldDrawHighlighted, bool shouldDrawDown) override;
};

//==============================================================================
class KaradagBeatEditor  : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit KaradagBeatEditor (KaradagBeatProcessor&);
    ~KaradagBeatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Iki zarfin da o anki hali - geri alma yigininda tutulan birim. */
    struct EnvelopeSnapshot
    {
        std::vector<EnvPoint> time, volume;
    };

    void timerCallback() override;

    /** Ust barin solundaki imza: basamak + egri glifi ve wordmark. */
    void drawSignature (juce::Graphics&, juce::Rectangle<int> area) const;

    bool keyPressed (const juce::KeyPress&) override;

    EnvelopeSnapshot captureSnapshot() const;
    void applySnapshot (const EnvelopeSnapshot&);
    void pushUndoStep();
    void undo();
    void redo();
    void updateUndoButtons();

    /** Combobox'taki slot adlarini processor'dakilerle esitler. */
    void refreshSlotNames();

    /** Cizili zarflari bir kullanici slotuna kaydetmek icin isim sorar. */
    void promptSaveToSlot();

    /** Palet menusunu acar ve secimi diske yazar. */
    void showThemeMenu();

    /** FILE menusu: pattern disa / ice aktarma. */
    void showFileMenu();
    void exportPatternFile();
    void importPatternFile();

    /** Bir lane'i bir izgara adimi ileri ya da geri kaydirir. */
    void shiftLane (EnvelopeEditor& editor, Envelope& env, int direction);

    /** Renk bagimli her seyi o anki paletten tazeler. */
    void applyThemeColours();

    /** Snap izgarasini pattern uzunluguna gore yeniden hesaplar. */
    void refreshGrid();

    /** Zarflari slota yazar, listeyi tazeler ve slotu secer. */
    void commitSave (int slot, const juce::String& name);

    /** Kaydetmek icin hedef slot: secili slot kullaniciysa o, degilse ilk bos slot. */
    int chooseTargetSlot() const;

    KaradagBeatProcessor& processor;
    BeatLookAndFeel lookAndFeel;

    // setTooltip cagrilari ancak boyle bir pencere varsa gorunur hale gelir
    juce::TooltipWindow tooltips { this, 700 };

    EnvelopeEditor timeEditor;
    EnvelopeEditor volumeEditor;

    juce::ComboBox presetBox, snapBox, barsBox;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton undoButton { "UNDO" };
    juce::TextButton redoButton { "REDO" };
    juce::TextButton fileButton { "FILE" };
    juce::ToggleButton latchToggle { "LATCH" };

    // lane basliklarindaki araclar: cizim modu ve bir izgara adimi kaydirma
    juce::ToggleButton timeDrawToggle { "DRAW" }, volDrawToggle { "DRAW" };
    juce::TextButton   timeShiftLeft  { "<" }, timeShiftRight { ">" };
    juce::TextButton   volShiftLeft   { "<" }, volShiftRight  { ">" };

    // lane basina yumusatma (ms)
    juce::Slider timeSmoothSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider volSmoothSlider  { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label  timeSmoothLabel, volSmoothLabel;
    ThemeButton themeButton;
    juce::ToggleButton timeToggle  { "TIME" };
    juce::ToggleButton volToggle   { "VOLUME" };
    juce::ToggleButton midiToggle  { "MIDI" };
    juce::Slider mixSlider { juce::Slider::RotaryHorizontalVerticalDrag,
                             juce::Slider::NoTextBox };

    juce::Label timeLabel, volLabel, hintLabel, mixLabel;

    using APVTS = juce::AudioProcessorValueTreeState;
    std::unique_ptr<APVTS::ComboBoxAttachment> presetAttach, barsAttach;
    std::unique_ptr<APVTS::ButtonAttachment>   timeAttach, volAttach, midiAttach, latchAttach;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<APVTS::SliderAttachment>   mixAttach, timeSmoothAttach, volSmoothAttach;

    int lastSeenMidiPreset = -1;

    std::vector<float> waveform = std::vector<float> ((size_t) KaradagBeatProcessor::waveformBins, 0.0f);

    std::vector<EnvelopeSnapshot> undoStack, redoStack;
    static constexpr int kMaxUndoSteps = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KaradagBeatEditor)
};
