#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/PresetManager.h"

using APVTS = juce::AudioProcessorValueTreeState;
using Params = APVTS::ParameterLayout;

//==============================================================================
APVTS::ParameterLayout KhaenAudioProcessor::createParameterLayout()
{
    Params params;

    auto pct  = [](float v) { return juce::NormalisableRange<float>(0.f, 1.f, 0.001f); (void)v; };
    auto ms   = [](float lo, float hi) { return juce::NormalisableRange<float>(lo, hi, 0.1f); };
    auto semi = []() { return juce::NormalisableRange<float>(-12.f, 12.f, 0.5f); };

    // ── Physical model ────────────────────────────────────────────────────────
    params.add(std::make_unique<juce::AudioParameterFloat>("reedStiffness",  "Reed Stiffness",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.65f));
    params.add(std::make_unique<juce::AudioParameterFloat>("boreDiameter",   "Bore Diameter (mm)",
        juce::NormalisableRange<float>(8.f, 30.f, 0.1f), 14.f));
    params.add(std::make_unique<juce::AudioParameterFloat>("blowPressure",   "Blow Pressure",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.55f));
    params.add(std::make_unique<juce::AudioParameterFloat>("lipDamping",     "Lip Damping",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.30f));
    params.add(std::make_unique<juce::AudioParameterFloat>("boreReflection", "Bore Reflection",
        juce::NormalisableRange<float>(0.80f, 0.999f, 0.001f), 0.97f));
    juce::StringArray materials { "Bamboo", "Aged Bamboo", "Jade", "Brass", "PVC", "Dark Wood", "Metal" };
    params.add(std::make_unique<juce::AudioParameterChoice>("material", "Material", materials, 0));
    params.add(std::make_unique<juce::AudioParameterFloat>("breathSpeed", "Breath Speed",
        juce::NormalisableRange<float>(0.20f, 3.0f, 0.05f), 1.20f));
    params.add(std::make_unique<juce::AudioParameterFloat>("suctionBite", "Suction Bite",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));
    params.add(std::make_unique<juce::AudioParameterBool>("autoInhaleRiff", "Auto-Riff Inhale/Exhale", true));

    // ── Hunt-Crossley Articulation & Vocal Tract (IWK/MDW Vienna Research) ─────
    juce::StringArray articulations { "Legato", "Portato", "Staccato", "Slap Tongue" };
    params.add(std::make_unique<juce::AudioParameterChoice>("articulation", "Articulation", articulations, 0));
    params.add(std::make_unique<juce::AudioParameterFloat>("tongueContact", "Tongue Contact (ms)",
        juce::NormalisableRange<float>(10.0f, 100.0f, 0.5f), 35.0f));
    params.add(std::make_unique<juce::AudioParameterFloat>("vocalTract", "Vocal Tract Formant",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    // ── Macros ────────────────────────────────────────────────────────────────
    params.add(std::make_unique<juce::AudioParameterFloat>("macroAir",        "Air",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.50f));
    params.add(std::make_unique<juce::AudioParameterFloat>("macroEmbouchure", "Embouchure",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.50f));
    params.add(std::make_unique<juce::AudioParameterFloat>("macroResonance",  "Resonance",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.50f));

    // ── Tape Delay ────────────────────────────────────────────────────────────
    params.add(std::make_unique<juce::AudioParameterFloat>("delayTime",     "Delay Time",
        ms(10.f, 2000.f), 400.f));
    params.add(std::make_unique<juce::AudioParameterFloat>("delayFeedback", "Delay Feedback",
        juce::NormalisableRange<float>(0.f, 0.95f, 0.01f), 0.30f));
    params.add(std::make_unique<juce::AudioParameterFloat>("delayMix",      "Delay Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.0f));
    params.add(std::make_unique<juce::AudioParameterBool>("delayPingPong", "Ping-Pong", false));
    params.add(std::make_unique<juce::AudioParameterFloat>("delayWow",      "Delay Wow",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.15f));
    params.add(std::make_unique<juce::AudioParameterFloat>("delayFlutter",  "Delay Flutter",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.08f));
    params.add(std::make_unique<juce::AudioParameterFloat>("delayHfDamp",   "HF Damping",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.65f));
    params.add(std::make_unique<juce::AudioParameterBool>("delayBypass",    "Delay Bypass", false));

    // ── Harmonizer ────────────────────────────────────────────────────────────
    params.add(std::make_unique<juce::AudioParameterFloat>("harmonShift",  "Harmonizer Shift",
        semi(), 0.f));
    params.add(std::make_unique<juce::AudioParameterFloat>("harmonMix",    "Harmonizer Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.0f));
    params.add(std::make_unique<juce::AudioParameterFloat>("harmonVoices", "Harm Spread",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.3f));
    params.add(std::make_unique<juce::AudioParameterFloat>("harmonDetune", "Detune (cents)",
        juce::NormalisableRange<float>(-50.f, 50.f, 0.5f), 0.f));
    params.add(std::make_unique<juce::AudioParameterBool>("harmonBypass",  "Harmonizer Bypass", false));

    // ── Granular Crystallizer ─────────────────────────────────────────────────
    params.add(std::make_unique<juce::AudioParameterFloat>("crystalGrainMs",  "Grain Size (ms)",
        ms(20.f, 500.f), 120.f));
    params.add(std::make_unique<juce::AudioParameterFloat>("crystalDelayMs",  "Crystal Delay (ms)",
        ms(10.f, 1000.f), 350.f));
    params.add(std::make_unique<juce::AudioParameterFloat>("crystalFeedback", "Crystal Feedback",
        juce::NormalisableRange<float>(0.f, 0.90f, 0.01f), 0.35f));
    params.add(std::make_unique<juce::AudioParameterFloat>("crystalMix",      "Crystal Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.0f));
    juce::StringArray crystalModes { "Reverse Echo", "Pitch Feedback", "Glitched Freeze" };
    params.add(std::make_unique<juce::AudioParameterChoice>("crystalMode", "Crystal Mode", crystalModes, 0));
    params.add(std::make_unique<juce::AudioParameterBool>("crystalFreeze", "Freeze Gate", false));
    params.add(std::make_unique<juce::AudioParameterFloat>("crystalLoopMs",   "Loop Size (ms)",
        ms(20.f, 500.f), 80.f));
    params.add(std::make_unique<juce::AudioParameterFloat>("crystalPitch",    "Crystal Pitch",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.5f), 12.f));
    params.add(std::make_unique<juce::AudioParameterBool>("crystalBypass",    "Crystallizer Bypass", false));

    // ── Reverb Engine (Laos Presets) ──────────────────────────────────────────
    params.add(std::make_unique<juce::AudioParameterFloat>("reverbSize",   "Reverb Size",
        juce::NormalisableRange<float>(0.10f, 1.0f, 0.01f), 0.70f));
    params.add(std::make_unique<juce::AudioParameterFloat>("reverbDecay",  "Reverb Decay",
        juce::NormalisableRange<float>(0.10f, 0.98f, 0.01f), 0.70f));
    params.add(std::make_unique<juce::AudioParameterFloat>("reverbDamp",   "Reverb Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f));
    params.add(std::make_unique<juce::AudioParameterFloat>("reverbWidth",  "Reverb Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.85f));
    params.add(std::make_unique<juce::AudioParameterFloat>("reverbMix",    "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));
    juce::StringArray laosPresets {
        "Wat Phu (Ancient Temple)",
        "Luang Prabang Sanctuary",
        "Tham Kong Lor (Giant Cave)",
        "That Luang Pavilion",
        "Mekong River Grotto"
    };
    params.add(std::make_unique<juce::AudioParameterChoice>("reverbPreset", "Reverb Preset", laosPresets, 0));
    params.add(std::make_unique<juce::AudioParameterBool>("reverbBypass",  "Reverb Bypass", false));

    return params;
}

//==============================================================================
KhaenAudioProcessor::KhaenAudioProcessor()
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      m_apvts(*this, nullptr, "KhaenState", createParameterLayout())
{
    // Cache raw parameter pointers for lock-free audio-thread access
    m_pReedStiffness   = m_apvts.getRawParameterValue("reedStiffness");
    m_pBoreDiameter    = m_apvts.getRawParameterValue("boreDiameter");
    m_pBlowPressure    = m_apvts.getRawParameterValue("blowPressure");
    m_pLipDamping      = m_apvts.getRawParameterValue("lipDamping");
    m_pBoreReflection  = m_apvts.getRawParameterValue("boreReflection");
    m_pMaterial        = m_apvts.getRawParameterValue("material");
    m_pBreathSpeed     = m_apvts.getRawParameterValue("breathSpeed");
    m_pSuctionBite     = m_apvts.getRawParameterValue("suctionBite");
    m_pAutoInhaleRiff  = m_apvts.getRawParameterValue("autoInhaleRiff");
    m_pArticulation    = m_apvts.getRawParameterValue("articulation");
    m_pTongueContact   = m_apvts.getRawParameterValue("tongueContact");
    m_pVocalTract      = m_apvts.getRawParameterValue("vocalTract");

    m_pMacroAir        = m_apvts.getRawParameterValue("macroAir");
    m_pMacroEmbouchure = m_apvts.getRawParameterValue("macroEmbouchure");
    m_pMacroResonance  = m_apvts.getRawParameterValue("macroResonance");

    m_pDelayTime       = m_apvts.getRawParameterValue("delayTime");
    m_pDelayFeedback   = m_apvts.getRawParameterValue("delayFeedback");
    m_pDelayMix        = m_apvts.getRawParameterValue("delayMix");
    m_pDelayPingPong   = m_apvts.getRawParameterValue("delayPingPong");
    m_pDelayWow        = m_apvts.getRawParameterValue("delayWow");
    m_pDelayFlutter    = m_apvts.getRawParameterValue("delayFlutter");
    m_pDelayBypass     = m_apvts.getRawParameterValue("delayBypass");

    m_pHarmonShift     = m_apvts.getRawParameterValue("harmonShift");
    m_pHarmonMix       = m_apvts.getRawParameterValue("harmonMix");
    m_pHarmonVoices    = m_apvts.getRawParameterValue("harmonVoices");
    m_pHarmonDetune    = m_apvts.getRawParameterValue("harmonDetune");
    m_pHarmonBypass    = m_apvts.getRawParameterValue("harmonBypass");

    m_pCrystalGrain    = m_apvts.getRawParameterValue("crystalGrainMs");
    m_pCrystalDelay    = m_apvts.getRawParameterValue("crystalDelayMs");
    m_pCrystalFeedback = m_apvts.getRawParameterValue("crystalFeedback");
    m_pCrystalMix      = m_apvts.getRawParameterValue("crystalMix");
    m_pCrystalMode     = m_apvts.getRawParameterValue("crystalMode");
    m_pCrystalFreeze   = m_apvts.getRawParameterValue("crystalFreeze");
    m_pCrystalPitch    = m_apvts.getRawParameterValue("crystalPitch");
    m_pCrystalLoopMs   = m_apvts.getRawParameterValue("crystalLoopMs");
    m_pCrystalBypass   = m_apvts.getRawParameterValue("crystalBypass");

    m_pReverbSize      = m_apvts.getRawParameterValue("reverbSize");
    m_pReverbDecay     = m_apvts.getRawParameterValue("reverbDecay");
    m_pReverbDamp      = m_apvts.getRawParameterValue("reverbDamp");
    m_pReverbWidth     = m_apvts.getRawParameterValue("reverbWidth");
    m_pReverbMix       = m_apvts.getRawParameterValue("reverbMix");
    m_pReverbPreset    = m_apvts.getRawParameterValue("reverbPreset");
    m_pReverbBypass    = m_apvts.getRawParameterValue("reverbBypass");
}

KhaenAudioProcessor::~KhaenAudioProcessor() {}

//==============================================================================
void KhaenAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    m_synth.prepare(m_sampleRate, samplesPerBlock);
    
    m_tapeDelay.prepare(m_sampleRate, samplesPerBlock);
    m_harmonizer.prepare(m_sampleRate, samplesPerBlock);
    m_crystallizer.prepare(m_sampleRate, samplesPerBlock);
    m_reverb.prepare(m_sampleRate, samplesPerBlock);

    // No added latency — direct physical model output
    setLatencySamples(0);
}

juce::AudioProcessorEditor* KhaenAudioProcessor::createEditor()
{
    return new KhaenEditor(*this);
}

void KhaenAudioProcessor::releaseResources()
{
    m_tapeDelay.reset();
    m_harmonizer.reset();
    m_crystallizer.reset();
    m_reverb.reset();
}

//==============================================================================
bool KhaenAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;
    return true;
}

//==============================================================================
FreeReedModel::Parameters KhaenAudioProcessor::buildPhysicsParams() const
{
    using M = FreeReedModel::Material;

    float air        = m_pMacroAir->load();
    float embouchure = m_pMacroEmbouchure->load();
    float resonance  = m_pMacroResonance->load();

    // Macros modulate the underlying physics params
    FreeReedModel::Parameters p;
    p.reedStiffness  = juce::jlimit(0.f, 1.f, m_pReedStiffness->load()
                                    + (embouchure - 0.5f) * 0.3f);
    p.boreDiameterMm = juce::jlimit(8.f, 30.f, m_pBoreDiameter->load()
                                    + (resonance - 0.5f) * 6.f);
    p.blowPressure   = juce::jlimit(0.f, 1.f, m_pBlowPressure->load()
                                    + (air - 0.5f) * 0.4f);
    p.lipDamping     = m_pLipDamping->load();
    p.boreReflection = m_pBoreReflection->load();

    int matIdx = static_cast<int>(m_pMaterial->load());
    p.material = static_cast<M>(juce::jlimit(0, 6, matIdx));

    p.breathSpeedHz  = m_pBreathSpeed ? m_pBreathSpeed->load() : 1.20f;
    p.suctionBite    = m_pSuctionBite ? m_pSuctionBite->load() : 0.65f;
    p.autoInhaleRiff = (m_pAutoInhaleRiff && m_pAutoInhaleRiff->load() > 0.5f);

    int artIdx = m_pArticulation ? static_cast<int>(m_pArticulation->load()) : 0;
    p.articulation   = static_cast<FreeReedModel::Articulation>(juce::jlimit(0, 3, artIdx));
    p.tongueContactMs= m_pTongueContact ? m_pTongueContact->load() : 35.0f;
    p.vocalTract     = m_pVocalTract ? m_pVocalTract->load() : 0.50f;

    return p;
}

//==============================================================================
void KhaenAudioProcessor::syncParameters()
{
    // ── Synth physics ─────────────────────────────────────────────────────────
    m_synth.setModelParameters(buildPhysicsParams());

    // ── Tape Delay ────────────────────────────────────────────────────────────
    TapeDelay::Parameters dp;
    dp.delayTimeMs = m_pDelayTime->load();
    dp.feedback    = m_pDelayFeedback->load();
    dp.mix         = m_pDelayMix->load();
    dp.pingPong    = m_pDelayPingPong->load() > 0.5f;
    dp.wow         = m_pDelayWow->load();
    dp.flutter     = m_pDelayFlutter->load();
    dp.bypass      = (m_pDelayBypass && m_pDelayBypass->load() > 0.5f);
    m_tapeDelay.setParameters(dp);

    // ── Harmonizer ────────────────────────────────────────────────────────────
    Harmonizer::Parameters hp;
    hp.shiftSemitones = m_pHarmonShift->load();
    hp.mix            = m_pHarmonMix->load();
    hp.stereoSpread   = m_pHarmonVoices ? m_pHarmonVoices->load() : 0.3f;
    hp.detuneCents    = m_pHarmonDetune ? m_pHarmonDetune->load() : 0.f;
    hp.bypass         = (m_pHarmonBypass && m_pHarmonBypass->load() > 0.5f);
    m_harmonizer.setParameters(hp);

    // ── Crystallizer ──────────────────────────────────────────────────────────
    GranularCrystallizer::Parameters cp;
    cp.grainSizeMs   = m_pCrystalGrain->load();
    cp.delayMs       = m_pCrystalDelay->load();
    cp.feedback      = m_pCrystalFeedback->load();
    cp.mix           = m_pCrystalMix->load();
    cp.mode          = static_cast<GranularCrystallizer::Mode>(
                           static_cast<int>(m_pCrystalMode->load()));
    cp.freezeGate    = m_pCrystalFreeze->load() > 0.5f;
    cp.pitchInterval = m_pCrystalPitch->load();
    cp.loopSizeMs    = m_pCrystalLoopMs->load();
    cp.bypass        = (m_pCrystalBypass && m_pCrystalBypass->load() > 0.5f);
    m_crystallizer.setParameters(cp);

    // ── Reverb Engine ─────────────────────────────────────────────────────────
    ReverbEngine::Parameters rp;
    rp.size    = m_pReverbSize ? m_pReverbSize->load() : 0.70f;
    rp.decay   = m_pReverbDecay ? m_pReverbDecay->load() : 0.70f;
    rp.damp    = m_pReverbDamp ? m_pReverbDamp->load() : 0.40f;
    rp.width   = m_pReverbWidth ? m_pReverbWidth->load() : 0.85f;
    rp.mix     = m_pReverbMix ? m_pReverbMix->load() : 0.25f;
    rp.preset  = m_pReverbPreset ? static_cast<int>(m_pReverbPreset->load()) : 0;
    rp.bypass  = (m_pReverbBypass && m_pReverbBypass->load() > 0.5f);
    m_reverb.setParameters(rp);
}

//==============================================================================
void KhaenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    juce::MidiBuffer synthMidi;

    // Track active note, breath keyswitches & sustained bass notes from MIDI messages
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        int sampleOffset = metadata.samplePosition;

        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            int note = msg.getNoteNumber();
            float vel = msg.getFloatVelocity();

            // Breath Keyswitch detection (octave C1–B1, MIDI notes 24 to 35)
            if (note >= 24 && note <= 35)
            {
                // C1 (24), E1 (28), G1 (31), B1 (35) -> EXHALE (BLOW)
                // D1 (26), F1 (29), A1 (33) -> INHALE (DRAW / SUCTION)
                bool isExhale = (note == 24 || note == 28 || note == 31 || (note % 2 == 0 && note != 26));
                auto dir = isExhale ? FreeReedModel::BreathDirection::Exhale
                                    : FreeReedModel::BreathDirection::Inhale;

                m_synth.retriggerAllVoices(vel, dir);

                m_lastKeyswitchNote.store(note, std::memory_order_relaxed);
                m_lastKeyswitchVel.store(vel, std::memory_order_relaxed);
                m_keyswitchPulseFlash.store(1.0f, std::memory_order_relaxed);
                m_midiActivity.store(1, std::memory_order_relaxed);
                // Do not forward keyswitches to standard pitch allocation synthMidi
                continue;
            }

            if (note >= 0 && note < 128)
            {
                if (!m_heldNotes[note])
                {
                    m_heldNotes[note] = true;
                    if (note <= 60) // C4 or lower
                        m_heldBassNoteCount++;
                }
            }
            m_activeNoteNumber.store(static_cast<float>(note), std::memory_order_relaxed);
            m_midiActivity.store(1, std::memory_order_relaxed);
            synthMidi.addEvent(msg, sampleOffset);
        }
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            int note = msg.getNoteNumber();
            if (note >= 24 && note <= 35)
            {
                // Keyswitch note off (ignore without voice stopping)
                continue;
            }

            if (note >= 0 && note < 128)
            {
                if (m_heldNotes[note])
                {
                    m_heldNotes[note] = false;
                    if (note <= 60)
                        m_heldBassNoteCount = std::max(0, m_heldBassNoteCount - 1);
                }
            }
            synthMidi.addEvent(msg, sampleOffset);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            m_heldNotes.fill(false);
            m_heldBassNoteCount = 0;
            synthMidi.addEvent(msg, sampleOffset);
        }
        else
        {
            synthMidi.addEvent(msg, sampleOffset);
        }
    }

    // Decay visualizer keyswitch pulse flash
    float currentFlash = m_keyswitchPulseFlash.load(std::memory_order_relaxed);
    if (currentFlash > 0.001f)
    {
        double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        float decay = static_cast<float>(buffer.getNumSamples()) / static_cast<float>(sr * 0.15); // 150ms decay
        m_keyswitchPulseFlash.store(std::max(0.0f, currentFlash - decay), std::memory_order_relaxed);
    }

    // Lock continuous respiratory engine into suction when a bass note is sustained
    FreeReedModel::setSustainedBassNote(m_heldBassNoteCount > 0);

    if (!midiMessages.isEmpty())
        m_midiActivity.store(1, std::memory_order_relaxed);

    // Sync all parameters (cheap atomic reads)
    syncParameters();

    buffer.clear();

    // 1. Synthesiser (MPE + Physical Model) with keyswitch-filtered MIDI
    m_synth.renderNextBlock(buffer, synthMidi, 0, buffer.getNumSamples());

    // 2. FX Chain: Delay → Harmonizer → Crystallizer → Reverb
    m_tapeDelay.process(buffer);
    m_harmonizer.process(buffer);
    m_crystallizer.process(buffer);
    m_reverb.process(buffer);

    // 3. Update visualizer ring buffer & RMS
    const float* chanL = buffer.getReadPointer(0);
    float rmsAccum = 0.f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float s = chanL[i];
        rmsAccum += s * s;

        int wp = (m_visWritePos.load() + 1) % kVisBufSize;
        m_visBuf[wp] = s;
        m_visWritePos.store(wp);
    }

    rmsAccum = std::sqrt(rmsAccum / buffer.getNumSamples());
    m_outputRms.store(rmsAccum);

    if (rmsAccum < 0.0005f)
        m_activeNoteNumber.store(-1.f, std::memory_order_relaxed);
}

//==============================================================================
float KhaenAudioProcessor::getVisualizerSample() noexcept
{
    int rp = m_visReadPos.load();
    int wp = m_visWritePos.load();
    if (rp == wp) return 0.f;

    int nextRp = (rp + 1) % kVisBufSize;
    m_visReadPos.store(nextRp);
    return m_visBuf[nextRp];
}

bool KhaenAudioProcessor::consumeMidiActivity() noexcept
{
    return m_midiActivity.exchange(0) != 0;
}

bool KhaenAudioProcessor::hasMidiActivity() const noexcept
{
    return m_midiActivity.load() != 0;
}

//==============================================================================
void KhaenAudioProcessor::loadPreset (int index)
{
    m_presetManager.loadPreset(index, m_apvts);
}

//==============================================================================
void KhaenAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = m_apvts.copyState();
    // Store current preset index
    state.setProperty("currentPreset", m_presetManager.getCurrentPresetIndex(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void KhaenAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(m_apvts.state.getType()))
    {
        auto newState = juce::ValueTree::fromXml(*xmlState);
        m_apvts.replaceState(newState);
    }
}


//==============================================================================
// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KhaenAudioProcessor();
}
