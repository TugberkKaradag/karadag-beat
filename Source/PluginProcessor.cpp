#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Swing.h"

namespace
{
    constexpr int kMaxEnvPoints = 256;

    /** Bir slotun lane'ini zarfa kopyalar; bos lane varsayilan duz degerine doner. */
    void copyLane (const std::vector<EnvPoint>& src, Envelope& dst, double fallback)
    {
        if (src.empty()) dst.clearTo (fallback);
        else             dst.setPoints (src);
    }

    void copyEnvelope (const Envelope& src, Envelope& dst)
    {
        dst.setPoints (src.getPoints());
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout KaradagBeatProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { "preset", 1 }, "Pattern",
                    Presets::slotNames(), 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { "patternBars", 1 }, "Length",
                    juce::StringArray { "1 bar", "2 bars", "4 bars" }, 1));

    layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { "timeOn", 1 }, "Time", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { "volOn", 1 }, "Volume", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { "mix", 1 }, "Mix",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    // Yumusatma sureleri.  Kisa degerlerde ince ayar olsun diye egik aralik.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { "timeSmooth", 1 }, "Time Smooth",
                    juce::NormalisableRange<float> (1.0f, 80.0f, 0.1f, 0.4f), 4.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { "volSmooth", 1 }, "Volume Smooth",
                    juce::NormalisableRange<float> (0.5f, 80.0f, 0.1f, 0.4f), 2.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { "midiTrigger", 1 }, "MIDI Trigger", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { "midiLatch", 1 }, "MIDI Latch", false));

    // --- 0.2: filtre lane'i, retrigger, swing, zincir ---
    layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { "filterOn", 1 }, "Filter", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { "filterType", 1 }, "Filter Type",
                    juce::StringArray { "Low-pass", "High-pass" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { "filterReso", 1 }, "Filter Resonance",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { "filterSmooth", 1 }, "Filter Smooth",
                    juce::NormalisableRange<float> (0.5f, 80.0f, 0.1f, 0.4f), 5.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { "midiRetrigger", 1 }, "MIDI Retrigger", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { "swing", 1 }, "Swing",
                    juce::NormalisableRange<float> (50.0f, 75.0f, 0.1f), 50.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { "chainOn", 1 }, "Chain", false));

    return layout;
}

//==============================================================================
KaradagBeatProcessor::KaradagBeatProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "state", createParameterLayout())
{
    pPreset       = apvts.getRawParameterValue ("preset");
    pPatternBars  = apvts.getRawParameterValue ("patternBars");
    pTimeOn       = apvts.getRawParameterValue ("timeOn");
    pVolOn        = apvts.getRawParameterValue ("volOn");
    pFilterOn     = apvts.getRawParameterValue ("filterOn");
    pMix          = apvts.getRawParameterValue ("mix");
    pMidiTrigger  = apvts.getRawParameterValue ("midiTrigger");
    pMidiLatch    = apvts.getRawParameterValue ("midiLatch");
    pMidiRetrig   = apvts.getRawParameterValue ("midiRetrigger");
    pTimeSmooth   = apvts.getRawParameterValue ("timeSmooth");
    pVolSmooth    = apvts.getRawParameterValue ("volSmooth");
    pFilterSmooth = apvts.getRawParameterValue ("filterSmooth");
    pFilterType   = apvts.getRawParameterValue ("filterType");
    pFilterReso   = apvts.getRawParameterValue ("filterReso");
    pSwing        = apvts.getRawParameterValue ("swing");
    pChainOn      = apvts.getRawParameterValue ("chainOn");

    for (auto& s : chainSteps)
        s.store (kChainDrawing);

    // ses thread'inde nota eklerken ya da zarf kopyalarken bellek ayirmasin
    heldNotes.ensureStorageAllocated (128);
    swingScratch.reserve ((size_t) kMaxEnvPoints);

    for (auto* e : { &editTime, &editVolume, &editFilter,
                     &stashTime, &stashVolume, &stashFilter,
                     &pendingTime, &pendingVolume, &pendingFilter,
                     &baseTime, &baseVolume, &baseFilter,
                     &audioTime, &audioVolume, &audioFilter,
                     &playTime, &playVolume, &playFilter })
        e->reserve (kMaxEnvPoints);

    loadUserSlotsFromDisk();
}

bool KaradagBeatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void KaradagBeatProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumInputChannels()));

    heldNotes.clear();
    latchedNote = -1;
    retriggered = false;
    midiActive.store (false);

    // Taban (cizim) korunur: host yeniden hazirlasa da (ornegin buffer boyu
    // degisince) calinan pattern cizime doner, slotun kayitli haline degil.
    activeOverride = -1;
    baseChanged    = true;
    audioChanged   = true;
}

//==============================================================================
void KaradagBeatProcessor::publishEnvelopes()
{
    {
        const juce::SpinLock::ScopedLockType lock (publishLock);
        pendingTime  .setPoints (editTime  .getPoints());
        pendingVolume.setPoints (editVolume.getPoints());
        pendingFilter.setPoints (editFilter.getPoints());
        pendingIsTweak = editorShowsSlot.load();
    }

    hasPending.store (true);
}

void KaradagBeatProcessor::copySlot (int index, Envelope& time, Envelope& volume, Envelope& filter) const
{
    if (! juce::isPositiveAndBelow (index, (int) slots.size()))
        return;

    const auto& preset = slots[(size_t) index];
    copyLane (preset.time,   time,   0.0);
    copyLane (preset.volume, volume, 1.0);
    copyLane (preset.filter, filter, 1.0);
}

void KaradagBeatProcessor::loadSlotIntoEditor (int index)
{
    if (! juce::isPositiveAndBelow (index, Presets::kNumSlots))
        return;

    // MIDI ile bir slot gosterilirken secilen preset, gosterim bitince geri
    // gelecek cizimin yerine gecer.  Sesteki tabani parametre degisimi gunceller.
    if (editorShowsSlot.load())
    {
        const juce::SpinLock::ScopedLockType lock (slotLock);
        copySlot (index, stashTime, stashVolume, stashFilter);
        return;
    }

    {
        const juce::SpinLock::ScopedLockType lock (slotLock);
        copySlot (index, editTime, editVolume, editFilter);
    }

    publishEnvelopes();
}

void KaradagBeatProcessor::showSlotInEditor (int index)
{
    if (! juce::isPositiveAndBelow (index, Presets::kNumSlots))
        return;

    if (! editorShowsSlot.load())
    {
        copyEnvelope (editTime,   stashTime);
        copyEnvelope (editVolume, stashVolume);
        copyEnvelope (editFilter, stashFilter);
        editorShowsSlot.store (true);
    }

    const juce::SpinLock::ScopedLockType lock (slotLock);
    copySlot (index, editTime, editVolume, editFilter);
}

void KaradagBeatProcessor::restoreDrawingInEditor()
{
    if (! editorShowsSlot.load())
        return;

    copyEnvelope (stashTime,   editTime);
    copyEnvelope (stashVolume, editVolume);
    copyEnvelope (stashFilter, editFilter);
    editorShowsSlot.store (false);

    publishEnvelopes();
}

bool KaradagBeatProcessor::saveEditorToSlot (int index, const juce::String& name)
{
    if (! Presets::isUserSlot (index))
        return false;

    {
        const juce::SpinLock::ScopedLockType lock (slotLock);
        auto& slot = slots[(size_t) index];

        slot.name   = name.isNotEmpty() ? name : Presets::defaultDisplayName (index);
        slot.time   = editTime  .getPoints();
        slot.volume = editVolume.getPoints();
        slot.filter = editFilter.getPoints();
    }

    saveUserSlotsToDisk();
    return true;
}

int KaradagBeatProcessor::getPatternBars() const noexcept
{
    static constexpr int bars[] = { 1, 2, 4 };
    return bars[juce::jlimit (0, 2, (int) pPatternBars->load())];
}

double KaradagBeatProcessor::patternToRealPhase (double patternPhase) const noexcept
{
    return Swing::patternToReal (patternPhase, swingCellsForGui.load(), swingAmountForGui.load());
}

//==============================================================================
int KaradagBeatProcessor::getChainStep (int step) const noexcept
{
    if (! juce::isPositiveAndBelow (step, kMaxChainSteps))
        return kChainDrawing;

    return chainSteps[(size_t) step].load();
}

void KaradagBeatProcessor::setChainStep (int step, int slotOrDrawing) noexcept
{
    if (! juce::isPositiveAndBelow (step, kMaxChainSteps))
        return;

    const int value = juce::isPositiveAndBelow (slotOrDrawing, Presets::kNumSlots) ? slotOrDrawing
                                                                                  : kChainDrawing;
    chainSteps[(size_t) step].store (value);
}

void KaradagBeatProcessor::setChainLength (int steps) noexcept
{
    chainLength.store (juce::jlimit (1, kMaxChainSteps, steps));
}

//==============================================================================
bool KaradagBeatProcessor::exportPattern (const juce::File& file, const juce::String& name) const
{
    juce::XmlElement xml ("KaradagBeatPattern");
    xml.setAttribute ("version", 2);
    xml.setAttribute ("name",    name);
    xml.setAttribute ("bars",    getPatternBars());
    xml.setAttribute ("time",    editTime  .toString());
    xml.setAttribute ("volume",  editVolume.toString());
    xml.setAttribute ("filter",  editFilter.toString());

    return xml.writeTo (file);
}

bool KaradagBeatProcessor::importPattern (const juce::File& file, juce::String& loadedName)
{
    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName ("KaradagBeatPattern"))
        return false;

    // Once gecici zarflara oku - dosya bozuksa mevcut cizim bozulmasin
    Envelope timeEnv (0.0), volEnv (1.0), filterEnv (1.0);

    if (! timeEnv.fromString (xml->getStringAttribute ("time"))
        || ! volEnv.fromString (xml->getStringAttribute ("volume")))
        return false;

    // Filtre 0.2'de geldi; eski dosyalarda yok, o zaman filtre acik kalir
    const auto filterText = xml->getStringAttribute ("filter");

    if (filterText.isNotEmpty() && ! filterEnv.fromString (filterText))
        return false;

    editTime  .setPoints (timeEnv  .getPoints());
    editVolume.setPoints (volEnv   .getPoints());
    editFilter.setPoints (filterEnv.getPoints());
    publishEnvelopes();

    const int bars = xml->getIntAttribute ("bars", 0);
    const int choice = bars == 1 ? 0 : bars == 2 ? 1 : bars == 4 ? 2 : -1;

    if (choice >= 0)
        if (auto* param = apvts.getParameter ("patternBars"))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) choice));

    loadedName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    return true;
}

juce::String KaradagBeatProcessor::getSlotName (int index) const
{
    if (! juce::isPositiveAndBelow (index, Presets::kNumSlots))
        return {};

    const juce::SpinLock::ScopedLockType lock (slotLock);
    return slots[(size_t) index].name;
}

bool KaradagBeatProcessor::isSlotFilled (int index) const
{
    if (! juce::isPositiveAndBelow (index, Presets::kNumSlots))
        return false;

    const juce::SpinLock::ScopedLockType lock (slotLock);
    return ! slots[(size_t) index].isEmpty();
}

//==============================================================================
juce::File KaradagBeatProcessor::getUserPatternFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Karadag")
             .getChildFile ("KaradagBeat")
             .getChildFile ("user_patterns.xml");
}

void KaradagBeatProcessor::loadUserSlotsFromDisk()
{
    const auto file = getUserPatternFile();

    if (! file.existsAsFile())
        return;

    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName ("KaradagBeatUserPatterns"))
        return;

    const juce::SpinLock::ScopedLockType lock (slotLock);

    for (auto* slotXml : xml->getChildWithTagNameIterator ("Slot"))
    {
        // "user" = kacinci kullanici slotu (fabrika sayisindan bagimsiz).
        // Fabrika pattern'i eklendiginde kullanici kayitlari kaymasin diye
        // mutlak index yerine bunu kullaniyoruz; "index" eski dosyalar icin.
        int index = -1;

        if (slotXml->hasAttribute ("user"))
            index = Presets::numPresets() + slotXml->getIntAttribute ("user", -1);
        else
            index = slotXml->getIntAttribute ("index", -1);

        if (! Presets::isUserSlot (index))
            continue;

        auto& slot = slots[(size_t) index];
        slot.name = slotXml->getStringAttribute ("name", Presets::defaultDisplayName (index));

        auto readLane = [slotXml] (const char* attribute, double fallback, std::vector<EnvPoint>& dst)
        {
            dst.clear();
            const auto text = slotXml->getStringAttribute (attribute);
            Envelope e (fallback);

            if (text.isNotEmpty() && e.fromString (text))
                dst = e.getPoints();
        };

        readLane ("time",   0.0, slot.time);
        readLane ("volume", 1.0, slot.volume);
        readLane ("filter", 1.0, slot.filter);
    }
}

void KaradagBeatProcessor::saveUserSlotsToDisk() const
{
    juce::XmlElement xml ("KaradagBeatUserPatterns");

    {
        const juce::SpinLock::ScopedLockType lock (slotLock);

        for (int i = 0; i < Presets::kNumSlots; ++i)
        {
            if (! Presets::isUserSlot (i))
                continue;

            const auto& slot = slots[(size_t) i];

            if (slot.isEmpty())
                continue;

            auto* slotXml = xml.createNewChildElement ("Slot");
            slotXml->setAttribute ("user", i - Presets::numPresets());
            slotXml->setAttribute ("name", slot.name);

            auto writeLane = [slotXml] (const char* attribute, double fallback,
                                        const std::vector<EnvPoint>& src)
            {
                if (src.empty())
                    return;

                Envelope e (fallback);
                e.setPoints (src);
                slotXml->setAttribute (attribute, e.toString());
            };

            writeLane ("time",   0.0, slot.time);
            writeLane ("volume", 1.0, slot.volume);
            writeLane ("filter", 1.0, slot.filter);
        }
    }

    const auto file = getUserPatternFile();
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}

//==============================================================================
bool KaradagBeatProcessor::beginMidiBlock()
{
    const bool enabled = pMidiTrigger->load() > 0.5f;
    const bool latch   = pMidiLatch  ->load() > 0.5f;

    // Tetikleme kapandiysa ya da mod degistiyse temiz bir baslangic yap
    if (! enabled || latch != wasLatching)
    {
        heldNotes.clear();
        latchedNote = -1;
        midiActive.store (false);
        wasLatching = latch;
    }

    return enabled;
}

bool KaradagBeatProcessor::handleMidiMessage (const juce::MidiMessage& m)
{
    if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNotes.clear();
        latchedNote = -1;
        midiActive.store (false);
        return false;
    }

    if (wasLatching)
    {
        // Latch: nota bir pattern'i acar, ayni nota tekrar basilinca kapatir.
        // Notayi birakmak hicbir sey yapmaz - uzun freeze'ler icin piano
        // roll'a uzun nota cizmek gerekmiyor.
        if (! m.isNoteOn())
            return false;

        const int note  = m.getNoteNumber();
        const int index = note - midiBaseNote;

        if (note == latchedNote)
        {
            latchedNote = -1;
            midiActive.store (false);
            return false;
        }

        if (juce::isPositiveAndBelow (index, Presets::kNumSlots))
        {
            latchedNote = note;
            midiPreset.store (index);
            midiActive.store (true);
            return true;
        }

        return false;
    }

    if      (m.isNoteOn())  heldNotes.add (m.getNoteNumber());
    else if (m.isNoteOff()) heldNotes.removeValue (m.getNoteNumber());
    else                    return false;

    // Basili notalardan slot araligina dusen en tizi kazanir.  Aralik
    // disindaki bir nota, altindaki gecerli notayi gizlememeli.
    int winner = -1;
    int winnerNote = -1;

    for (int i = heldNotes.size() - 1; i >= 0 && winner < 0; --i)
    {
        const int index = heldNotes[i] - midiBaseNote;

        if (juce::isPositiveAndBelow (index, Presets::kNumSlots))
        {
            winner     = index;
            winnerNote = heldNotes[i];
        }
    }

    if (winner >= 0)
        midiPreset.store (winner);

    midiActive.store (winner >= 0);

    // Yeni bir pattern ancak kazanan notanin kendisi basildiginda baslar;
    // alttaki bir notaya basmak calani yeniden baslatmamali.
    return m.isNoteOn() && winner >= 0 && m.getNoteNumber() == winnerNote;
}

int KaradagBeatProcessor::resolveOverride() noexcept
{
    if (midiActive.load())
    {
        activeChainStep.store (-1);
        return midiPreset.load();
    }

    if (pChainOn->load() > 0.5f)
    {
        const int length = juce::jlimit (1, kMaxChainSteps, chainLength.load());
        const int step   = (int) (((chainCycle % length) + length) % length);

        activeChainStep.store (step);
        return chainSteps[(size_t) step].load();
    }

    activeChainStep.store (-1);
    return -1;
}

void KaradagBeatProcessor::updatePatternSources()
{
    // 1) Preset parametresi degisti: yeni taban.  Kullanicinin elle cizdigi zarf
    //    geldikten sonra bunu tekrar uygularsak cizimi her blokta sileriz, bu
    //    yuzden yalnizca SECIM degistiginde yuklenir.
    const int param = juce::jlimit (0, Presets::kNumSlots - 1, (int) pPreset->load());

    if (param != lastParamPreset)
    {
        const juce::SpinLock::ScopedTryLockType lock (slotLock);

        if (lock.isLocked())
        {
            copySlot (param, baseTime, baseVolume, baseFilter);
            lastParamPreset = param;
            baseChanged = true;
        }
        // kilit o an GUI'deyse yukleme bir sonraki parcaya kalir
    }

    // 2) GUI'den elle duzenlenmis zarf geldi mi?
    if (hasPending.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (publishLock);

        if (lock.isLocked())
        {
            if (! pendingIsTweak)
            {
                copyEnvelope (pendingTime,   baseTime);
                copyEnvelope (pendingVolume, baseVolume);
                copyEnvelope (pendingFilter, baseFilter);
                baseChanged = true;
            }
            else if (activeOverride >= 0)
            {
                // Editor calan slotu gosteriyor: degisiklik yalnizca ona uygulanir
                copyEnvelope (pendingTime,   audioTime);
                copyEnvelope (pendingVolume, audioVolume);
                copyEnvelope (pendingFilter, audioFilter);
                audioChanged = true;
            }

            hasPending.store (false);
        }
    }

    // 3) Gecici kaynak (MIDI / zincir) degisti mi?
    const int wanted = resolveOverride();

    if (wanted != activeOverride)
    {
        if (wanted >= 0)
        {
            const juce::SpinLock::ScopedTryLockType lock (slotLock);

            if (lock.isLocked())
            {
                copySlot (wanted, audioTime, audioVolume, audioFilter);
                activeOverride = wanted;
                audioChanged = true;
            }
        }
        else
        {
            copyEnvelope (baseTime,   audioTime);
            copyEnvelope (baseVolume, audioVolume);
            copyEnvelope (baseFilter, audioFilter);
            activeOverride = -1;
            baseChanged  = false;
            audioChanged = true;
        }
    }
    else if (wanted < 0 && baseChanged)
    {
        copyEnvelope (baseTime,   audioTime);
        copyEnvelope (baseVolume, audioVolume);
        copyEnvelope (baseFilter, audioFilter);
        baseChanged  = false;
        audioChanged = true;
    }
}

void KaradagBeatProcessor::refreshPlayEnvelopes()
{
    if (! audioChanged)
        return;

    audioChanged = false;
    swingOn = Swing::isActive (appliedCells, appliedSwing);

    if (! swingOn)
        return;

    Swing::apply (audioTime.getPoints(), swingScratch, appliedCells, appliedSwing, true);
    playTime.setPoints (swingScratch);

    Swing::apply (audioVolume.getPoints(), swingScratch, appliedCells, appliedSwing, false);
    playVolume.setPoints (swingScratch);

    Swing::apply (audioFilter.getPoints(), swingScratch, appliedCells, appliedSwing, false);
    playFilter.setPoints (swingScratch);
}

//==============================================================================
void KaradagBeatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const bool midiEnabled  = beginMidiBlock();
    const bool retrigEnabled = pMidiRetrig->load() > 0.5f;

    if (! retrigEnabled || ! midiActive.load())
        retriggered = false;

    // --- host transport'undan tempo ve konum ---
    double bpm = 120.0, ppq = 0.0;
    int tsNumerator = 4, tsDenominator = 4;
    bool playing = false, havePpq = false;

    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (const auto b = pos->getBpm())
                bpm = *b;

            if (const auto ts = pos->getTimeSignature())
            {
                tsNumerator   = ts->numerator;
                tsDenominator = ts->denominator;
            }

            playing = pos->getIsPlaying();

            if (const auto p = pos->getPpqPosition())
            {
                ppq     = *p;
                havePpq = true;
            }
        }
    }

    bpm = juce::jlimit (20.0, 999.0, bpm);

    // Pattern uzunlugu 1 / 2 / 4 bar, ceyrek nota cinsinden
    static constexpr double barChoices[] = { 1.0, 2.0, 4.0 };
    const int barIndex = juce::jlimit (0, 2, (int) pPatternBars->load());

    const double beatsPerBar    = (double) juce::jmax (1, tsNumerator) * 4.0
                                / (double) juce::jmax (1, tsDenominator);
    const double patternBeats   = barChoices[barIndex] * beatsPerBar;
    const double samplesPerBeat = (60.0 / bpm) * sampleRateHz;

    engine.setPatternLengthSamples (patternBeats * samplesPerBeat);
    engine.setSmoothing (pTimeSmooth->load(), pVolSmooth->load());
    engine.setFilter (pFilterType->load() > 0.5f, pFilterReso->load(), pFilterSmooth->load());

    // Host'un pattern icindeki konumu.  Retrigger ile bastan baslatilmis bir
    // pattern, nota birakilana kadar kendi fazinda akar.
    const bool   hostSync  = playing && havePpq;
    const double hostTurns = hostSync ? ppq / patternBeats : 0.0;

    if (hostSync)
    {
        const double whole = std::floor (hostTurns);
        chainCycle = (juce::int64) whole;

        if (! retriggered)
            engine.setPhase (hostTurns - whole);
    }

    // --- swing: pattern'deki 1/16'lik sayisi ve miktar ---
    const int    swingCells  = juce::jmax (0, (int) std::floor (patternBeats * 4.0 + 1.0e-6));
    const double swingAmount = pSwing->load() / 100.0;

    swingCellsForGui .store (swingCells);
    swingAmountForGui.store (swingAmount);

    if (swingAmount != appliedSwing || swingCells != appliedCells)
    {
        appliedSwing = swingAmount;
        appliedCells = swingCells;
        audioChanged = true;
    }

    updatePatternSources();
    refreshPlayEnvelopes();

    // --- blogu MIDI olaylarinin ve zincir adimlarinin dustugu noktalarda bol ---
    // Pattern degisimi tam o sample'da gerceklessin.  Aksi halde vurusa konan
    // bir nota pattern'i bir buffer boyu erken degistirir.
    const bool  timeOn   = pTimeOn  ->load() > 0.5f;
    const bool  volOn    = pVolOn   ->load() > 0.5f;
    const bool  filterOn = pFilterOn->load() > 0.5f;
    const bool  chainOn  = pChainOn ->load() > 0.5f;
    const float mix      = pMix     ->load();

    const int numSamples = buffer.getNumSamples();
    int cursor = 0;

    auto renderUntil = [&] (int end)
    {
        end = juce::jlimit (cursor, numSamples, end);

        while (cursor < end)
        {
            int chunkEnd = end;

            // Zincir: pattern turunun bittigi sample'da da bol ki yeni adim tam
            // turun basinda baslasin
            if (chainOn)
            {
                const double remaining = (1.0 - engine.getPhase()) * engine.getPatternLengthSamples();
                const int toWrap = juce::jmax (1, (int) std::ceil (remaining - 1.0e-7));
                chunkEnd = juce::jmin (end, cursor + toWrap);
            }

            const double phaseBefore = engine.getPhase();

            juce::AudioBuffer<float> part (buffer.getArrayOfWritePointers(),
                                           buffer.getNumChannels(), cursor, chunkEnd - cursor);

            engine.processBlock (part,
                                 swingOn ? playTime   : audioTime,
                                 swingOn ? playVolume : audioVolume,
                                 swingOn ? playFilter : audioFilter,
                                 timeOn, volOn, filterOn, mix);

            cursor = chunkEnd;

            if (chainOn && engine.getPhase() < phaseBefore)
            {
                ++chainCycle;
                updatePatternSources();
                refreshPlayEnvelopes();
            }
        }
    };

    if (midiEnabled)
    {
        for (const auto meta : midi)
        {
            renderUntil (meta.samplePosition);

            const bool started = handleMidiMessage (meta.getMessage());

            if (started && retrigEnabled)
            {
                engine.setPhase (0.0);
                retriggered = true;
            }
            else if (retriggered && ! midiActive.load())
            {
                // Nota birakildi: pattern host'un o anki konumuna geri doner
                retriggered = false;

                if (hostSync)
                {
                    const double turns = hostTurns + (double) cursor / engine.getPatternLengthSamples();
                    engine.setPhase (turns - std::floor (turns));
                }
            }

            updatePatternSources();
            refreshPlayEnvelopes();
        }
    }

    renderUntil (numSamples);

    displayPhase.store (Swing::realToPattern (engine.getPhase(), appliedCells, appliedSwing));
}

//==============================================================================
juce::AudioProcessorEditor* KaradagBeatProcessor::createEditor()
{
    return new KaradagBeatEditor (*this);
}

void KaradagBeatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Editor o an MIDI ile calan bir slotu gosteriyorsa kaydedilecek olan
    // kenarda bekleyen cizimdir
    const bool showing = editorShowsSlot.load();

    auto state = apvts.copyState();
    state.setProperty ("timeEnv",   (showing ? stashTime   : editTime)  .toString(), nullptr);
    state.setProperty ("volumeEnv", (showing ? stashVolume : editVolume).toString(), nullptr);
    state.setProperty ("filterEnv", (showing ? stashFilter : editFilter).toString(), nullptr);

    juce::StringArray chain;

    for (int i = 0; i < kMaxChainSteps; ++i)
        chain.add (juce::String (getChainStep (i)));

    state.setProperty ("chain",       chain.joinIntoString (","), nullptr);
    state.setProperty ("chainLength", getChainLength(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void KaradagBeatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid())
        return;

    apvts.replaceState (state);

    const auto timeText   = state.getProperty ("timeEnv").toString();
    const auto volText    = state.getProperty ("volumeEnv").toString();
    const auto filterText = state.getProperty ("filterEnv").toString();

    editorShowsSlot.store (false);

    if (timeText.isNotEmpty()) editTime  .fromString (timeText);
    if (volText .isNotEmpty()) editVolume.fromString (volText);

    // 0.1 projelerinde filtre yok - acik baslasin
    if (filterText.isEmpty() || ! editFilter.fromString (filterText))
        editFilter.clearTo (1.0);

    const auto chain = juce::StringArray::fromTokens (state.getProperty ("chain").toString(), ",", {});

    for (int i = 0; i < kMaxChainSteps; ++i)
        setChainStep (i, i < chain.size() ? chain[i].getIntValue() : kChainDrawing);

    setChainLength ((int) state.getProperty ("chainLength", 4));

    publishEnvelopes();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KaradagBeatProcessor();
}
