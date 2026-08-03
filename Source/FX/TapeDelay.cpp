#include "TapeDelay.h"
#include <algorithm>

//==============================================================================
TapeDelay::TapeDelay()
{
    reset();
}

void TapeDelay::prepare (double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_bufSize    = static_cast<int>(m_sampleRate * kMaxDelaySeconds) + 16;

    m_bufL.assign(m_bufSize, 0.f);
    m_bufR.assign(m_bufSize, 0.f);
    reset();
}

void TapeDelay::reset()
{
    if (!m_bufL.empty()) std::fill(m_bufL.begin(), m_bufL.end(), 0.f);
    if (!m_bufR.empty()) std::fill(m_bufR.begin(), m_bufR.end(), 0.f);
    m_writeL = m_writeR = 0;
    m_wowPhase = m_flutterPhase1 = m_flutterPhase2 = 0.f;
    m_fbDampL = m_fbDampR = 0.f;

    float initDelay = static_cast<float>(m_params.delayTimeMs * 0.001 * m_sampleRate);
    m_smoothDelayL = m_smoothDelayR = juce::jlimit(4.f, static_cast<float>(m_bufSize - 8), initDelay);
}

void TapeDelay::setParameters (const Parameters& p)
{
    m_params = p;
}

//==============================================================================
float TapeDelay::readHermite (const std::vector<float>& buf,
                              float delaySamples,
                              int writePos) const
{
    if (m_bufSize <= 0) return 0.f;

    int   intDel = static_cast<int>(delaySamples);
    float frac   = delaySamples - static_cast<float>(intDel);

    auto wrapIdx = [this](int idx) {
        int r = idx % m_bufSize;
        return (r < 0) ? (r + m_bufSize) : r;
    };

    int i0 = wrapIdx(writePos - intDel + 1);
    int i1 = wrapIdx(writePos - intDel);
    int i2 = wrapIdx(writePos - intDel - 1);
    int i3 = wrapIdx(writePos - intDel - 2);

    float y0 = buf[static_cast<size_t>(i0)];
    float y1 = buf[static_cast<size_t>(i1)];
    float y2 = buf[static_cast<size_t>(i2)];
    float y3 = buf[static_cast<size_t>(i3)];

    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

float TapeDelay::tapeSaturation (float x) const
{
    // Soft magnetic tape saturation with unity gain at low amplitudes
    return std::tanh(x * 1.15f) * 0.87f;
}

//==============================================================================
void TapeDelay::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (m_bufSize <= 0 || m_bufL.empty() || m_bufR.empty() || numSamples <= 0)
        return;

    auto* dataL = buffer.getWritePointer(0);
    auto* dataR = numChannels > 1 ? buffer.getWritePointer(1) : dataL;

    // Fast bypass if disabled, or mix is zero with no feedback residual
    if (m_params.bypass || (m_params.mix < 0.001f && std::abs(m_fbDampL) < 0.0001f && std::abs(m_fbDampR) < 0.0001f))
    {
        // Keep write pointer moving with dry input so enabling is seamless
        for (int i = 0; i < numSamples; ++i)
        {
            m_bufL[static_cast<size_t>(m_writeL)] = dataL[i];
            m_bufR[static_cast<size_t>(m_writeR)] = dataR[i];
            m_writeL = (m_writeL + 1) % m_bufSize;
            m_writeR = (m_writeR + 1) % m_bufSize;
        }
        return;
    }

    // Target delay in samples
    float targetDelay = static_cast<float>(m_params.delayTimeMs * 0.001 * m_sampleRate);
    targetDelay = juce::jlimit(4.f, static_cast<float>(m_bufSize - 8), targetDelay);

    // Feedback progressive HF damping
    float dampFreq = juce::jlimit(600.f, 19000.f, 19000.f * (1.0f - m_params.hfDamping * 0.85f));
    float dampCoeff = std::exp(-juce::MathConstants<float>::twoPi * dampFreq / static_cast<float>(m_sampleRate));

    // LFO phase increments
    float wowInc      = juce::MathConstants<float>::twoPi * 0.5f  / static_cast<float>(m_sampleRate); // 0.5 Hz
    float flutterInc1 = juce::MathConstants<float>::twoPi * 4.2f  / static_cast<float>(m_sampleRate); // 4.2 Hz
    float flutterInc2 = juce::MathConstants<float>::twoPi * 7.1f  / static_cast<float>(m_sampleRate); // 7.1 Hz

    float mix = juce::jlimit(0.f, 1.f, m_params.mix);
    float wetGain = mix;

    for (int i = 0; i < numSamples; ++i)
    {
        float dryL = dataL[i];
        float dryR = dataR[i];

        // 1. Smooth target delay parameter tracking (click-free)
        m_smoothDelayL += 0.002f * (targetDelay - m_smoothDelayL);
        m_smoothDelayR += 0.002f * (targetDelay - m_smoothDelayR);

        // 2. Dual-LFO smooth tape wow & flutter modulation
        m_wowPhase += wowInc;
        if (m_wowPhase >= juce::MathConstants<float>::twoPi)
            m_wowPhase -= juce::MathConstants<float>::twoPi;

        m_flutterPhase1 += flutterInc1;
        if (m_flutterPhase1 >= juce::MathConstants<float>::twoPi)
            m_flutterPhase1 -= juce::MathConstants<float>::twoPi;

        m_flutterPhase2 += flutterInc2;
        if (m_flutterPhase2 >= juce::MathConstants<float>::twoPi)
            m_flutterPhase2 -= juce::MathConstants<float>::twoPi;

        float wow = std::sin(m_wowPhase) * m_params.wow * 0.015f * m_smoothDelayL;
        float flutter = (std::sin(m_flutterPhase1) * 0.7f + std::sin(m_flutterPhase2) * 0.3f)
                        * m_params.flutter * 0.004f * m_smoothDelayL;

        float delayL = juce::jlimit(4.f, static_cast<float>(m_bufSize - 8), m_smoothDelayL + wow + flutter);
        float delayR = juce::jlimit(4.f, static_cast<float>(m_bufSize - 8), m_smoothDelayR + wow - flutter);

        // 3. Read Hermite cubic interpolated delayed signal (noise-free)
        float wetL = readHermite(m_bufL, delayL, m_writeL);
        float wetR = readHermite(m_bufR, delayR, m_writeR);

        // 4. Feedback HF damping
        m_fbDampL = (1.0f - dampCoeff) * wetL + dampCoeff * m_fbDampL;
        m_fbDampR = (1.0f - dampCoeff) * wetR + dampCoeff * m_fbDampR;

        float fbL = tapeSaturation(m_fbDampL * m_params.feedback);
        float fbR = tapeSaturation(m_fbDampR * m_params.feedback);

        // 5. Write to delay buffer
        if (m_params.pingPong)
        {
            m_bufL[static_cast<size_t>(m_writeL)] = dryL + fbR;
            m_bufR[static_cast<size_t>(m_writeR)] = dryR + fbL;
        }
        else
        {
            m_bufL[static_cast<size_t>(m_writeL)] = dryL + fbL;
            m_bufR[static_cast<size_t>(m_writeR)] = dryR + fbR;
        }

        m_writeL = (m_writeL + 1) % m_bufSize;
        m_writeR = (m_writeR + 1) % m_bufSize;

        // 6. Output parallel dry + wet mix (constant dry level)
        dataL[i] = dryL + wetL * wetGain;
        dataR[i] = dryR + wetR * wetGain;
    }
}
