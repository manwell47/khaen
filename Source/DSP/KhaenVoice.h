#pragma once
#include <JuceHeader.h>
#include "FreeReedModel.h"

//==============================================================================
/**
 * KhaenVoice — MPE Synthesiser Voice
 *
 * Wraps FreeReedModel into a juce::MPESynthesiserVoice.
 * Handles per-note MPE expression:
 *   - Pressure (Z-axis)  → breath intensity
 *   - Slide (Y-axis)     → lip control / timbre
 *   - Pitch Bend (X-axis)→ micro-tonal glissando
 */
class KhaenVoice : public juce::MPESynthesiserVoice
{
public:
    KhaenVoice();
    ~KhaenVoice() override = default;

    //==========================================================================
    // juce::MPESynthesiserVoice overrides
    void noteStarted()   override;
    void noteStopped (bool allowTailOff) override;
    void notePressureChanged()  override;
    void notePitchbendChanged() override;
    void noteTimbreChanged()    override;
    void noteKeyStateChanged()  override;

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample,
                          int numSamples) override;

    //==========================================================================
    void prepare (double sampleRate, int blockSize);

    /** Apply model parameters (called when user changes knobs or presets) */
    void setModelParameters (const FreeReedModel::Parameters& params);

    /** Retrigger breath pulse across active voice */
    void retriggerBreathPulse (float velocity, FreeReedModel::BreathDirection dir);

    bool isVoiceActive() const noexcept { return m_model.isActive(); }

private:
    FreeReedModel m_model;
    FreeReedModel::Parameters m_params;

    double m_sampleRate = 44100.0;
    int    m_blockSize  = 512;

    // Smooth MPE transitions to avoid zipper noise
    juce::SmoothedValue<float> m_pressureSmooth  { 0.f };
    juce::SmoothedValue<float> m_pitchBendSmooth { 0.f };
    juce::SmoothedValue<float> m_timbreSmooth    { 0.f };
    float m_noteVelocity { 0.8f };

    static constexpr float kSmoothingMs = 10.f; // 10ms ramp time
};
