#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kMaxEnvPoints = 256;
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

    return layout;
}

//==============================================================================
KaradagBeatProcessor::KaradagBeatProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "state", createParameterLayout())
{
    pPreset      = apvts.getRawParameterValue ("preset");
    pPatternBars = apvts.getRawParameterValue ("patternBars");
    pTimeOn      = apvts.getRawParameterValue ("timeOn");
    pVolOn       = apvts.getRawParameterValue ("volOn");
    pMix         = apvts.getRawParameterValue ("mix");
    pMidiTrigger = apvts.getRawParameterValue ("midiTrigger");
    pMidiLatch   = apvts.getRawParameterValue ("midiLatch");
    pTimeSmooth  = apvts.getRawParameterValue ("timeSmooth");
    pVolSmooth   = apvts.getRawParameterValue ("volSmooth");

    // ses thread'inde nota eklerken bellek ayirmasin
    heldNotes.ensureStorageAllocated (128);

    for (auto* e : { &editTime, &editVolume, &pendingTime, &pendingVolume, &audioTime, &audioVolume })
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

    lastRequestedPreset = -1;   // ilk blokta preset yeniden uygulansin
    heldNotes.clear();
    latchedNote = -1;
    midiActive.store (false);
}

//==============================================================================
void KaradagBeatProcessor::publishEnvelopes()
{
    {
        const juce::SpinLock::ScopedLockType lock (publishLock);
        pendingTime  .setPoints (editTime  .getPoints());
        pendingVolume.setPoints (editVolume.getPoints());
    }

    hasPending.store (true);
}

void KaradagBeatProcessor::loadSlotIntoEditor (int index, bool alsoPublish)
{
    if (! juce::isPositiveAndBelow (index, Presets::kNumSlots))
        return;

    {
        const juce::SpinLock::ScopedLockType lock (slotLock);
        const auto& preset = slots[(size_t) index];

        if (preset.time.empty())  editTime.clearTo (0.0);
        else                      editTime.setPoints (preset.time);

        if (preset.volume.empty()) editVolume.clearTo (1.0);
        else                       editVolume.setPoints (preset.volume);
    }

    if (alsoPublish)
        publishEnvelopes();
}

void KaradagBeatProcessor::applySlotToAudioEnvelopes (int index)
{
    // Cagiran taraf slotLock'u tutuyor olmali.
    if (! juce::isPositiveAndBelow (index, (int) slots.size()))
        return;

    const auto& preset = slots[(size_t) index];

    if (preset.time.empty())  audioTime.clearTo (0.0);
    else                      audioTime.setPoints (preset.time);

    if (preset.volume.empty()) audioVolume.clearTo (1.0);
    else                       audioVolume.setPoints (preset.volume);
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
    }

    saveUserSlotsToDisk();
    return true;
}

int KaradagBeatProcessor::getPatternBars() const noexcept
{
    static constexpr int bars[] = { 1, 2, 4 };
    return bars[juce::jlimit (0, 2, (int) pPatternBars->load())];
}

bool KaradagBeatProcessor::exportPattern (const juce::File& file, const juce::String& name) const
{
    juce::XmlElement xml ("KaradagBeatPattern");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("name",    name);
    xml.setAttribute ("bars",    getPatternBars());
    xml.setAttribute ("time",    editTime  .toString());
    xml.setAttribute ("volume",  editVolume.toString());

    return xml.writeTo (file);
}

bool KaradagBeatProcessor::importPattern (const juce::File& file, juce::String& loadedName)
{
    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName ("KaradagBeatPattern"))
        return false;

    // Once gecici zarflara oku - dosya bozuksa mevcut cizim bozulmasin
    Envelope timeEnv (0.0), volEnv (1.0);

    if (! timeEnv.fromString (xml->getStringAttribute ("time"))
        || ! volEnv.fromString (xml->getStringAttribute ("volume")))
        return false;

    editTime  .setPoints (timeEnv.getPoints());
    editVolume.setPoints (volEnv .getPoints());
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
    const auto& slot = slots[(size_t) index];
    return ! (slot.time.empty() && slot.volume.empty());
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

        Envelope timeEnv (0.0), volEnv (1.0);

        const auto timeText = slotXml->getStringAttribute ("time");
        const auto volText  = slotXml->getStringAttribute ("volume");

        auto& slot = slots[(size_t) index];
        slot.name = slotXml->getStringAttribute ("name", Presets::defaultDisplayName (index));

        slot.time.clear();
        slot.volume.clear();

        if (timeText.isNotEmpty() && timeEnv.fromString (timeText))
            slot.time = timeEnv.getPoints();

        if (volText.isNotEmpty() && volEnv.fromString (volText))
            slot.volume = volEnv.getPoints();
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

            if (slot.time.empty() && slot.volume.empty())
                continue;

            auto* slotXml = xml.createNewChildElement ("Slot");
            slotXml->setAttribute ("user", i - Presets::numPresets());
            slotXml->setAttribute ("name", slot.name);

            if (! slot.time.empty())
            {
                Envelope e (0.0);
                e.setPoints (slot.time);
                slotXml->setAttribute ("time", e.toString());
            }

            if (! slot.volume.empty())
            {
                Envelope e (1.0);
                e.setPoints (slot.volume);
                slotXml->setAttribute ("volume", e.toString());
            }
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

void KaradagBeatProcessor::handleMidiMessage (const juce::MidiMessage& m)
{
    if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNotes.clear();
        latchedNote = -1;
        midiActive.store (false);
        return;
    }

    if (wasLatching)
    {
        // Latch: nota bir pattern'i acar, ayni nota tekrar basilinca kapatir.
        // Notayi birakmak hicbir sey yapmaz - uzun freeze'ler icin piano
        // roll'a uzun nota cizmek gerekmiyor.
        if (! m.isNoteOn())
            return;

        const int note  = m.getNoteNumber();
        const int index = note - midiBaseNote;

        if (note == latchedNote)
        {
            latchedNote = -1;
            midiActive.store (false);
        }
        else if (juce::isPositiveAndBelow (index, Presets::kNumSlots))
        {
            latchedNote = note;
            midiPreset.store (index);
            midiActive.store (true);
        }

        return;
    }

    if      (m.isNoteOn())  heldNotes.add (m.getNoteNumber());
    else if (m.isNoteOff()) heldNotes.removeValue (m.getNoteNumber());
    else                    return;

    // Basili notalardan slot araligina dusen en tizi kazanir.  Aralik
    // disindaki bir nota, altindaki gecerli notayi gizlememeli.
    int winner = -1;

    for (int i = heldNotes.size() - 1; i >= 0 && winner < 0; --i)
    {
        const int index = heldNotes[i] - midiBaseNote;

        if (juce::isPositiveAndBelow (index, Presets::kNumSlots))
            winner = index;
    }

    if (winner >= 0)
        midiPreset.store (winner);

    midiActive.store (winner >= 0);
}

void KaradagBeatProcessor::applyDesiredPreset()
{
    // MIDI tetiklemesi parametreyi ezer
    const int desiredPreset = midiActive.load() ? midiPreset.load()
                                                : (int) pPreset->load();

    // Yalnizca preset SECIMI degistiginde yukle.  Kullanicinin elle cizdigi
    // zarf geldikten sonra bunu tekrar uygularsak cizimi her blokta sileriz.
    if (desiredPreset != lastRequestedPreset)
    {
        const juce::SpinLock::ScopedTryLockType lock (slotLock);

        if (lock.isLocked())
        {
            applySlotToAudioEnvelopes (desiredPreset);
            lastRequestedPreset = desiredPreset;
        }
        // kilit o an GUI'deyse yukleme bir sonraki parcaya kalir
    }
}

//==============================================================================
void KaradagBeatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const bool midiEnabled = beginMidiBlock();

    applyDesiredPreset();

    // --- GUI'den elle duzenlenmis zarf geldi mi? ---
    if (hasPending.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (publishLock);

        if (lock.isLocked())
        {
            audioTime  .setPoints (pendingTime  .getPoints());
            audioVolume.setPoints (pendingVolume.getPoints());
            hasPending.store (false);
        }
    }

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

    if (playing && havePpq)
    {
        double phase = std::fmod (ppq / patternBeats, 1.0);

        if (phase < 0.0)
            phase += 1.0;

        engine.setPhase (phase);
    }

    // --- blogu MIDI olaylarinin dustugu noktalarda bolerek isle ---
    // Pattern degisimi, notanin blok icindeki tam sample'inda gerceklessin.
    // Aksi halde vurusa konan bir nota pattern'i bir buffer boyu erken degistirir.
    const bool  timeOn = pTimeOn->load() > 0.5f;
    const bool  volOn  = pVolOn ->load() > 0.5f;
    const float mix    = pMix   ->load();

    const int numSamples = buffer.getNumSamples();
    int cursor = 0;

    auto renderUntil = [&] (int end)
    {
        end = juce::jlimit (cursor, numSamples, end);

        if (end > cursor)
        {
            juce::AudioBuffer<float> part (buffer.getArrayOfWritePointers(),
                                           buffer.getNumChannels(), cursor, end - cursor);

            engine.processBlock (part, audioTime, audioVolume, timeOn, volOn, mix);
        }

        cursor = end;
    };

    if (midiEnabled)
    {
        for (const auto meta : midi)
        {
            renderUntil (meta.samplePosition);
            handleMidiMessage (meta.getMessage());
            applyDesiredPreset();
        }
    }

    renderUntil (numSamples);

    displayPhase.store (engine.getPhase());
}

//==============================================================================
juce::AudioProcessorEditor* KaradagBeatProcessor::createEditor()
{
    return new KaradagBeatEditor (*this);
}

void KaradagBeatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("timeEnv",   editTime  .toString(), nullptr);
    state.setProperty ("volumeEnv", editVolume.toString(), nullptr);

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

    const auto timeText = state.getProperty ("timeEnv").toString();
    const auto volText  = state.getProperty ("volumeEnv").toString();

    if (timeText.isNotEmpty()) editTime  .fromString (timeText);
    if (volText .isNotEmpty()) editVolume.fromString (volText);

    publishEnvelopes();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KaradagBeatProcessor();
}
