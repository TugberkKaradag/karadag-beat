#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
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
/** Zincir duzenleyici: CHAIN'in yanindaki "..." ile acilan kucuk panel.
    Her adim bir slot ya da editordeki cizim; uzunluk 1-8 adim. */
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
    // Tests/EditorRender.cpp: ekran disi goruntu alirken zamanlayiciyi elle ilerletir
    friend struct EditorSnapshot;

    /** Uc zarfin da o anki hali - geri alma yigininda tutulan birim. */
    struct EnvelopeSnapshot
    {
        std::vector<EnvPoint> time, volume, filter;
    };

    /** Bir lane'in basligindaki araclar: ac/kapa, etiket, yumusatma, kaydirma, cizim. */
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

    /** Ust barin solundaki imza: basamak + egri glifi ve wordmark. */
    void drawSignature (juce::Graphics&, juce::Rectangle<int> area) const;

    bool keyPressed (const juce::KeyPress&) override;

    EnvelopeSnapshot captureSnapshot() const;
    void applySnapshot (const EnvelopeSnapshot&);
    void pushUndoStep();
    void undo();
    void redo();
    void updateUndoButtons();

    /** Uc editoru disaridan degisen zarflar icin tazeler. */
    void refreshAllEditors();

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

    /** Zincir panelini CHAIN'in yaninda acar. */
    void showChainPanel();

    /** Bir lane'i bir izgara adimi ileri ya da geri kaydirir. */
    void shiftLane (EnvelopeEditor& editor, Envelope& env, int direction);

    /** Lane basligindaki araclari kurar (ortak kisim). */
    void setupLane (LaneControls&, EnvelopeEditor&, Envelope&,
                    const juce::String& toggleText, const juce::String& smoothTooltip);

    /** Lane basligini ve editoru verilen alana yerlestirir.
        Filtre lane'inde baslik tip secici ve rezonansi da tasir. */
    void layoutLane (LaneControls&, EnvelopeEditor&, juce::Rectangle<int> area, bool withFilterExtras);

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
    EnvelopeEditor filterEditor;

    LaneControls timeLane, volLane, filterLane;

    // filtre lane'ine ozel
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

    // alt bar: MIDI, zincir, swing
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
