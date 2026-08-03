#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

//==============================================================================
/**
 * GranularCrystallizer — Creative Granular FX Module
 *
 * Three selectable modes inspired by the Eventide Crystallizer:
 *
 *  Mode 0 — Reverse Granular Echo
 *    Captures grains (80–200ms), reverses them, re-injects with delay.
 *
 *  Mode 1 — Pitch-Shifted Feedback
 *    Each echo repeat shifts pitch up/down by a configurable interval
 *    (octave, fifth, etc.), creating cascading harmonic clouds.
 *
 *  Mode 2 — Glitched Freeze
 *    Captures a micro-loop (20–500ms) and sustains it frozen in time.
 *    A gate trigger controls freeze on/off.
 */
class GranularCrystallizer
{
public:
    enum class Mode { ReverseEcho = 0, PitchFeedback, GlitchedFreeze };

    struct Parameters
    {
        Mode  mode          = Mode::ReverseEcho;
        float grainSizeMs   = 120.f;   // [20..500] ms — grain capture length
        float delayMs       = 350.f;   // [10..1000] ms — echo delay time
        float feedback      = 0.50f;   // [0..0.90]
        float pitchInterval = 12.f;    // semitones [−24..+24] (for PitchFeedback)
        float mix           = 0.45f;   // [0..1]
        bool  freezeGate    = false;   // Freeze trigger (Mode 2)
        float loopSizeMs    = 80.f;    // [20..500] ms loop size (Mode 2)
        bool  bypass        = true;    // true = bypassed by default
    };

    GranularCrystallizer();
    ~GranularCrystallizer() = default;

    void prepare (double sampleRate, int blockSize);
    void reset();
    void setParameters (const Parameters& p);
    void triggerFreeze (bool on);   // external freeze trigger

    void process (juce::AudioBuffer<float>& buffer);

private:
    //==========================================================================
    static constexpr int kMaxBufSec = 4;

    // Circular capture buffer (stereo)
    std::vector<float> m_capBufL, m_capBufR;
    int   m_capWritePos  = 0;
    int   m_capBufSize   = 0;

    // Frozen loop buffer
    std::vector<float> m_frozenL, m_frozenR;
    int   m_frozenSize   = 0;
    float m_frozenReadPos = 0.f;
    bool  m_isFrozen     = false;

    // Delay feedback buffer
    std::vector<float> m_delBufL, m_delBufR;
    int   m_delWritePos  = 0;
    int   m_delBufSize   = 0;

    // Internal state per mode
    float m_grainPhase       = 0.f;
    int   m_grainReadCursor  = 0;
    bool  m_grainReversing   = false;

    // 1-pole low-pass for feedback smoothing
    float m_fbLPL = 0.f, m_fbLPR = 0.f;

    double m_sampleRate = 44100.0;
    Parameters m_params;

    //==========================================================================
    void processModeReverseEcho     (float* L, float* R, int n);
    void processModePitchFeedback   (float* L, float* R, int n);
    void processModeGlitchedFreeze  (float* L, float* R, int n);

    float pitchShift (const std::vector<float>& buf, int bufSize,
                      int readBase, float pitchRatio, float phase) const;

    float readCircular (const std::vector<float>& buf, int bufSize,
                        int writePos, float delaySamples) const;
};
