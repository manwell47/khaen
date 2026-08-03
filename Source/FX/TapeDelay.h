#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

//==============================================================================
/**
 * TapeDelay — Professional Analog Tape Delay with Dual-LFO Modulation & Hermite Interpolation
 *
 * Features:
 *  - 4-point Hermite cubic interpolation (zero zipper noise or interpolation distortion)
 *  - Dual smooth sinusoidal LFOs: Wow (0.5 Hz) and Flutter (4.2 Hz + 7.1 Hz)
 *  - Parameter smoothing to eliminate clicks during time changes
 *  - 1-pole progressive high-frequency damping simulating magnetic tape loss
 *  - Soft magnetic saturation on feedback path
 *  - Ping-pong stereo mode
 *  - Clean bypass when dry
 */
class TapeDelay
{
public:
    struct Parameters
    {
        float delayTimeMs  = 400.f;   // [10..2000] ms
        float feedback     = 0.35f;   // [0..0.95]
        float wow          = 0.15f;   // [0..1] — slow LFO depth
        float flutter      = 0.08f;   // [0..1] — fast flutter depth
        float mix          = 0.0f;    // [0..1] dry/wet (0 = dry by default)
        bool  pingPong     = false;
        float hfDamping    = 0.65f;   // [0..1] — tape high-freq rolloff
        bool  bypass       = true;    // true = bypassed by default
    };

    TapeDelay();
    ~TapeDelay() = default;

    void prepare (double sampleRate, int blockSize);
    void reset();
    void setParameters (const Parameters& p);

    void process (juce::AudioBuffer<float>& buffer);

private:
    static constexpr float kMaxDelaySeconds = 2.0f;

    float readHermite (const std::vector<float>& buf, float delaySamples, int writePos) const;
    float tapeSaturation (float x) const;

    std::vector<float> m_bufL, m_bufR;
    int   m_writeL = 0, m_writeR = 0;
    int   m_bufSize = 0;

    // Smoothed delay times
    float m_smoothDelayL = 0.f;
    float m_smoothDelayR = 0.f;

    // Dual-sine Wow and Flutter LFOs
    float m_wowPhase       = 0.f;
    float m_flutterPhase1  = 0.f;
    float m_flutterPhase2  = 0.f;

    // Feedback damping (1-pole LP)
    float m_fbDampL = 0.f, m_fbDampR = 0.f;

    double m_sampleRate = 44100.0;
    Parameters m_params;
};
