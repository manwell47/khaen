#include "ReverbEngine.h"

//==============================================================================
// Prime base delay times in milliseconds at 44.1kHz
static constexpr float kBaseCombL_ms[4] = { 29.7f, 37.1f, 43.3f, 49.9f };
static constexpr float kBaseCombR_ms[4] = { 31.3f, 38.9f, 45.7f, 52.3f };
static constexpr float kBaseAllPassL_ms[2] = { 5.1f, 1.7f };
static constexpr float kBaseAllPassR_ms[2] = { 5.7f, 1.9f };

//==============================================================================
ReverbEngine::ReverbEngine()
{
    m_smoothMix.reset(64);
    m_smoothDamp.reset(64);
}

//==============================================================================
void ReverbEngine::prepare (double sampleRate, int blockSize)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    float srRatio = static_cast<float>(m_sampleRate / 44100.0);

    // Prepare comb filters
    for (int i = 0; i < 4; ++i)
    {
        float baseDelayL = kBaseCombL_ms[i] * 0.001f * static_cast<float>(m_sampleRate);
        float baseDelayR = kBaseCombR_ms[i] * 0.001f * static_cast<float>(m_sampleRate);
        int maxDelay = static_cast<int>(baseDelayR * 2.5f) + 128;

        float rateL = 0.45f + 0.15f * static_cast<float>(i);
        float rateR = 0.52f + 0.13f * static_cast<float>(i);
        float depth = 6.f * srRatio;

        m_combsL[i].prepare(maxDelay, baseDelayL, rateL, depth);
        m_combsR[i].prepare(maxDelay, baseDelayR, rateR, depth);
    }

    // Prepare allpass diffusers
    for (int i = 0; i < 2; ++i)
    {
        int delayL = static_cast<int>(kBaseAllPassL_ms[i] * 0.001f * static_cast<float>(m_sampleRate));
        int delayR = static_cast<int>(kBaseAllPassR_ms[i] * 0.001f * static_cast<float>(m_sampleRate));
        m_allpassL[i].prepare(delayL, 0.5f);
        m_allpassR[i].prepare(delayR, 0.5f);
    }

    m_smoothMix.reset(static_cast<int>(m_sampleRate * 0.02)); // 20ms smoothing
    m_smoothDamp.reset(static_cast<int>(m_sampleRate * 0.02));

    reset();
}

//==============================================================================
void ReverbEngine::reset()
{
    for (auto& c : m_combsL) c.reset();
    for (auto& c : m_combsR) c.reset();
    for (auto& a : m_allpassL) a.reset();
    for (auto& a : m_allpassR) a.reset();

    m_smoothMix.setCurrentAndTargetValue(m_params.mix);
    m_smoothDamp.setCurrentAndTargetValue(m_params.damp);
}

//==============================================================================
void ReverbEngine::setParameters (const Parameters& p)
{
    m_params = p;
    m_smoothMix.setTargetValue(p.mix);
    m_smoothDamp.setTargetValue(juce::jlimit(0.0f, 0.90f, p.damp));

    // Update delay scale based on Room Size
    float sizeScale = juce::jlimit(0.2f, 2.0f, 0.5f + p.size * 1.0f);
    for (int i = 0; i < 4; ++i)
    {
        m_combsL[i].delayLen = kBaseCombL_ms[i] * 0.001f * static_cast<float>(m_sampleRate) * sizeScale;
        m_combsR[i].delayLen = kBaseCombR_ms[i] * 0.001f * static_cast<float>(m_sampleRate) * sizeScale;
    }
}

//==============================================================================
void ReverbEngine::applyLaosPreset (int presetIndex, Parameters& p)
{
    p.preset = presetIndex;
    switch (presetIndex)
    {
        case 0: // Wat Phu (Ancient Temple) - Champasak sandstone shrine
            p.size  = 0.82f;
            p.decay = 0.78f;
            p.damp  = 0.45f;
            p.width = 0.85f;
            p.mix   = 0.32f;
            break;

        case 1: // Luang Prabang Sanctuary - Golden teakwood meditation hall
            p.size  = 0.65f;
            p.decay = 0.62f;
            p.damp  = 0.30f;
            p.width = 0.75f;
            p.mix   = 0.28f;
            break;

        case 2: // Tham Kong Lor (Giant Cave) - 7.5km subterranean limestone river cavern
            p.size  = 0.96f;
            p.decay = 0.92f;
            p.damp  = 0.58f;
            p.width = 1.00f;
            p.mix   = 0.42f;
            break;

        case 3: // That Luang Pavilion - Open sacred stupa courtyard
            p.size  = 0.50f;
            p.decay = 0.50f;
            p.damp  = 0.18f;
            p.width = 0.80f;
            p.mix   = 0.25f;
            break;

        case 4: // Mekong River Grotto - High river alcove with mist
            p.size  = 0.76f;
            p.decay = 0.82f;
            p.damp  = 0.38f;
            p.width = 0.90f;
            p.mix   = 0.35f;
            break;

        default:
            break;
    }
}

//==============================================================================
juce::String ReverbEngine::getLaosPresetName (int index)
{
    switch (index)
    {
        case 0: return "Wat Phu (Ancient Temple)";
        case 1: return "Luang Prabang Sanctuary";
        case 2: return "Tham Kong Lor (Giant Cave)";
        case 3: return "That Luang Pavilion";
        case 4: return "Mekong River Grotto";
        default: return "Custom Reverb";
    }
}

//==============================================================================
void ReverbEngine::process (juce::AudioBuffer<float>& buffer)
{
    if (m_params.bypass || (m_params.mix <= 0.001f && m_smoothMix.getCurrentValue() <= 0.001f))
        return; // Pure zero-latency dry bypass

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 2) return;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    const float feedback = juce::jlimit(0.10f, 0.97f, 0.40f + m_params.decay * 0.56f);
    const float width = juce::jlimit(0.0f, 1.0f, m_params.width);
    const float wetGainL = 0.5f * (1.0f + width);
    const float wetGainR = 0.5f * (1.0f - width);

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = left[i];
        float inR = right[i];
        float inMono = 0.5f * (inL + inR);

        float currentMix = m_smoothMix.getNextValue();
        float currentDamp = m_smoothDamp.getNextValue();

        // 1. Parallel modulated comb filters
        float combOutL = 0.f;
        float combOutR = 0.f;

        for (int c = 0; c < 4; ++c)
        {
            combOutL += m_combsL[c].process(inMono, feedback, currentDamp, static_cast<float>(m_sampleRate));
            combOutR += m_combsR[c].process(inMono, feedback, currentDamp, static_cast<float>(m_sampleRate));
        }

        combOutL *= 0.25f;
        combOutR *= 0.25f;

        // 2. Cascaded all-pass diffusers
        float diffL = combOutL;
        float diffR = combOutR;

        for (int a = 0; a < 2; ++a)
        {
            diffL = m_allpassL[a].process(diffL);
            diffR = m_allpassR[a].process(diffR);
        }

        // 3. Stereo cross-mix matrix with width control
        float wetL = diffL * wetGainL + diffR * wetGainR;
        float wetR = diffR * wetGainL + diffL * wetGainR;

        // 4. Dry / Wet output blending (constant dry level + parallel wet reverb space)
        left[i]  = inL + currentMix * wetL;
        right[i] = inR + currentMix * wetR;
    }
}
