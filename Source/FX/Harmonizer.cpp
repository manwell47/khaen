#include "Harmonizer.h"
#include <algorithm>

//==============================================================================
Harmonizer::Harmonizer()
{
    m_bufL.assign(kMaxBufSize, 0.f);
    m_bufR.assign(kMaxBufSize, 0.f);
    reset();
}

void Harmonizer::prepare (double sampleRate, int /*blockSize*/)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_bufL.assign(kMaxBufSize, 0.f);
    m_bufR.assign(kMaxBufSize, 0.f);
    reset();
}

void Harmonizer::reset()
{
    if (!m_bufL.empty()) std::fill(m_bufL.begin(), m_bufL.end(), 0.f);
    if (!m_bufR.empty()) std::fill(m_bufR.begin(), m_bufR.end(), 0.f);
    m_writePos = 0;
    m_phaseL   = 0.f;
    m_phaseR   = 0.f;
}

void Harmonizer::setParameters (const Parameters& p)
{
    m_params = p;
}

//==============================================================================
float Harmonizer::readHermite (const std::vector<float>& buf,
                               float delaySamples,
                               int writePos) const
{
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

//==============================================================================
void Harmonizer::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || m_bufL.empty() || m_bufR.empty())
        return;

    auto* dataL = buffer.getWritePointer(0);
    auto* dataR = numChannels > 1 ? buffer.getWritePointer(1) : dataL;

    // Fast bypass if disabled, dry, or no pitch shift configured
    bool hasPitchShift = (std::abs(m_params.shiftSemitones) >= 0.05f 
                       || std::abs(m_params.detuneCents) >= 0.5f 
                       || m_params.stereoSpread >= 0.05f);

    if (m_params.bypass || m_params.mix < 0.001f || !hasPitchShift)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            m_bufL[static_cast<size_t>(m_writePos)] = dataL[i];
            m_bufR[static_cast<size_t>(m_writePos)] = dataR[i];
            m_writePos = (m_writePos + 1) % m_bufSize;
        }
        return;
    }

    // Pitch ratio calculation
    float detuneSpread = m_params.stereoSpread * 8.0f; // subtle spread in cents
    float shiftL = m_params.shiftSemitones + (m_params.detuneCents - detuneSpread) / 100.0f;
    float shiftR = m_params.shiftSemitones + (m_params.detuneCents + detuneSpread) / 100.0f;

    float ratioL = std::pow(2.f, shiftL / 12.f);
    float ratioR = std::pow(2.f, shiftR / 12.f);

    float deltaL = (1.0f - ratioL) / static_cast<float>(kWindowSize);
    float deltaR = (1.0f - ratioR) / static_cast<float>(kWindowSize);

    float mix     = juce::jlimit(0.f, 1.f, m_params.mix);
    float wetGain = mix;

    for (int i = 0; i < numSamples; ++i)
    {
        float dryL = dataL[i];
        float dryR = dataR[i];

        m_bufL[static_cast<size_t>(m_writePos)] = dryL;
        m_bufR[static_cast<size_t>(m_writePos)] = dryR;

        // Left Channel dual-tap calculation
        float pL1 = m_phaseL - std::floor(m_phaseL);
        float pL2 = (m_phaseL + 0.5f) - std::floor(m_phaseL + 0.5f);

        float dL1 = pL1 * static_cast<float>(kWindowSize) + 4.f;
        float dL2 = pL2 * static_cast<float>(kWindowSize) + 4.f;

        float wL1 = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * pL1));
        float wL2 = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * pL2));

        float sL1 = readHermite(m_bufL, dL1, m_writePos);
        float sL2 = readHermite(m_bufL, dL2, m_writePos);
        float wetL = sL1 * wL1 + sL2 * wL2;

        // Right Channel dual-tap calculation
        float pR1 = m_phaseR - std::floor(m_phaseR);
        float pR2 = (m_phaseR + 0.5f) - std::floor(m_phaseR + 0.5f);

        float dR1 = pR1 * static_cast<float>(kWindowSize) + 4.f;
        float dR2 = pR2 * static_cast<float>(kWindowSize) + 4.f;

        float wR1 = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * pR1));
        float wR2 = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * pR2));

        float sR1 = readHermite(m_bufR, dR1, m_writePos);
        float sR2 = readHermite(m_bufR, dR2, m_writePos);
        float wetR = sR1 * wR1 + sR2 * wR2;

        // Output mix (preserve full dry + add harmonized wet voices)
        dataL[i] = dryL + wetL * wetGain;
        dataR[i] = dryR + wetR * wetGain;

        m_phaseL += deltaL;
        if (m_phaseL >= 1.0f) m_phaseL -= 1.0f;
        else if (m_phaseL < 0.0f) m_phaseL += 1.0f;

        m_phaseR += deltaR;
        if (m_phaseR >= 1.0f) m_phaseR -= 1.0f;
        else if (m_phaseR < 0.0f) m_phaseR += 1.0f;

        m_writePos = (m_writePos + 1) % m_bufSize;
    }
}
