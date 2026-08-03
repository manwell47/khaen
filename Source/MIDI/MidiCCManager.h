#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>

//==============================================================================
/**
 * MidiCCManager — Real-time MIDI CC Mapping & MIDI Learn Manager
 *
 * Designed for immediate plug-and-play with hardware MIDI USB controllers,
 * master keyboards, wind controllers (EWI) and standard MIDI CC gear.
 *
 * Thread-safe:
 *  - Real-time audio thread lookup is O(1) lock-free via atomic parameter pointers.
 *  - GUI / message thread handles interactive learning, context menus and persistence.
 */
class MidiCCManager : private juce::AsyncUpdater
{
public:
    //==========================================================================
    struct Mapping
    {
        juce::String paramID;
        int ccNumber = -1;      // 0..127 (-1 = unassigned)
        int channel  = 0;       // 0 = Any/Omni, 1..16 = specific MIDI channel
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void midiMappingsChanged() = 0;
        virtual void midiLearnStateChanged (const juce::String& paramID, bool isLearning) = 0;
    };

    //==========================================================================
    MidiCCManager();
    ~MidiCCManager() override;

    void setAPVTS (juce::AudioProcessorValueTreeState* apvts);

    // ── Audio-thread MIDI CC message processing ──────────────────────────────
    /**
     * Called from processBlock() when a MIDI CC message is received.
     * Returns true if the CC was handled (either learned or mapped to a parameter).
     */
    bool handleMidiCC (int channel, int ccNumber, float raw0To127Value) noexcept;

    // ── MIDI Learn Controls ──────────────────────────────────────────────────
    void startLearning (const juce::String& paramID);
    void stopLearning();
    bool isLearning() const noexcept { return m_isLearning.load(std::memory_order_acquire); }
    bool isLearningParam (const juce::String& paramID) const;
    juce::String getLearningParamID() const;

    // ── Manual & Preset Mappings ─────────────────────────────────────────────
    void setMapping (const juce::String& paramID, int ccNumber, int channel = 0);
    void clearMapping (const juce::String& paramID);
    void clearAll();

    int  getCCForParam (const juce::String& paramID) const;
    int  getChannelForParam (const juce::String& paramID) const;
    juce::String getParamForCC (int ccNumber, int channel = 0) const;

    std::vector<Mapping> getAllMappings() const;

    /** Preset for Standard MIDI Controllers (CC 1 Mod, CC 2 Breath, CC 11 Expression, CC 71 Timbre, CC 74 Brightness, CC 91 Reverb, CC 93 Delay) */
    void applyStandardMidiCC();

    // ── XML Persistence & User Defaults ──────────────────────────────────────
    void saveToXml (juce::XmlElement& xml) const;
    void loadFromXml (const juce::XmlElement& xml);

    void saveAsGlobalDefault();
    bool loadGlobalDefault();

    // ── Listeners ────────────────────────────────────────────────────────────
    void addListener (Listener* listener);
    void removeListener (Listener* listener);

private:
    void handleAsyncUpdate() override;
    void rebuildFastLookupTable();

    //==========================================================================
    juce::AudioProcessorValueTreeState* m_apvts = nullptr;

    mutable std::mutex m_mapMutex;
    std::unordered_map<juce::String, Mapping> m_mappings;

    // Fast lock-free audio thread lookup: CC 0..127 -> Parameter pointer
    std::array<std::atomic<juce::RangedAudioParameter*>, 128> m_ccLookup {};

    // MIDI Learn state
    std::atomic<bool> m_isLearning { false };
    std::atomic<juce::RangedAudioParameter*> m_learningParam { nullptr };
    std::atomic<int> m_learnedCC { -1 };
    std::atomic<int> m_learnedChannel { 0 };
    mutable std::mutex m_learnMutex;
    juce::String m_learningParamID;

    juce::ListenerList<Listener> m_listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiCCManager)
};
