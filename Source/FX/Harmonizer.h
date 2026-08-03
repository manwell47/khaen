#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

//==============================================================================
/**
 * Harmonizer — Studio Dual-Tap Crossfading Stereo Pitch Shifter
 *
 * Features:
 *  - Continuous phase-locked dual-tap crossfading pitch modulation
 *  - 4-point Hermite cubic interpolation for silky clean pitch shifting
 *  - Raised-cosine (Hann) complementary windowing with constant unity gain (W1 + W2 = 1.0)
 *  - Shift range: [-12..+12] semitones
 *  - Fine detune: [-50..+50] cents
 *  - Stereo spread (subtle anti-phase detune for wide, warm stereo widening)
 *  - Dedicated Bypass mode with clean dry pass-through
 */
class Harmonizer
{
public:
    struct Parameters
    {
        float shiftSemitones = 7.f;    // [-12..+12] semitones
        float detuneCents    = 0.f;    // [-50..+50] cents
        float stereoSpread   = 0.3f;   // [0..1]
        float mix            = 0.0f;   // [0..1] dry/wet
        bool  bypass         = true;   // true = bypassed by default
    };

    Harmonizer();
    ~Harmonizer() = default;

    void prepare (double sampleRate, int blockSize);
    void reset();
    void setParameters (const Parameters& p);

    void process (juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kMaxBufSize = 8192;
    static constexpr int kWindowSize = 2048;

    float readHermite (const std::vector<float>& buf, float delaySamples, int writePos) const;

    std::vector<float> m_bufL, m_bufR;
    int   m_writePos = 0;
    int   m_bufSize  = kMaxBufSize;

    float m_phaseL = 0.f;
    float m_phaseR = 0.f;

    double m_sampleRate = 44100.0;
    Parameters m_params;
};
