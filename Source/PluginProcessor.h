#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/KhaenSynthesiser.h"
#include "FX/TapeDelay.h"
#include "FX/Harmonizer.h"
#include "FX/GranularCrystallizer.h"
#include "FX/ReverbEngine.h"
#include "Presets/PresetManager.h"
#include "MIDI/MidiCCManager.h"

//==============================================================================
/**
 * KhaenAudioProcessor — Main Audio Processor
 *
 * Architecture:
 *   MIDI In → MPE Synthesiser (16 KhaenVoice)
 *          → Post-FX Bus: TapeDelay → Harmonizer → GranularCrystallizer → ReverbEngine
 *          → Audio Out
 *
 * All parameters managed by APVTS.
 * Latency reported to host (Cubase Pro compatible).
 * supportsMPE() returns true.
 */
class KhaenAudioProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    KhaenAudioProcessor();
    ~KhaenAudioProcessor() override;

    //==========================================================================
    // AudioProcessor overrides
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;


    #ifdef JucePlugin_Name
    const juce::String getName() const override { return JucePlugin_Name; }
    #else
    const juce::String getName() const override { return "Khaen"; }
    #endif

    bool   acceptsMidi()   const override { return true; }
    bool   producesMidi()  const override { return false; }
    bool   isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 3.5; } // reverb / delay tail
    bool   supportsMPE()   const override { return true; }

    //==========================================================================
    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    //==========================================================================
    // Public accessors for the editor
    juce::AudioProcessorValueTreeState& getAPVTS() { return m_apvts; }
    PresetManager& getPresetManager()              { return m_presetManager; }
    MidiCCManager& getMidiCCManager()              { return m_midiCCManager; }
    KhaenSynthesiser& getSynthesiser()             { return m_synth; }
    ReverbEngine& getReverb()                      { return m_reverb; }

    /** Load a preset by index (thread-safe via message thread) */
    void loadPreset (int index);

    /** Get the current output RMS level (for VU meter) */
    float getOutputRms() const noexcept { return m_outputRms.load(); }

    /** Get raw audio sample for waveform visualizer (ring buffer) */
    float getVisualizerSample() noexcept;

    /** Get instantaneous detected/active MIDI note number (-1.f if none) */
    float getActiveNoteNumber() const noexcept { return m_activeNoteNumber.load(); }

    /** Get instantaneous global breath cycle phase and inhale/exhale direction */
    float getBreathPhase() const noexcept { return FreeReedModel::getGlobalBreathPhase(); }
    bool  isBreathInhale() const noexcept { return FreeReedModel::isGlobalInhale(); }
    float getKeyswitchPulseFlash() const noexcept { return m_keyswitchPulseFlash.load(); }
    int   getLastKeyswitchNote() const noexcept { return m_lastKeyswitchNote.load(); }

    /** Returns true if the instrument is actively being played or producing sound */
    bool isInstrumentActive() const noexcept
    {
        return m_activeNoteNumber.load(std::memory_order_relaxed) >= 0.f
            || m_outputRms.load(std::memory_order_relaxed) > 0.001f
            || m_keyswitchPulseFlash.load(std::memory_order_relaxed) > 0.02f;
    }

    /** Consume MIDI activity flag for LED indicator (lock-free) */
    bool consumeMidiActivity() noexcept;
    bool hasMidiActivity() const noexcept;

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Map APVTS parameters to DSP modules */
    void syncParameters();

    /** Build FreeReedModel::Parameters from APVTS (honouring macros) */
    FreeReedModel::Parameters buildPhysicsParams() const;

    //==========================================================================
    juce::AudioProcessorValueTreeState m_apvts;

    KhaenSynthesiser     m_synth;
    TapeDelay            m_tapeDelay;
    Harmonizer           m_harmonizer;
    GranularCrystallizer m_crystallizer;
    ReverbEngine         m_reverb;
    PresetManager        m_presetManager;
    MidiCCManager        m_midiCCManager;

    // Visualizer ring buffer (lock-free)
    static constexpr int kVisBufSize = 4096;
    std::array<float, kVisBufSize> m_visBuf {};
    std::atomic<int>   m_visWritePos { 0 };
    std::atomic<int>   m_visReadPos  { 0 };
    std::atomic<float> m_outputRms   { 0.f };
    std::atomic<float> m_activeNoteNumber { -1.f };
    std::atomic<float> m_keyswitchPulseFlash { 0.f };
    std::atomic<int>   m_lastKeyswitchNote   { -1 };
    std::atomic<float> m_lastKeyswitchVel    { 0.f };

    // Active held notes tracking for bass suction lock
    std::array<bool, 128> m_heldNotes {};
    int m_heldBassNoteCount = 0;

    // Parameter raw pointers (lock-free access in processBlock)
    std::atomic<float>* m_pReedStiffness  = nullptr;
    std::atomic<float>* m_pBoreDiameter   = nullptr;
    std::atomic<float>* m_pBlowPressure   = nullptr;
    std::atomic<float>* m_pLipDamping     = nullptr;
    std::atomic<float>* m_pBoreReflection = nullptr;
    std::atomic<float>* m_pMaterial       = nullptr;

    std::atomic<float>* m_pMacroAir         = nullptr;
    std::atomic<float>* m_pMacroEmbouchure  = nullptr;
    std::atomic<float>* m_pMacroResonance   = nullptr;

    std::atomic<float>* m_pBreathSpeed    = nullptr;
    std::atomic<float>* m_pSuctionBite    = nullptr;
    std::atomic<float>* m_pAutoInhaleRiff = nullptr;
    std::atomic<float>* m_pArticulation   = nullptr;
    std::atomic<float>* m_pTongueContact  = nullptr;
    std::atomic<float>* m_pVocalTract     = nullptr;

    std::atomic<float>* m_pDelayTime        = nullptr;
    std::atomic<float>* m_pDelayFeedback    = nullptr;
    std::atomic<float>* m_pDelayMix         = nullptr;
    std::atomic<float>* m_pDelayPingPong    = nullptr;
    std::atomic<float>* m_pDelayWow         = nullptr;
    std::atomic<float>* m_pDelayFlutter     = nullptr;
    std::atomic<float>* m_pDelayBypass      = nullptr;

    std::atomic<float>* m_pHarmonShift      = nullptr;
    std::atomic<float>* m_pHarmonMix        = nullptr;
    std::atomic<float>* m_pHarmonVoices     = nullptr;
    std::atomic<float>* m_pHarmonDetune     = nullptr;
    std::atomic<float>* m_pHarmonBypass     = nullptr;

    std::atomic<float>* m_pCrystalGrain    = nullptr;
    std::atomic<float>* m_pCrystalDelay    = nullptr;
    std::atomic<float>* m_pCrystalFeedback = nullptr;
    std::atomic<float>* m_pCrystalMix      = nullptr;
    std::atomic<float>* m_pCrystalMode     = nullptr;
    std::atomic<float>* m_pCrystalFreeze   = nullptr;
    std::atomic<float>* m_pCrystalPitch    = nullptr;
    std::atomic<float>* m_pCrystalLoopMs   = nullptr;
    std::atomic<float>* m_pCrystalBypass   = nullptr;

    std::atomic<float>* m_pReverbSize      = nullptr;
    std::atomic<float>* m_pReverbDecay     = nullptr;
    std::atomic<float>* m_pReverbDamp      = nullptr;
    std::atomic<float>* m_pReverbWidth     = nullptr;
    std::atomic<float>* m_pReverbMix       = nullptr;
    std::atomic<float>* m_pReverbPreset    = nullptr;
    std::atomic<float>* m_pReverbBypass    = nullptr;
    
    std::atomic<int>    m_midiActivity {0};
    double              m_sampleRate { 44100.0 };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KhaenAudioProcessor)
};
