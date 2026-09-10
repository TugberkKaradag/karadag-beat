#include "PluginEditor.h"
#include "Theme.h"

namespace
{
    // Tum renkler Theme.h'den geliyor; bu dosyada sabit renk yok.
    inline const Theme& th()  { return Themes::current(); }

    const char* const kHintText =
        "click  add / move      right-click  delete      double-click  step      drag o  bend curve"
        "      alt-drag or DRAW  paint steps      shift  free      notes from C4  trigger";

    /** Pattern dosyalarinin varsayilan klasoru. */
    juce::File patternFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("Karadag Beat Patterns");
        dir.createDirectory();
        return dir;
    }

    /** Time paneli basligi - pattern uzunluguna gore degisir. */
    juce::String timeLabelText (int bars)
    {
        return "TIME   top: live   bottom: " + juce::String (bars)
                 + (bars == 1 ? " bar back" : " bars back");
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
}

//==============================================================================
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
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.strokePath (back, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (th().timeAccent);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Point<float> tip (centre.x + arcR * std::sin (angle),
                            centre.y - arcR * std::cos (angle));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (lineW * 1.1f, lineW * 1.1f).withCentre (tip));
}

void BeatLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawHighlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);

    g.setColour (on ? accent.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.05f));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (on ? accent : juce::Colours::white.withAlpha (shouldDrawHighlighted ? 0.35f : 0.18f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.2f);

    g.setColour (on ? accent.brighter (0.3f) : th().text.withAlpha (0.55f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

//==============================================================================
ThemeButton::ThemeButton()  : juce::Button ("theme")
{
    setTooltip ("Colour theme");
}

void ThemeButton::paintButton (juce::Graphics& g, bool shouldDrawHighlighted, bool)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colours::white.withAlpha (shouldDrawHighlighted ? 0.09f : 0.05f));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (th().edge);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const float r = 4.6f;
    const auto c = bounds.getCentre();

    g.setColour (th().timeAccent);
    g.fillEllipse (juce::Rectangle<float> (r, r).withCentre ({ c.x - 3.6f, c.y }));

    g.setColour (th().volumeAccent);
    g.fillEllipse (juce::Rectangle<float> (r, r).withCentre ({ c.x + 3.6f, c.y }));
}

//==============================================================================
KaradagBeatEditor::KaradagBeatEditor (KaradagBeatProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      timeEditor   (p.getEditableTimeEnvelope(),   false),
      volumeEditor (p.getEditableVolumeEnvelope(), true)
{
    setLookAndFeel (&lookAndFeel);

    // --- zarf editorleri ---
    timeEditor.setAccentColour (th().timeAccent);
    volumeEditor.setAccentColour (th().volumeAccent);
    timeEditor.setShowRuler (true);

    timeEditor.onChange   = [this] { processor.publishEnvelopes(); };
    volumeEditor.onChange = [this] { processor.publishEnvelopes(); };

    timeEditor.onEditBegin   = [this] { pushUndoStep(); };
    volumeEditor.onEditBegin = [this] { pushUndoStep(); };

    addAndMakeVisible (timeEditor);
    addAndMakeVisible (volumeEditor);

    // --- pattern slotu secici ---
    // Baslik ve ayiricilarin ID'si 0; ComboBox indeksleri yalnizca gercek ogeleri
    // saydigi icin parametre baglantisi (indeks = slot) bozulmuyor.
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

    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();

        if (index >= 0)
        {
            pushUndoStep();          // slot yuklenmeden onceki cizim geri alinabilsin
            processor.loadSlotIntoEditor (index);
            timeEditor.envelopeChangedExternally();
            volumeEditor.envelopeChangedExternally();
        }
    };
    addAndMakeVisible (presetBox);

    // --- kullanici slotuna kaydet ---
    saveButton.setColour (juce::TextButton::buttonColourId, th().panel);
    saveButton.setColour (juce::TextButton::textColourOffId, th().text.withAlpha (0.8f));
    saveButton.onClick = [this] { promptSaveToSlot(); };
    addAndMakeVisible (saveButton);

    // --- geri al / ileri al ---
    // Klavye kisayoluna guvenemiyoruz: host (ozellikle FL) Ctrl+Z'yi kendi
    // geri almasi icin yakalayabiliyor.  Gorunur dugmeler her yerde calisir.
    for (auto* b : { &undoButton, &redoButton })
    {
        b->setColour (juce::TextButton::buttonColourId, th().panel);
        b->setColour (juce::TextButton::textColourOffId, th().text.withAlpha (0.85f));
        addAndMakeVisible (b);
    }

    undoButton.setTooltip ("Undo  (Ctrl+Z)");
    redoButton.setTooltip ("Redo  (Ctrl+Shift+Z)");
    saveButton.setTooltip ("Store the drawn envelopes in a user slot");
    presetBox .setTooltip ("Pattern slot  -  1-21 factory, 22-48 your own");
    snapBox   .setTooltip ("Grid the points snap to  (hold Shift to drag freely)");
    midiToggle.setTooltip ("Trigger patterns from notes, starting at C4");
    mixSlider .setTooltip ("Blend between the dry input and the processed signal");
    timeToggle.setTooltip ("Enable the time envelope");
    volToggle .setTooltip ("Enable the volume envelope");
    themeButton.onClick = [this] { showThemeMenu(); };
    addAndMakeVisible (themeButton);

    fileButton.setTooltip ("Export or import a pattern file (.kbeat)");
    fileButton.onClick = [this] { showFileMenu(); };
    addAndMakeVisible (fileButton);

    latchToggle.setTooltip ("Latch: a note switches its pattern on until the same note is played again");
    addAndMakeVisible (latchToggle);

    for (auto* t : { &timeDrawToggle, &volDrawToggle })
    {
        t->setTooltip ("Paint steps into grid cells  (or hold Alt while dragging)");
        addAndMakeVisible (t);
    }

    timeDrawToggle.onClick = [this] { timeEditor  .setDrawMode (timeDrawToggle.getToggleState()); };
    volDrawToggle .onClick = [this] { volumeEditor.setDrawMode (volDrawToggle .getToggleState()); };

    timeShiftLeft .onClick = [this] { shiftLane (timeEditor,   processor.getEditableTimeEnvelope(),   -1); };
    timeShiftRight.onClick = [this] { shiftLane (timeEditor,   processor.getEditableTimeEnvelope(),   +1); };
    volShiftLeft  .onClick = [this] { shiftLane (volumeEditor, processor.getEditableVolumeEnvelope(), -1); };
    volShiftRight .onClick = [this] { shiftLane (volumeEditor, processor.getEditableVolumeEnvelope(), +1); };

    for (auto* b : { &timeShiftLeft, &volShiftLeft })
        b->setTooltip ("Shift this lane one grid step earlier");

    for (auto* b : { &timeShiftRight, &volShiftRight })
        b->setTooltip ("Shift this lane one grid step later");

    for (auto* b : { &timeShiftLeft, &timeShiftRight, &volShiftLeft, &volShiftRight })
        addAndMakeVisible (b);

    // --- yumusatma ---
    timeSmoothSlider.setTooltip ("Crossfade length at time jumps  -  longer gives washier stutters");
    volSmoothSlider .setTooltip ("How fast the volume may change  -  longer turns hard gates into a soft tremolo");

    for (auto* sl : { &timeSmoothSlider, &volSmoothSlider })
    {
        sl->setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (sl);
    }

    for (auto* l : { &timeSmoothLabel, &volSmoothLabel })
        addAndMakeVisible (l);

    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    updateUndoButtons();

    // --- snap secici ---
    snapBox.addItem ("1/4",   1);
    snapBox.addItem ("1/8",   2);
    snapBox.addItem ("1/16",  3);
    snapBox.addItem ("1/32",  4);
    snapBox.addItem ("1/8T",  5);
    snapBox.addItem ("1/16T", 6);
    snapBox.setSelectedId (3, juce::dontSendNotification);
    snapBox.onChange = [this] { refreshGrid(); };
    addAndMakeVisible (snapBox);

    // --- pattern uzunlugu ---
    barsBox.addItemList ({ "1 bar", "2 bars", "4 bars" }, 1);
    barsBox.setTooltip ("Pattern length  -  the whole grid spans this many bars");
    barsBox.onChange = [this] { refreshGrid(); };
    addAndMakeVisible (barsBox);

    // --- dugmeler ---
    timeToggle.setColour (juce::TextButton::buttonOnColourId, th().timeAccent);
    volToggle .setColour (juce::TextButton::buttonOnColourId, th().volumeAccent);
    midiToggle.setColour (juce::TextButton::buttonOnColourId, th().timeAccent);

    for (auto* b : { &timeToggle, &volToggle, &midiToggle })
        addAndMakeVisible (b);

    mixSlider.setRange (0.0, 1.0, 0.001);
    addAndMakeVisible (mixSlider);

    // --- etiketler ---
    styleLabel (timeLabel,  timeLabelText (2), 10.5f, th().timeAccent.withAlpha (0.75f));
    styleLabel (volLabel,   "VOLUME   gate, sidechain, pump", 10.5f, th().volumeAccent.withAlpha (0.75f));
    styleLabel (mixLabel,   "MIX", 9.5f, th().textDim, juce::Justification::centred);
    styleLabel (hintLabel, kHintText, 10.0f, th().textDim.withAlpha (0.85f));
    hintLabel.setFont (juce::FontOptions (10.0f));

    for (auto* l : { &timeLabel, &volLabel, &hintLabel, &mixLabel })
        addAndMakeVisible (l);

    // --- parametre baglantilari ---
    presetAttach = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, "preset", presetBox);
    barsAttach   = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, "patternBars", barsBox);
    timeAttach   = std::make_unique<APVTS::ButtonAttachment>   (processor.apvts, "timeOn", timeToggle);
    volAttach    = std::make_unique<APVTS::ButtonAttachment>   (processor.apvts, "volOn", volToggle);
    midiAttach   = std::make_unique<APVTS::ButtonAttachment>   (processor.apvts, "midiTrigger", midiToggle);
    latchAttach  = std::make_unique<APVTS::ButtonAttachment>   (processor.apvts, "midiLatch", latchToggle);
    mixAttach    = std::make_unique<APVTS::SliderAttachment>   (processor.apvts, "mix", mixSlider);

    timeSmoothAttach = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "timeSmooth", timeSmoothSlider);
    volSmoothAttach  = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "volSmooth",  volSmoothSlider);

    for (auto* sl : { &timeSmoothSlider, &volSmoothSlider })
        sl->setTextValueSuffix (" ms");

    Themes::loadPreference();
    applyThemeColours();
    refreshGrid();

    setWantsKeyboardFocus (true);

    setSize (1040, 600);
    setResizable (true, true);
    setResizeLimits (1000, 480, 1800, 1150);

    startTimerHz (30);
}

KaradagBeatEditor::~KaradagBeatEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
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
             processor.getEditableVolumeEnvelope().getPoints() };
}

void KaradagBeatEditor::applySnapshot (const EnvelopeSnapshot& snapshot)
{
    processor.getEditableTimeEnvelope()  .setPoints (snapshot.time);
    processor.getEditableVolumeEnvelope().setPoints (snapshot.volume);
    processor.publishEnvelopes();

    timeEditor.envelopeChangedExternally();
    volumeEditor.envelopeChangedExternally();
}

void KaradagBeatEditor::pushUndoStep()
{
    auto snapshot = captureSnapshot();

    // Ayni durumu iki kez ust uste yigmayalim - tekerlek gibi hizli
    // tekrarlanan olaylar yigini gereksizce doldurmasin
    if (! undoStack.empty()
        && samePoints (undoStack.back().time, snapshot.time)
        && samePoints (undoStack.back().volume, snapshot.volume))
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
    static const int perBar[]    = { 4, 8, 16, 32, 12, 24 };   // 1/4 1/8 1/16 1/32 1/8T 1/16T

    const int barCount  = bars[juce::jlimit (0, 2, barsBox.getSelectedItemIndex())];
    const int divisions = barCount * perBar[juce::jlimit (0, 5, snapBox.getSelectedId() - 1)];

    for (auto* e : { &timeEditor, &volumeEditor })
    {
        e->setPatternBars (barCount);
        e->setGridDivisions (divisions);
    }

    timeLabel.setText (timeLabelText (barCount), juce::dontSendNotification);
}

void KaradagBeatEditor::applyThemeColours()
{
    lookAndFeel.refreshFromTheme();

    timeEditor  .setAccentColour (th().timeAccent);
    volumeEditor.setAccentColour (th().volumeAccent);

    timeToggle.setColour (juce::TextButton::buttonOnColourId, th().timeAccent);
    volToggle .setColour (juce::TextButton::buttonOnColourId, th().volumeAccent);
    midiToggle.setColour (juce::TextButton::buttonOnColourId, th().timeAccent);
    latchToggle.setColour (juce::TextButton::buttonOnColourId, th().timeAccent);
    timeDrawToggle.setColour (juce::TextButton::buttonOnColourId, th().timeAccent);
    volDrawToggle .setColour (juce::TextButton::buttonOnColourId, th().volumeAccent);

    timeSmoothSlider.setColour (juce::Slider::trackColourId, th().timeAccent.withAlpha (0.8f));
    timeSmoothSlider.setColour (juce::Slider::thumbColourId, th().timeAccent);
    volSmoothSlider .setColour (juce::Slider::trackColourId, th().volumeAccent.withAlpha (0.8f));
    volSmoothSlider .setColour (juce::Slider::thumbColourId, th().volumeAccent);

    for (auto* sl : { &timeSmoothSlider, &volSmoothSlider })
        sl->setColour (juce::Slider::backgroundColourId, juce::Colours::white.withAlpha (0.08f));

    styleLabel (timeSmoothLabel, "SMOOTH", 9.5f, th().textDim, juce::Justification::centredRight);
    styleLabel (volSmoothLabel,  "SMOOTH", 9.5f, th().textDim, juce::Justification::centredRight);

    for (auto* b : { &saveButton, &undoButton, &redoButton, &fileButton,
                     &timeShiftLeft, &timeShiftRight, &volShiftLeft, &volShiftRight })
    {
        b->setColour (juce::TextButton::buttonColourId,  th().panel);
        b->setColour (juce::TextButton::textColourOffId, th().text.withAlpha (0.82f));
    }

    styleLabel (timeLabel, timeLabel.getText(), 10.5f,
                th().timeAccent.withAlpha (0.75f));
    styleLabel (volLabel,  "VOLUME   gate, sidechain, pump", 10.5f,
                th().volumeAccent.withAlpha (0.75f));
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

            // Dosyaya verilen ad pattern'in adi olsun - karsi taraf onu gorecek
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
                timeEditor  .envelopeChangedExternally();
                volumeEditor.envelopeChangedExternally();
                refreshGrid();
            }
            else
            {
                // basarisiz yukleme geri alma yiginina bos bir adim birakmasin
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

//==============================================================================
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

    // Secili slot zaten kullaniciya aitse uzerine yaz
    if (Presets::isUserSlot (selected))
        return selected;

    // Degilse ilk bos kullanici slotunu bul
    for (int i = Presets::numPresets(); i < Presets::kNumSlots; ++i)
        if (! processor.isSlotFilled (i))
            return i;

    // Hepsi doluysa ilk kullanici slotuna dus
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

    // Hedef slot da secilebilsin - boylece bir pattern'i baska bir slota
    // kopyalamak icin yukleyip farkli slota kaydetmek yeterli
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

            // Kendi slotunu yukleyip ayni yere geri kaydetmek bilincli bir hareket;
            // yalnizca BASKA bir dolu slotun uzerine yazarken sor - kaydedilmis bir
            // pattern'in uzerine yazmak geri alinamiyor.
            const bool overwritesOther = processor.isSlotFilled (slot)
                                      && slot != presetBox.getSelectedItemIndex();

            if (! overwritesOther)
            {
                commitSave (slot, name);
                return;
            }

            // Hazir mesaj kutusu ilk dugmeye Enter kisayolunu bagliyor.  Kaydet
            // penceresinde de Enter = Save oldugu icin iki kez Enter'a basmak
            // farkinda olmadan bir pattern'in uzerine yazardi.  Burada yikici
            // dugmenin kisayolu yok; Escape vazgecer.
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

//==============================================================================
void KaradagBeatEditor::timerCallback()
{
    for (int i = 0; i < KaradagBeatProcessor::waveformBins; ++i)
        waveform[(size_t) i] = processor.getWaveformPeak (i);

    timeEditor  .setWaveform (waveform);
    volumeEditor.setWaveform (waveform);

    const double phase = processor.getPlayheadPhase();
    timeEditor  .setPlayheadPhase (phase);
    volumeEditor.setPlayheadPhase (phase);

    // MIDI ile pattern degistiyse gorseli takip ettir (sese tekrar yayin yapmadan)
    const int midiPreset = processor.getMidiPreset();

    if (midiPreset != lastSeenMidiPreset)
    {
        lastSeenMidiPreset = midiPreset;

        const int index = (midiPreset >= 0) ? midiPreset : presetBox.getSelectedItemIndex();

        if (index >= 0)
        {
            processor.loadSlotIntoEditor (index, false);
            timeEditor.envelopeChangedExternally();
            volumeEditor.envelopeChangedExternally();
        }

        // Hangi slotun MIDI ile tetiklendigi gorunsun - piano roll'dan
        // calarken secili slot ile calan slot farkli olabiliyor
        midiToggle.setButtonText (midiPreset >= 0
                                    ? "MIDI " + juce::String (midiPreset + 1)
                                    : juce::String ("MIDI"));
    }
}

//==============================================================================
void KaradagBeatEditor::drawSignature (juce::Graphics& g, juce::Rectangle<int> area) const
{
    auto r = area.toFloat();

    // Glif: once bir basamak, sonra bir egri - eklentinin iki karakteri.
    auto box = r.removeFromLeft (28.0f).withSizeKeepingCentre (24.0f, 20.0f);

    const float x = box.getX(), y = box.getY();
    const float w = box.getWidth(), h = box.getHeight();

    juce::Path mark;
    mark.startNewSubPath (x,             y + h * 0.70f);
    mark.lineTo          (x + w * 0.22f, y + h * 0.70f);
    mark.lineTo          (x + w * 0.22f, y + h * 0.26f);
    mark.lineTo          (x + w * 0.46f, y + h * 0.26f);
    mark.lineTo          (x + w * 0.46f, y + h * 0.90f);
    mark.lineTo          (x + w * 0.62f, y + h * 0.90f);
    mark.quadraticTo     (x + w,         y + h * 0.90f,
                          x + w,         y + h * 0.08f);

    g.setColour (th().timeAccent);
    g.strokePath (mark, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    // Egrinin tepesinde ses zarfinin rengiyle bir nokta - iki lane bir arada
    g.setColour (th().volumeAccent);
    g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f)
                     .withCentre ({ x + w, y + h * 0.08f }));

    // Wordmark
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

    // ust bar
    g.setColour (th().panel);
    g.fillRect (0, 0, getWidth(), 56);
    g.setColour (th().edge);
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());

    drawSignature (g, { 12, 0, 152, 56 });
}

void KaradagBeatEditor::resized()
{
    auto area = getLocalBounds();

    // --- ust bar ---
    auto top = area.removeFromTop (56).reduced (10, 6);
    top.removeFromLeft (152);   // imza alani (paint icinde ciziliyor)

    top.removeFromLeft (6);
    presetBox.setBounds (top.removeFromLeft (200).withSizeKeepingCentre (200, 26));

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

    // sagdan sola: mix, midi
    auto mixArea = top.removeFromRight (62);
    mixLabel .setBounds (mixArea.removeFromBottom (10));
    mixSlider.setBounds (mixArea);

    top.removeFromRight (8);
    midiToggle.setBounds (top.removeFromRight (74).withSizeKeepingCentre (74, 26));

    top.removeFromRight (4);
    latchToggle.setBounds (top.removeFromRight (62).withSizeKeepingCentre (62, 26));

    top.removeFromRight (6);
    themeButton.setBounds (top.removeFromRight (34).withSizeKeepingCentre (34, 26));

    // --- alt ipucu satiri ---
    area.removeFromBottom (2);
    hintLabel.setBounds (area.removeFromBottom (18).reduced (12, 0));

    // --- iki zarf paneli ---
    area = area.reduced (10, 8);
    const int half = area.getHeight() / 2;

    auto timeArea = area.removeFromTop (half);
    {
        auto header = timeArea.removeFromTop (20);
        timeToggle.setBounds (header.removeFromLeft (62).reduced (0, 2));

        timeDrawToggle.setBounds (header.removeFromRight (56).reduced (0, 2));
        header.removeFromRight (6);
        timeShiftRight.setBounds (header.removeFromRight (22).reduced (0, 2));
        header.removeFromRight (2);
        timeShiftLeft .setBounds (header.removeFromRight (22).reduced (0, 2));
        header.removeFromRight (12);
        timeSmoothSlider.setBounds (header.removeFromRight (100));
        timeSmoothLabel .setBounds (header.removeFromRight (50));
        header.removeFromLeft (8);
        timeLabel.setBounds (header);
        timeEditor.setBounds (timeArea.withTrimmedBottom (6));
    }

    auto volArea = area;
    {
        auto header = volArea.removeFromTop (20);
        volToggle.setBounds (header.removeFromLeft (62).reduced (0, 2));

        volDrawToggle.setBounds (header.removeFromRight (56).reduced (0, 2));
        header.removeFromRight (6);
        volShiftRight.setBounds (header.removeFromRight (22).reduced (0, 2));
        header.removeFromRight (2);
        volShiftLeft .setBounds (header.removeFromRight (22).reduced (0, 2));
        header.removeFromRight (12);
        volSmoothSlider.setBounds (header.removeFromRight (100));
        volSmoothLabel .setBounds (header.removeFromRight (50));
        header.removeFromLeft (8);
        volLabel.setBounds (header);
        volumeEditor.setBounds (volArea);
    }
}
