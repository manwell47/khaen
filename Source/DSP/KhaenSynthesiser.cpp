#include "KhaenSynthesiser.h"

//==============================================================================
KhaenSynthesiser::KhaenSynthesiser()
{
    // Add voices to the pool
    for (int i = 0; i < kNumVoices; ++i)
    {
        auto* voice = new KhaenVoice();
        m_voicePtrs.push_back(voice);
        addVoice(voice);
    }

    // Start in legacy mode (accepts notes on all channels, like standard MIDI).
    // It will automatically upgrade to MPE if an MPE Configuration Message (MCM) is received.
    enableLegacyMode(48);
}

//==============================================================================
void KhaenSynthesiser::prepare (double sampleRate, int blockSize)
{
    m_sampleRate = sampleRate;
    m_blockSize  = blockSize;

    setCurrentPlaybackSampleRate(sampleRate);

    for (auto* voice : m_voicePtrs)
        voice->prepare(sampleRate, blockSize);
}

//==============================================================================
void KhaenSynthesiser::setModelParameters (const FreeReedModel::Parameters& params)
{
    m_params = params;
    for (auto* voice : m_voicePtrs)
        voice->setModelParameters(params);
}

//==============================================================================
void KhaenSynthesiser::retriggerAllVoices (float velocity, FreeReedModel::BreathDirection dir)
{
    for (auto* voice : m_voicePtrs)
    {
        if (voice != nullptr && voice->isVoiceActive())
            voice->retriggerBreathPulse(velocity, dir);
    }
}

//==============================================================================
KhaenVoice* KhaenSynthesiser::getVoice (int index)
{
    if (index >= 0 && index < static_cast<int>(m_voicePtrs.size()))
        return m_voicePtrs[index];
    return nullptr;
}
