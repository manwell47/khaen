#pragma once
#include <JuceHeader.h>
#include "KhaenVoice.h"

//==============================================================================
/**
 * KhaenSynthesiser — MPE Synthesiser
 *
 * Manages a pool of 16 KhaenVoice instances for full MPE polyphony.
 * Configures MPE zones and handles voice allocation.
 */
class KhaenSynthesiser : public juce::MPESynthesiser
{
public:
    static constexpr int kNumVoices = 16;

    KhaenSynthesiser();
    ~KhaenSynthesiser() override = default;

    void prepare (double sampleRate, int blockSize);

    /** Apply parameters to all active and future voices */
    void setModelParameters (const FreeReedModel::Parameters& params);

    /** Retrigger breath pulse across all currently sounding voices (0:09-0:27 keyswitch mechanic) */
    void retriggerAllVoices (float velocity, FreeReedModel::BreathDirection dir);

    /** Get raw pointer to a voice (for visualizer access) */
    KhaenVoice* getVoice (int index);

private:
    double m_sampleRate = 44100.0;
    int    m_blockSize  = 512;
    FreeReedModel::Parameters m_params;

    std::vector<KhaenVoice*> m_voicePtrs; // non-owning, JUCE owns via addVoice
};
