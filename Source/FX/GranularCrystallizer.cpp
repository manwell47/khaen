#include "GranularCrystallizer.h"
#include <cmath>
#include <algorithm>

//==============================================================================
GranularCrystallizer::GranularCrystallizer() { reset(); }

void GranularCrystallizer::prepare (double sampleRate, int /*blockSize*/)
{
    m_sampleRate  = sampleRate;
    m_capBufSize  = static_cast<int>(sampleRate * kMaxBufSec);
    m_delBufSize  = static_cast<int>(sampleRate * kMaxBufSec);

    m_capBufL.assign(m_capBufSize, 0.f);
    m_capBufR.assign(m_capBufSize, 0.f);
    m_delBufL.assign(m_delBufSize, 0.f);
    m_delBufR.assign(m_delBufSize, 0.f);
    m_frozenL.assign(m_capBufSize, 0.f);
    m_frozenR.assign(m_capBufSize, 0.f);

    reset();
}

void GranularCrystallizer::reset()
{
    m_capWritePos   = 0;
    m_delWritePos   = 0;
    m_frozenReadPos = 0.f;
    m_frozenSize    = 0;
    m_isFrozen      = false;
    m_grainPhase    = 0.f;
    m_grainReadCursor = 0;
    m_fbLPL = m_fbLPR = 0.f;
}

void GranularCrystallizer::setParameters (const Parameters& p)
{
    m_params = p;
}

void GranularCrystallizer::triggerFreeze (bool on)
{
    if (on && !m_isFrozen)
    {
        // Capture current loop from the capture buffer
        int loopSamples = static_cast<int>(m_params.loopSizeMs * 0.001 * m_sampleRate);
        loopSamples = juce::jlimit(32, m_capBufSize, loopSamples);
        m_frozenSize = loopSamples;

        for (int i = 0; i < loopSamples; ++i)
        {
            int idx = (m_capWritePos - loopSamples + i + m_capBufSize) % m_capBufSize;
            m_frozenL[i] = m_capBufL[idx];
            m_frozenR[i] = m_capBufR[idx];
        }
        m_frozenReadPos = 0.f;
    }
    m_isFrozen = on;
}

//==============================================================================
float GranularCrystallizer::readCircular (const std::vector<float>& buf,
                                           int bufSize, int writePos,
                                           float delaySamples) const
{
    if (bufSize <= 0) return 0.f;

    int intDelay = static_cast<int>(delaySamples);
    float frac   = delaySamples - static_cast<float>(intDelay);

    int idx0 = (writePos - intDelay) % bufSize;
    if (idx0 < 0) idx0 += bufSize;
    
    int idx1 = (writePos - intDelay - 1) % bufSize;
    if (idx1 < 0) idx1 += bufSize;

    return buf[idx0] * (1.f - frac) + buf[idx1] * frac;
}

float GranularCrystallizer::pitchShift (const std::vector<float>& buf,
                                         int bufSize, int readBase,
                                         float pitchRatio, float phase) const
{
    float rp  = static_cast<float>(readBase) + phase * pitchRatio;

    // Guard against NaN/inf read positions: casting those to int is undefined
    // behaviour and can yield a negative index (out-of-bounds read -> crash).
    if (!std::isfinite (rp))
        rp = 0.0f;
    else if (rp < 0.0f || rp >= static_cast<float>(bufSize))
        rp = std::fmod (std::fmod (rp, static_cast<float>(bufSize)) + static_cast<float>(bufSize),
                        static_cast<float>(bufSize));

    int   i0  = static_cast<int>(rp) % bufSize;
    int   i1  = (i0 + 1) % bufSize;
    float frac = rp - std::floor(rp);
    return buf[i0] * (1.f - frac) + buf[i1] * frac;
}

//==============================================================================
void GranularCrystallizer::processModeReverseEcho (float* L, float* R, int n)
{
    int grainSamples = static_cast<int>(m_params.grainSizeMs * 0.001 * m_sampleRate);
    grainSamples = juce::jlimit(32, m_capBufSize / 2, grainSamples);

    float delayFrac = m_params.delayMs * 0.001f * static_cast<float>(m_sampleRate);

    for (int i = 0; i < n; ++i)
    {
        // Write input into capture buffer
        m_capBufL[m_capWritePos] = L[i];
        m_capBufR[m_capWritePos] = R[i];

        // Read a reversed grain from the capture buffer
        // Phase goes 0→1 over grainSamples, but we read BACKWARDS
        float phase = m_grainPhase / static_cast<float>(grainSamples);
        float win   = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * phase));

        int revIdx = (m_capWritePos - m_grainReadCursor + m_capBufSize) % m_capBufSize;
        float grainL = m_capBufL[revIdx] * win;
        float grainR = m_capBufR[revIdx] * win;

        m_grainReadCursor--;
        if (m_grainReadCursor < 0) m_grainReadCursor = grainSamples;

        m_grainPhase++;
        if (m_grainPhase >= grainSamples) m_grainPhase = 0.f;

        // Write grain into delay buffer with feedback (soft-clipped to prevent explosion)
        float fbL = readCircular(m_delBufL, m_delBufSize, m_delWritePos, delayFrac);
        float fbR = readCircular(m_delBufR, m_delBufSize, m_delWritePos, delayFrac);

        m_delBufL[m_delWritePos] = std::tanh(grainL + fbL * m_params.feedback);
        m_delBufR[m_delWritePos] = std::tanh(grainR + fbR * m_params.feedback);

        m_capWritePos = (m_capWritePos + 1) % m_capBufSize;
        m_delWritePos = (m_delWritePos + 1) % m_delBufSize;

        L[i] = L[i] + fbL * m_params.mix;
        R[i] = R[i] + fbR * m_params.mix;
    }
}

//==============================================================================
void GranularCrystallizer::processModePitchFeedback (float* L, float* R, int n)
{
    float pitchRatio = std::pow(2.f, m_params.pitchInterval / 12.f);
    float delayFrac  = m_params.delayMs * 0.001f * static_cast<float>(m_sampleRate);

    for (int i = 0; i < n; ++i)
    {
        // Write dry input to capture buffer
        m_capBufL[m_capWritePos] = L[i];
        m_capBufR[m_capWritePos] = R[i];

        // Read delayed feedback and pitch-shift it
        float fbL = readCircular(m_delBufL, m_delBufSize, m_delWritePos, delayFrac);
        float fbR = readCircular(m_delBufR, m_delBufSize, m_delWritePos, delayFrac);

        // Pitch-shift feedback signal
        m_grainPhase += 1.f;
        float psL = pitchShift(m_delBufL, m_delBufSize, m_delWritePos,
                               pitchRatio, m_grainPhase / m_sampleRate);
        float psR = pitchShift(m_delBufR, m_delBufSize, m_delWritePos,
                               pitchRatio, m_grainPhase / m_sampleRate);

        // LP smooth feedback to prevent runaway
        m_fbLPL = 0.995f * m_fbLPL + 0.005f * psL;
        m_fbLPR = 0.995f * m_fbLPR + 0.005f * psR;

        m_delBufL[m_delWritePos] = std::tanh(L[i] + m_fbLPL * m_params.feedback);
        m_delBufR[m_delWritePos] = std::tanh(R[i] + m_fbLPR * m_params.feedback);

        m_capWritePos = (m_capWritePos + 1) % m_capBufSize;
        m_delWritePos = (m_delWritePos + 1) % m_delBufSize;

        L[i] = L[i] + fbL * m_params.mix;
        R[i] = R[i] + fbR * m_params.mix;
    }
}

//==============================================================================
void GranularCrystallizer::processModeGlitchedFreeze (float* L, float* R, int n)
{
    // Always capture to buffer (so we have fresh material to freeze)
    for (int i = 0; i < n; ++i)
        m_capBufL[m_capWritePos] = L[i],
        m_capBufR[m_capWritePos] = R[i],
        m_capWritePos = (m_capWritePos + 1) % m_capBufSize;

    // Apply freeze trigger if set in params
    if (m_params.freezeGate != m_isFrozen)
        triggerFreeze(m_params.freezeGate);

    if (m_isFrozen && m_frozenSize > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            int   ri  = static_cast<int>(m_frozenReadPos) % m_frozenSize;
            float wetL = m_frozenL[ri];
            float wetR = m_frozenR[ri];

            m_frozenReadPos += 1.f;
            if (m_frozenReadPos >= m_frozenSize) m_frozenReadPos = 0.f;

            // Subtle Hann window for smooth looping
            float win = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi
                                                * (m_frozenReadPos / m_frozenSize)));

            L[i] = L[i] + wetL * win * m_params.mix;
            R[i] = R[i] + wetR * win * m_params.mix;
        }
    }
}

//==============================================================================
void GranularCrystallizer::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (m_capBufSize <= 0 || m_delBufSize <= 0 || m_params.bypass || (m_params.mix < 0.001f && !m_params.freezeGate))
        return;

    float* L = buffer.getWritePointer(0);
    float* R = numChannels > 1 ? buffer.getWritePointer(1) : L;

    switch (m_params.mode)
    {
        case Mode::ReverseEcho:    processModeReverseEcho(L, R, numSamples);    break;
        case Mode::PitchFeedback:  processModePitchFeedback(L, R, numSamples);  break;
        case Mode::GlitchedFreeze: processModeGlitchedFreeze(L, R, numSamples); break;
    }
}
