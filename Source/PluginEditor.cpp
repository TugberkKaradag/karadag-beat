#include "PluginEditor.h"
#include "Brand.h"
#include "Theme.h"

namespace
{
    inline const Theme& th()  { return Themes::current(); }

    const char* const kHintText =
        "click  add / move      right-click  delete      double-click  step      drag o  bend curve"
        "      alt-drag or DRAW  paint steps      shift  free      notes from C4  trigger";

    const char* const kVolumeLabel = "VOLUME   gate, sidechain, pump";

    juce::File patternFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("Karadag Beat Patterns");
        dir.createDirectory();
        return dir;
    }

    juce::String timeLabelText (int bars)
    {
        return "TIME   top: live   bottom: " + juce::String (bars)
                 + (bars == 1 ? " bar back" : " bars back");
    }

    juce::String filterLabelText (bool highPass)
    {
        return highPass ? "FILTER   high-pass   top: open   bottom: thin"
                        : "FILTER   low-pass   top: open   bottom: dark";
    }

    void styleLabel (juce::Label& label, const juce::String& text,
                     float fontHeight, juce::Colour colour,
                     juce::Justification just = juce::Justification::centredLeft)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::FontOptions (fontHeight, juce::Font::bold));
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (just);
    }

    constexpr int kDrawingItemId = 1;

    void fillSlotChoices (juce::ComboBox& box, const KaradagBeatProcessor& processor)
    {
        box.clear (juce::dontSendNotification);
        box.addItem ("Drawing", kDrawingItemId);
        box.addSeparator();

        for (int i = 0; i < Presets::kNumSlots; ++i)
        {
            if (i == Presets::numPresets())
                box.addSeparator();

            box.addItem (juce::String (i + 1) + " " + processor.getSlotName (i), i + 2);
        }
    }
}

BeatLookAndFeel::BeatLookAndFeel()
{
    refreshFromTheme();
}

void BeatLookAndFeel::refreshFromTheme()
{
    setColour (juce::ComboBox::backgroundColourId,   th().panel);
    setColour (juce::ComboBox::textColourId,         th().text);
    setColour (juce::ComboBox::outlineColourId,      th().edge);
    setColour (juce::ComboBox::arrowColourId,        th().textDim);
    setColour (juce::PopupMenu::backgroundColourId,  th().panel);
    setColour (juce::PopupMenu::textColourId,        th().text);
    setColour (juce::PopupMenu::headerTextColourId,  th().textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, th().timeAccent.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId,       th().text);
    setColour (juce::TooltipWindow::backgroundColourId, th().panel);
    setColour (juce::TooltipWindow::textColourId,       th().text);
    setColour (juce::TooltipWindow::outlineColourId,    th().edge);
    setColour (juce::AlertWindow::backgroundColourId, th().panel);
    setColour (juce::AlertWindow::textColourId,       th().text);
    setColour (juce::AlertWindow::outlineColourId,    th().edge);
    setColour (juce::TextEditor::backgroundColourId,  th().plot);
    setColour (juce::TextEditor::textColourId,        th().text);
    setColour (juce::TextEditor::highlightColourId,   th().timeAccent.withAlpha (0.3f));
    setColour (juce::TextButton::buttonColourId,      th().panel);
    setColour (juce::TextButton::textColourOffId,     th().text);
    setColour (juce::Label::textColourId,             th().text);
    setColour (juce::BubbleComponent::backgroundColourId, th().panel);
    setColour (juce::BubbleComponent::outlineColourId,    th().edge);
}

void BeatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle,
                                        juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float lineW = juce::jmax (2.0f, radius * 0.16f);
    const float arcR  = radius - lineW * 0.5f;

    juce::Path back;
    back.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (th().text.withAlpha (0.12f));
    g.strokePath (back, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (th().timeAccent);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Point<float> tip (centre.x + arcR * std::sin (angle),
                            centre.y - arcR * std::cos (angle));
    g.setColour (th().text);
    g.fillEllipse (juce::Rectangle<float> (lineW * 1.1f, lineW * 1.1f).withCentre (tip));
}

void BeatLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawHighlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);

    g.setColour (on ? accent.withAlpha (0.22f) : th().text.withAlpha (0.05f));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (on ? accent : th().text.withAlpha (shouldDrawHighlighted ? 0.35f : 0.18f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.2f);

    g.setColour (on ? accent.brighter (0.3f) : th().text.withAlpha (0.55f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

ThemeButton::ThemeButton()  : juce::Button ("theme")
{
    setTooltip ("Colour theme");
}

void ThemeButton::paintButton (juce::Graphics& g, bool shouldDrawHighlighted, bool)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (th().text.withAlpha (shouldDrawHighlighted ? 0.09f : 0.05f));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (th().edge);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const float r = 4.2f;
    const auto c = bounds.getCentre();

    g.setColour (th().timeAccent);
    g.fillEllipse (juce::Rectangle<float> (r, r).withCentre ({ c.x - 5.2f, c.y }));

    g.setColour (th().volumeAccent);
    g.fillEllipse (juce::Rectangle<float> (r, r).withCentre ({ c.x, c.y }));

    g.setColour (th().filterAccent);
    g.fillEllipse (juce::Rectangle<float> (r, r).withCentre ({ c.x + 5.2f, c.y }));
}

ChainPanel::ChainPanel (KaradagBeatProcessor& p)  : processor (p)
{
    styleLabel (title, "PATTERN CHAIN   one step per pattern loop", 10.5f, th().timeAccent.withAlpha (0.85f));
    styleLabel (lengthLabel, "STEPS", 9.5f, th().textDim, juce::Justification::centredRight);
    addAndMakeVisible (title);
    addAndMakeVisible (lengthLabel);

    enableToggle.setColour (juce::TextButton::buttonOnColourId, th().timeAccent);
    enableToggle.setTooltip ("Play the steps in order, one per pattern loop.  MIDI notes still win.");
    addAndMakeVisible (enableToggle);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                       processor.apvts, "chainOn", enableToggle);

    for (int i = 1; i <= KaradagBeatProcessor::kMaxChainSteps; ++i)
        lengthBox.addItem (juce::String (i), i);

    lengthBox.setSelectedId (processor.getChainLength(), juce::dontSendNotification);
    lengthBox.onChange = [this]
    {
        processor.setChainLength (lengthBox.getSelectedId());
        refreshEnabledSteps();
    };
    addAndMakeVisible (lengthBox);

    for (int i = 0; i < KaradagBeatProcessor::kMaxChainSteps; ++i)
    {
        auto& box = stepBoxes[(size_t) i];
        fillSlotChoices (box, processor);

        const int value = processor.getChainStep (i);
        box.setSelectedId (value < 0 ? kDrawingItemId : value + 2, juce::dontSendNotification);

        box.onChange = [this, i]
        {
            const int id = stepBoxes[(size_t) i].getSelectedId();
            processor.setChainStep (i, id == kDrawingItemId ? KaradagBeatProcessor::kChainDrawing : id - 2);
        };

        box.setTooltip ("\"Drawing\" plays what is drawn in the editor");
        addAndMakeVisible (box);

        styleLabel (stepLabels[(size_t) i], juce::String (i + 1), 11.0f, th().textDim,
                    juce::Justification::centredRight);
        addAndMakeVisible (stepLabels[(size_t) i]);
    }

    refreshEnabledSteps();
    setSize (470, 186);
    startTimerHz (15);
}

void ChainPanel::refreshEnabledSteps()
{
    const int length = processor.getChainLength();

    for (int i = 0; i < KaradagBeatProcessor::kMaxChainSteps; ++i)
    {
        stepBoxes [(size_t) i].setEnabled (i < length);
        stepLabels[(size_t) i].setAlpha (i < length ? 1.0f : 0.35f);
    }
}

void ChainPanel::timerCallback()
{
    const int active = processor.getActiveChainStep();

    if (active != lastActiveStep)
    {
        lastActiveStep = active;

        for (int i = 0; i < KaradagBeatProcessor::kMaxChainSteps; ++i)
            stepLabels[(size_t) i].setColour (juce::Label::textColourId,
                                              i == active ? th().timeAccent : th().textDim);
    }
}

void ChainPanel::paint (juce::Graphics& g)
{
    g.fillAll (th().panel);
}

void ChainPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 10);

    auto header = area.removeFromTop (24);
    enableToggle.setBounds (header.removeFromRight (86).reduced (0, 1));
    header.removeFromRight (10);
    lengthBox  .setBounds (header.removeFromRight (52).reduced (0, 1));
    lengthLabel.setBounds (header.removeFromRight (44));
    title.setBounds (header);

    area.removeFromTop (10);

    const int rows = KaradagBeatProcessor::kMaxChainSteps / 2;
    const int rowHeight = area.getHeight() / rows;
    const int columnWidth = area.getWidth() / 2;

    for (int i = 0; i < KaradagBeatProcessor::kMaxChainSteps; ++i)
    {
        const int column = i / rows;
        const int row    = i % rows;

        auto cell = juce::Rectangle<int> (area.getX() + column * columnWidth,
                                          area.getY() + row * rowHeight,
                                          columnWidth, rowHeight).reduced (4, 3);

        stepLabels[(size_t) i].setBounds (cell.removeFromLeft (20));
        cell.removeFromLeft (6);
        stepBoxes[(size_t) i].setBounds (cell);
    }
}

KaradagBeatEditor::KaradagBeatEditor (KaradagBeatProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      timeEditor   (p.getEditableTimeEnvelope(),   EnvelopeEditor::Style::time),
      volumeEditor (p.getEditableVolumeEnvelope(), EnvelopeEditor::Style::volume),
      filterEditor (p.getEditableFilterEnvelope(), EnvelopeEditor::Style::filter)
{
    setLookAndFeel (&lookAndFeel);

    timeEditor.setShowRuler (true);

    setupLane (timeLane, timeEditor, p.getEditableTimeEnvelope(), "TIME",
               "Crossfade length at time jumps  -  longer gives washier stutters");
    setupLane (volLane, volumeEditor, p.getEditableVolumeEnvelope(), "VOLUME",
               "How fast the volume may change  -  longer turns hard gates into a soft tremolo");
    setupLane (filterLane, filterEditor, p.getEditableFilterEnvelope(), "FILTER",
               "How fast the cutoff may move  -  longer gives smooth sweeps from steps");

    timeLane  .toggle.setTooltip ("Enable the time envelope");
    volLane   .toggle.setTooltip ("Enable the volume envelope");
    filterLane.toggle.setTooltip ("Enable the filter envelope  (fully open = no filtering at all)");

    filterTypeBox.addItemList ({ "LP", "HP" }, 1);
    filterTypeBox.setTooltip ("Low-pass darkens, high-pass thins out");
    addAndMakeVisible (filterTypeBox);

    filterResoSlider.setTooltip ("Filter resonance");
    filterResoSlider.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (filterResoSlider);
    addAndMakeVisible (filterResoLabel);

    {
        const auto names = Presets::slotNames();

        presetBox.addSectionHeading ("FACTORY");

        for (int i = 0; i < Presets::kNumSlots; ++i)
        {
            if (i == Presets::numPresets())
                presetBox.addSectionHeading ("YOUR PATTERNS");

            presetBox.addItem (names[i], i + 1);
        }
    }
    refreshSlotNames();

    presetAttach = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, "preset", presetBox);

    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();

        if (index >= 0)
        {
            pushUndoStep();
            processor.loadSlotIntoEditor (index);
            refreshAllEditors();
        }
    };
    addAndMakeVisible (presetBox);

    saveButton.onClick = [this] { promptSaveToSlot(); };
    addAndMakeVisible (saveButton);

    for (auto* b : { &undoButton, &redoButton })
        addAndMakeVisible (b);

    undoButton.setTooltip ("Undo  (Ctrl+Z)");
    redoButton.setTooltip ("Redo  (Ctrl+Shift+Z)");
    saveButton.setTooltip ("Store the drawn envelopes in a user slot");
    presetBox .setTooltip ("Pattern slot  -  1-21 factory, 22-48 your own");
    snapBox   .setTooltip ("Grid the points snap to  (hold Shift to drag freely)");
    mixSlider .setTooltip ("Blend between the dry input and the processed signal");

    themeButton.onClick = [this] { showThemeMenu(); };
    addAndMakeVisible (themeButton);

    fileButton.setTooltip ("Export or import a pattern file (.kbeat)");
    fileButton.onClick = [this] { showFileMenu(); };
    addAndMakeVisible (fileButton);

    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    updateUndoButtons();

    snapBox.addItem ("1/4",   1);
    snapBox.addItem ("1/8",   2);
    snapBox.addItem ("1/16",  3);
    snapBox.addItem ("1/32",  4);
    snapBox.addItem ("1/8T",  5);
    snapBox.addItem ("1/16T", 6);
    snapBox.setSelectedId (3, juce::dontSendNotification);
    snapBox.onChange = [this] { refreshGrid(); };
    addAndMakeVisible (snapBox);

    barsBox.addItemList ({ "1 bar", "2 bars", "4 bars" }, 1);
    barsBox.setTooltip ("Pattern length  -  the whole grid spans this many bars");
    barsBox.onChange = [this] { refreshGrid(); };
    addAndMakeVisible (barsBox);

    mixSlider.setRange (0.0, 1.0, 0.001);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (mixLabel);

    midiToggle  .setTooltip ("Trigger patterns from notes, starting at C4");
    latchToggle .setTooltip ("Latch: a note switches its pattern on until the same note is played again");
    retrigToggle.setTooltip ("Retrigger: every note restarts its pattern from the beginning  -  "
                             "tape stops and scratches land exactly on the note");
    chainToggle .setTooltip ("Chain: play a sequence of patterns, one per pattern loop");
    chainEditButton.setTooltip ("Edit the chain");
    chainEditButton.onClick = [this] { showChainPanel(); };

    for (auto* b : std::initializer_list<juce::Component*> { &midiToggle, &latchToggle, &retrigToggle,
                                                             &chainToggle, &chainEditButton })
        addAndMakeVisible (b);

    swingSlider.setTooltip ("Swing: pushes every second 1/16 later  -  50 % is straight  (double-click resets)");
    swingSlider.setPopupDisplayEnabled (true, true, this);
    swingSlider.setDoubleClickReturnValue (true, 50.0);
    addAndMakeVisible (swingSlider);
    addAndMakeVisible (swingLabel);

    hintLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (hintLabel);

    barsAttach       = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, "patternBars", barsBox);
    filterTypeAttach = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, "filterType", filterTypeBox);

    timeAttach   = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "timeOn",        timeLane.toggle);
    volAttach    = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "volOn",         volLane.toggle);
    filterAttach = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "filterOn",      filterLane.toggle);
    midiAttach   = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "midiTrigger",   midiToggle);
    latchAttach  = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "midiLatch",     latchToggle);
    retrigAttach = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "midiRetrigger", retrigToggle);
    chainAttach  = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "chainOn",       chainToggle);

    mixAttach          = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "mix",          mixSlider);
    swingAttach        = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "swing",        swingSlider);
    filterResoAttach   = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "filterReso",   filterResoSlider);
    timeSmoothAttach   = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "timeSmooth",   timeLane.smooth);
    volSmoothAttach    = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "volSmooth",    volLane.smooth);
    filterSmoothAttach = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "filterSmooth", filterLane.smooth);

    for (auto* sl : { &timeLane.smooth, &volLane.smooth, &filterLane.smooth })
        sl->setTextValueSuffix (" ms");

    swingSlider.setTextValueSuffix (" %");

    Themes::loadPreference();
    applyThemeColours();
    refreshGrid();

    setWantsKeyboardFocus (true);

    setSize (1040, 720);
    setResizable (true, true);
    setResizeLimits (1000, 600, 1800, 1250);

    startTimerHz (30);
}

KaradagBeatEditor::~KaradagBeatEditor()
{
    setLookAndFeel (nullptr);
}

void KaradagBeatEditor::setupLane (LaneControls& lane, EnvelopeEditor& editor, Envelope& env,
                                   const juce::String& toggleText, const juce::String& smoothTooltip)
{
    editor.onChange    = [this] { processor.publishEnvelopes(); };
    editor.onEditBegin = [this] { pushUndoStep(); };
    addAndMakeVisible (editor);

    lane.toggle.setButtonText (toggleText);
    addAndMakeVisible (lane.toggle);
    addAndMakeVisible (lane.label);

    lane.smooth.setTooltip (smoothTooltip);
    lane.smooth.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (lane.smooth);
    addAndMakeVisible (lane.smoothLabel);

    lane.shiftLeft .setTooltip ("Shift this lane one grid step earlier");
    lane.shiftRight.setTooltip ("Shift this lane one grid step later");
    lane.shiftLeft .onClick = [this, &editor, &env] { shiftLane (editor, env, -1); };
    lane.shiftRight.onClick = [this, &editor, &env] { shiftLane (editor, env, +1); };
    addAndMakeVisible (lane.shiftLeft);
    addAndMakeVisible (lane.shiftRight);

    lane.draw.setTooltip ("Paint steps into grid cells  (or hold Alt while dragging)");
    lane.draw.onClick = [&lane, &editor] { editor.setDrawMode (lane.draw.getToggleState()); };
    addAndMakeVisible (lane.draw);
}

namespace
{
    bool samePoints (const std::vector<EnvPoint>& a, const std::vector<EnvPoint>& b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].x != b[i].x || a[i].y != b[i].y
                || a[i].tension != b[i].tension || a[i].stepped != b[i].stepped)
                return false;

        return true;
    }
}

KaradagBeatEditor::EnvelopeSnapshot KaradagBeatEditor::captureSnapshot() const
{
    return { processor.getEditableTimeEnvelope().getPoints(),
             processor.getEditableVolumeEnvelope().getPoints(),
             processor.getEditableFilterEnvelope().getPoints() };
}

void KaradagBeatEditor::applySnapshot (const EnvelopeSnapshot& snapshot)
{
    processor.getEditableTimeEnvelope()  .setPoints (snapshot.time);
    processor.getEditableVolumeEnvelope().setPoints (snapshot.volume);
    processor.getEditableFilterEnvelope().setPoints (snapshot.filter);
    processor.publishEnvelopes();

    refreshAllEditors();
}

void KaradagBeatEditor::refreshAllEditors()
{
    timeEditor  .envelopeChangedExternally();
    volumeEditor.envelopeChangedExternally();
    filterEditor.envelopeChangedExternally();
}

void KaradagBeatEditor::pushUndoStep()
{
    auto snapshot = captureSnapshot();

    if (! undoStack.empty()
        && samePoints (undoStack.back().time,   snapshot.time)
        && samePoints (undoStack.back().volume, snapshot.volume)
        && samePoints (undoStack.back().filter, snapshot.filter))
        return;

    undoStack.push_back (std::move (snapshot));
    redoStack.clear();

    if ((int) undoStack.size() > kMaxUndoSteps)
        undoStack.erase (undoStack.begin());

    updateUndoButtons();
}

void KaradagBeatEditor::updateUndoButtons()
{
    undoButton.setEnabled (! undoStack.empty());
    redoButton.setEnabled (! redoStack.empty());
}

void KaradagBeatEditor::undo()
{
    if (undoStack.empty())
        return;

    redoStack.push_back (captureSnapshot());
    applySnapshot (undoStack.back());
    undoStack.pop_back();
    updateUndoButtons();
}

void KaradagBeatEditor::redo()
{
    if (redoStack.empty())
        return;

    undoStack.push_back (captureSnapshot());
    applySnapshot (redoStack.back());
    redoStack.pop_back();
    updateUndoButtons();
}

bool KaradagBeatEditor::keyPressed (const juce::KeyPress& key)
{
    const bool ctrl = key.getModifiers().isCommandDown();

    if (! ctrl)
        return false;

    if (key.isKeyCode ('Z'))
    {
        if (key.getModifiers().isShiftDown()) redo();
        else                                  undo();

        return true;
    }

    if (key.isKeyCode ('Y'))
    {
        redo();
        return true;
    }

    return false;
}

void KaradagBeatEditor::refreshGrid()
{
    static const int bars[]      = { 1, 2, 4 };
    static const int perBar[]    = { 4, 8, 16, 32, 12, 24 };

    const int barCount  = bars[juce::jlimit (0, 2, barsBox.getSelectedItemIndex())];
    const int divisions = barCount * perBar[juce::jlimit (0, 5, snapBox.getSelectedId() - 1)];

    for (auto* e : { &timeEditor, &volumeEditor, &filterEditor })
    {
        e->setPatternBars (barCount);
        e->setGridDivisions (divisions);
    }

    timeLane.label.setText (timeLabelText (barCount), juce::dontSendNotification);
}

void KaradagBeatEditor::applyThemeColours()
{
    lookAndFeel.refreshFromTheme();

    struct LaneColour { LaneControls* lane; EnvelopeEditor* editor; juce::Colour accent; };

    const LaneColour lanes[] = { { &timeLane,   &timeEditor,   th().timeAccent   },
                                 { &volLane,    &volumeEditor, th().volumeAccent },
                                 { &filterLane, &filterEditor, th().filterAccent } };

    for (const auto& l : lanes)
    {
        l.editor->setAccentColour (l.accent);
        l.lane->toggle.setColour (juce::TextButton::buttonOnColourId, l.accent);
        l.lane->draw  .setColour (juce::TextButton::buttonOnColourId, l.accent);

        l.lane->smooth.setColour (juce::Slider::trackColourId, l.accent.withAlpha (0.8f));
        l.lane->smooth.setColour (juce::Slider::thumbColourId, l.accent);
        l.lane->smooth.setColour (juce::Slider::backgroundColourId, th().text.withAlpha (0.08f));

        styleLabel (l.lane->smoothLabel, "SMOOTH", 9.5f, th().textDim, juce::Justification::centredRight);
        styleLabel (l.lane->label, l.lane->label.getText(), 10.5f, l.accent.withAlpha (0.75f));
    }

    styleLabel (volLane.label, kVolumeLabel, 10.5f, th().volumeAccent.withAlpha (0.75f));
    styleLabel (filterLane.label, filterLabelText (filterTypeBox.getSelectedItemIndex() == 1), 10.5f,
                th().filterAccent.withAlpha (0.75f));

    filterResoSlider.setColour (juce::Slider::trackColourId, th().filterAccent.withAlpha (0.8f));
    filterResoSlider.setColour (juce::Slider::thumbColourId, th().filterAccent);
    filterResoSlider.setColour (juce::Slider::backgroundColourId, th().text.withAlpha (0.08f));
    styleLabel (filterResoLabel, "RESO", 9.5f, th().textDim, juce::Justification::centredRight);

    swingSlider.setColour (juce::Slider::trackColourId, th().timeAccent.withAlpha (0.8f));
    swingSlider.setColour (juce::Slider::thumbColourId, th().timeAccent);
    swingSlider.setColour (juce::Slider::backgroundColourId, th().text.withAlpha (0.08f));
    styleLabel (swingLabel, "SWING", 9.5f, th().textDim, juce::Justification::centredRight);

    for (auto* t : { &midiToggle, &latchToggle, &retrigToggle, &chainToggle })
        t->setColour (juce::TextButton::buttonOnColourId, th().timeAccent);

    for (auto* b : { &saveButton, &undoButton, &redoButton, &fileButton, &chainEditButton,
                     &timeLane.shiftLeft, &timeLane.shiftRight, &volLane.shiftLeft, &volLane.shiftRight,
                     &filterLane.shiftLeft, &filterLane.shiftRight })
    {
        b->setColour (juce::TextButton::buttonColourId,  th().panel);
        b->setColour (juce::TextButton::textColourOffId, th().text.withAlpha (0.82f));
    }

    styleLabel (mixLabel,  "MIX", 9.5f, th().textDim, juce::Justification::centred);
    styleLabel (hintLabel, kHintText, 10.0f, th().textDim.withAlpha (0.85f));
    hintLabel.setFont (juce::FontOptions (10.0f));
}

void KaradagBeatEditor::shiftLane (EnvelopeEditor& editor, Envelope& env, int direction)
{
    pushUndoStep();
    env.shift ((double) direction / (double) juce::jmax (1, editor.getGridDivisions()));
    processor.publishEnvelopes();
    editor.envelopeChangedExternally();
}

void KaradagBeatEditor::showFileMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSectionHeader ("Pattern file");
    menu.addItem (1, "Export pattern...");
    menu.addItem (2, "Import pattern...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileButton),
        [this] (int result)
        {
            if      (result == 1) exportPatternFile();
            else if (result == 2) importPatternFile();
        });
}

void KaradagBeatEditor::exportPatternFile()
{
    const int slot = presetBox.getSelectedItemIndex();
    const auto suggested = slot >= 0 ? processor.getSlotName (slot) : juce::String ("Pattern");
    const juce::String ext (KaradagBeatProcessor::patternFileExtension);

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export pattern",
        patternFolder().getChildFile (juce::File::createLegalFileName (suggested) + ext),
        "*" + ext);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, ext] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file == juce::File())
                return;

            file = file.withFileExtension (ext);

            if (! processor.exportPattern (file, file.getFileNameWithoutExtension()))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Export failed",
                                                        "Could not write " + file.getFullPathName(),
                                                        "OK", this);
        });
}

void KaradagBeatEditor::importPatternFile()
{
    const juce::String ext (KaradagBeatProcessor::patternFileExtension);

    fileChooser = std::make_unique<juce::FileChooser> ("Import pattern", patternFolder(), "*" + ext);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (! file.existsAsFile())
                return;

            pushUndoStep();

            juce::String name;

            if (processor.importPattern (file, name))
            {
                refreshAllEditors();
                refreshGrid();
            }
            else
            {
                if (! undoStack.empty())
                    undoStack.pop_back();

                updateUndoButtons();

                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Import failed",
                                                        file.getFileName() + " is not a Karadag Beat pattern.",
                                                        "OK", this);
            }
        });
}

void KaradagBeatEditor::showChainPanel()
{
    auto panel = std::make_unique<ChainPanel> (processor);
    juce::CallOutBox::launchAsynchronously (std::move (panel), chainEditButton.getBounds(), this);
}

void KaradagBeatEditor::showThemeMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSectionHeader ("Colour theme");

    const auto& themes = Themes::all();

    for (int i = 0; i < (int) themes.size(); ++i)
        menu.addItem (i + 1, themes[(size_t) i].name, true, i == Themes::getCurrentIndex());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&themeButton),
        [this] (int result)
        {
            if (result <= 0)
                return;

            Themes::setCurrent (result - 1);
            Themes::savePreference();
            applyThemeColours();

            if (auto* top = getTopLevelComponent())
                top->repaint();
        });
}

void KaradagBeatEditor::refreshSlotNames()
{
    for (int i = 0; i < Presets::kNumSlots; ++i)
    {
        const auto name = juce::String (i + 1) + " " + processor.getSlotName (i);

        if (presetBox.getItemText (i) != name)
            presetBox.changeItemText (i + 1, name);
    }
}

int KaradagBeatEditor::chooseTargetSlot() const
{
    const int selected = presetBox.getSelectedItemIndex();

    if (Presets::isUserSlot (selected))
        return selected;

    for (int i = Presets::numPresets(); i < Presets::kNumSlots; ++i)
        if (! processor.isSlotFilled (i))
            return i;

    return Presets::numPresets();
}

void KaradagBeatEditor::promptSaveToSlot()
{
    const int target = chooseTargetSlot();

    auto* window = new juce::AlertWindow ("Save pattern",
                                          "Name for slot " + juce::String (target + 1) + ":",
                                          juce::MessageBoxIconType::NoIcon);

    const auto suggested = processor.isSlotFilled (target)
                             ? processor.getSlotName (target)
                             : juce::String ("Pattern ") + juce::String (target - Presets::numPresets() + 1);

    window->addTextEditor ("name", suggested, {});

    juce::StringArray slotOptions;

    for (int i = Presets::numPresets(); i < Presets::kNumSlots; ++i)
        slotOptions.add (juce::String (i + 1) + "   " + processor.getSlotName (i)
                           + (processor.isSlotFilled (i) ? "   (in use)" : ""));

    window->addComboBox ("slot", slotOptions, "Slot");

    if (auto* box = window->getComboBoxComponent ("slot"))
        box->setSelectedItemIndex (target - Presets::numPresets(), juce::dontSendNotification);

    window->addButton ("Save",    1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel",  0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->addToDesktop (0);
    window->centreAroundComponent (this, window->getWidth(), window->getHeight());

    window->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, window] (int result)
        {
            if (result != 1)
                return;

            const auto name = window->getTextEditorContents ("name").trim();

            int slot = Presets::numPresets();

            if (auto* box = window->getComboBoxComponent ("slot"))
                slot += juce::jmax (0, box->getSelectedItemIndex());

            const bool overwritesOther = processor.isSlotFilled (slot)
                                      && slot != presetBox.getSelectedItemIndex();

            if (! overwritesOther)
            {
                commitSave (slot, name);
                return;
            }

            auto* confirm = new juce::AlertWindow ("Overwrite pattern?",
                                                   "Slot " + juce::String (slot + 1) + " already holds \""
                                                     + processor.getSlotName (slot) + "\".\n"
                                                     "Saving will replace it and cannot be undone.",
                                                   juce::MessageBoxIconType::WarningIcon);

            confirm->addButton ("Overwrite", 1);
            confirm->addButton ("Cancel",    0, juce::KeyPress (juce::KeyPress::escapeKey));

            confirm->addToDesktop (0);
            confirm->centreAroundComponent (this, confirm->getWidth(), confirm->getHeight());

            confirm->enterModalState (true,
                juce::ModalCallbackFunction::create ([this, slot, name] (int choice)
                {
                    if (choice == 1)
                        commitSave (slot, name);
                }),
                true);
        }),
        true);
}

void KaradagBeatEditor::commitSave (int slot, const juce::String& name)
{
    if (processor.saveEditorToSlot (slot, name))
    {
        refreshSlotNames();
        presetBox.setSelectedItemIndex (slot, juce::sendNotificationSync);
    }
}

void KaradagBeatEditor::timerCallback()
{
    const int bins = KaradagBeatProcessor::waveformBins;

    for (int i = 0; i < bins; ++i)
    {
        const double realPhase = processor.patternToRealPhase ((i + 0.5) / bins);
        const int realBin = juce::jlimit (0, bins - 1, (int) (realPhase * bins));
        waveform[(size_t) i] = processor.getWaveformPeak (realBin);
    }

    for (auto* e : { &timeEditor, &volumeEditor, &filterEditor })
    {
        e->setWaveform (waveform);
        e->setPlayheadPhase (processor.getPlayheadPhase());
    }

    const bool highPass = filterTypeBox.getSelectedItemIndex() == 1;
    filterEditor.setFilterHighPass (highPass);

    const auto filterText = filterLabelText (highPass);

    if (filterLane.label.getText() != filterText)
        filterLane.label.setText (filterText, juce::dontSendNotification);

    const int midiPreset = processor.getMidiPreset();

    if (midiPreset != lastSeenMidiPreset)
    {
        lastSeenMidiPreset = midiPreset;

        if (midiPreset >= 0) processor.showSlotInEditor (midiPreset);
        else                 processor.restoreDrawingInEditor();

        refreshAllEditors();

        midiToggle.setButtonText (midiPreset >= 0
                                    ? "MIDI " + juce::String (midiPreset + 1)
                                    : juce::String ("MIDI"));
    }

    const int chainStep = processor.getActiveChainStep();

    if (chainStep != lastSeenChainStep)
    {
        lastSeenChainStep = chainStep;
        chainToggle.setButtonText (chainStep >= 0
                                     ? "CHAIN " + juce::String (chainStep + 1) + "/"
                                         + juce::String (processor.getChainLength())
                                     : juce::String ("CHAIN"));
    }
}

void KaradagBeatEditor::drawSignature (juce::Graphics& g, juce::Rectangle<int> area) const
{
    auto r = area.toFloat();

    const auto box = r.removeFromLeft (28.0f).withSizeKeepingCentre (24.0f, 20.0f);
    Brand::drawGlyph (g, box, 2.0f, th().timeAccent, th().volumeAccent, 4.5f);

    r.removeFromLeft (9.0f);

    auto upper = r.removeFromTop (r.getHeight() * 0.52f);
    g.setFont (juce::Font (juce::FontOptions (9.5f)).withExtraKerningFactor (0.28f));
    g.setColour (th().textDim);
    g.drawText ("KARADAG", upper.withTrimmedTop (9.0f), juce::Justification::bottomLeft);

    g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)).withExtraKerningFactor (0.02f));
    g.setColour (th().text);
    g.drawText ("BEAT", r.withTrimmedBottom (8.0f), juce::Justification::topLeft);
}

void KaradagBeatEditor::paint (juce::Graphics& g)
{
    g.fillAll (th().background);

    g.setColour (th().panel);
    g.fillRect (0, 0, getWidth(), 56);
    g.setColour (th().edge);
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());

    const int barTop = getHeight() - 20 - 36;
    g.setColour (th().panel);
    g.fillRect (0, barTop, getWidth(), 36);
    g.setColour (th().edge);
    g.drawHorizontalLine (barTop, 0.0f, (float) getWidth());

    drawSignature (g, { 12, 0, 152, 56 });
}

void KaradagBeatEditor::layoutLane (LaneControls& lane, EnvelopeEditor& editor,
                                    juce::Rectangle<int> area, bool withFilterExtras)
{
    auto header = area.removeFromTop (20);
    lane.toggle.setBounds (header.removeFromLeft (62).reduced (0, 2));

    lane.draw.setBounds (header.removeFromRight (56).reduced (0, 2));
    header.removeFromRight (6);
    lane.shiftRight.setBounds (header.removeFromRight (22).reduced (0, 2));
    header.removeFromRight (2);
    lane.shiftLeft .setBounds (header.removeFromRight (22).reduced (0, 2));
    header.removeFromRight (12);
    lane.smooth     .setBounds (header.removeFromRight (100));
    lane.smoothLabel.setBounds (header.removeFromRight (50));

    if (withFilterExtras)
    {
        header.removeFromRight (10);
        filterResoSlider.setBounds (header.removeFromRight (80));
        filterResoLabel .setBounds (header.removeFromRight (38));
        header.removeFromRight (8);
        filterTypeBox.setBounds (header.removeFromRight (54).reduced (0, 1));
    }

    header.removeFromLeft (8);
    lane.label.setBounds (header);

    editor.setBounds (area);
}

void KaradagBeatEditor::resized()
{
    auto area = getLocalBounds();

    auto top = area.removeFromTop (56).reduced (10, 6);
    top.removeFromLeft (152);

    top.removeFromLeft (6);
    presetBox.setBounds (top.removeFromLeft (220).withSizeKeepingCentre (220, 26));

    top.removeFromLeft (6);
    saveButton.setBounds (top.removeFromLeft (66).withSizeKeepingCentre (66, 26));

    top.removeFromLeft (4);
    fileButton.setBounds (top.removeFromLeft (46).withSizeKeepingCentre (46, 26));

    top.removeFromLeft (6);
    undoButton.setBounds (top.removeFromLeft (46).withSizeKeepingCentre (46, 26));
    top.removeFromLeft (3);
    redoButton.setBounds (top.removeFromLeft (50).withSizeKeepingCentre (50, 26));

    top.removeFromLeft (8);
    snapBox.setBounds (top.removeFromLeft (74).withSizeKeepingCentre (74, 26));

    top.removeFromLeft (5);
    barsBox.setBounds (top.removeFromLeft (84).withSizeKeepingCentre (84, 26));

    auto mixArea = top.removeFromRight (62);
    mixLabel .setBounds (mixArea.removeFromBottom (10));
    mixSlider.setBounds (mixArea);

    top.removeFromRight (8);
    themeButton.setBounds (top.removeFromRight (40).withSizeKeepingCentre (40, 26));

    hintLabel.setBounds (area.removeFromBottom (20).reduced (12, 0));

    {
        auto bar = area.removeFromBottom (36).reduced (10, 5);

        midiToggle  .setBounds (bar.removeFromLeft (78));
        bar.removeFromLeft (4);
        latchToggle .setBounds (bar.removeFromLeft (62));
        bar.removeFromLeft (4);
        retrigToggle.setBounds (bar.removeFromLeft (70));

        bar.removeFromLeft (20);
        chainToggle    .setBounds (bar.removeFromLeft (96));
        bar.removeFromLeft (3);
        chainEditButton.setBounds (bar.removeFromLeft (30));

        swingSlider.setBounds (bar.removeFromRight (150));
        swingLabel .setBounds (bar.removeFromRight (50));
    }

    area = area.reduced (10, 8);
    const int total = area.getHeight();

    auto timeArea = area.removeFromTop ((int) (total * 0.38f));
    auto volArea  = area.removeFromTop ((int) (total * 0.32f));
    auto filtArea = area;

    layoutLane (timeLane,   timeEditor,   timeArea.withTrimmedBottom (6), false);
    layoutLane (volLane,    volumeEditor, volArea .withTrimmedBottom (6), false);
    layoutLane (filterLane, filterEditor, filtArea,                       true);
}
