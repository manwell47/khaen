#include "KhaenVoice.h"

//==============================================================================
KhaenVoice::KhaenVoice()
{
    m_pressureSmooth.reset(static_cast<int>(44100.0 * kSmoothingMs / 1000.f));
    m_pitchBendSmooth.reset(static_cast<int>(44100.0 * kSmoothingMs / 1000.f));
    m_timbreSmooth.reset(static_cast<int>(44100.0 * kSmoothingMs / 1000.f));
}

//==============================================================================
void KhaenVoice::prepare (double sampleRate, int blockSize)
{
    m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    m_blockSize  = blockSize;

    int smoothSamples = static_cast<int>(m_sampleRate * kSmoothingMs / 1000.f);
    m_pressureSmooth.reset(smoothSamples);
    m_pitchBendSmooth.reset(smoothSamples);
    m_timbreSmooth.reset(smoothSamples);

    m_model.prepare(m_sampleRate, blockSize);
}

//==============================================================================
void KhaenVoice::setModelParameters (const FreeReedModel::Parameters& params)
{
    m_params = params;
    m_model.setParameters(params);
}

//==============================================================================
void KhaenVoice::retriggerBreathPulse (float velocity, FreeReedModel::BreathDirection dir)
{
    if (m_model.isActive())
    {
        m_model.retriggerBreathPulse(velocity, dir);
        m_pressureSmooth.setCurrentAndTargetValue(std::min(1.4f, m_pressureSmooth.getCurrentValue() + 0.35f * velocity));
    }
}

//==============================================================================
void KhaenVoice::noteStarted()
{
    auto note = getCurrentlyPlayingNote();

    m_noteVelocity = note.noteOnVelocity.asUnsignedFloat();
    if (m_noteVelocity <= 0.01f)
        m_noteVelocity = 0.8f;

    // Base MIDI pitch
    float midiNote = static_cast<float>(note.initialNote);

    // MPE pitch bend
    float pitchBendSemitones = note.pitchbend.asSignedFloat() * 48.f;
    m_pitchBendSmooth.setCurrentAndTargetValue(pitchBendSemitones);

    // Pressure/Breath: In legacy MIDI / standard keyboard, pressure is 0, so use velocity!
    float pressure = note.pressure.asUnsignedFloat();
    float breath = (pressure > 0.01f) ? pressure : m_noteVelocity;
        
    // Silky smooth breath ramp on note onset
    m_pressureSmooth.setCurrentAndTargetValue(0.15f * breath);
    m_pressureSmooth.setTargetValue(breath);
    m_model.setBreathPressure(breath);

    // MPE timbre (slide/Y) -> lip control
    float timbre = note.timbre.asUnsignedFloat();
    m_timbreSmooth.setCurrentAndTargetValue(timbre);
    m_model.setLipControl(timbre);

    m_model.noteOn(midiNote, m_noteVelocity);
    m_model.setPitch(midiNote + pitchBendSemitones);
}

//==============================================================================
void KhaenVoice::noteStopped (bool allowTailOff)
{
    if (allowTailOff)
        m_model.noteOff(0.12f); // 120ms release
    else
        m_model.noteOff(0.005f); // 5ms quick release
}

//==============================================================================
void KhaenVoice::notePressureChanged()
{
    float pressure = getCurrentlyPlayingNote().pressure.asUnsignedFloat();
    float breath = (pressure > 0.01f) ? pressure : m_noteVelocity;
    m_pressureSmooth.setTargetValue(breath);
}

void KhaenVoice::notePitchbendChanged()
{
    auto note = getCurrentlyPlayingNote();
    float pitchBendSemitones = note.pitchbend.asSignedFloat() * 48.f;
    m_pitchBendSmooth.setTargetValue(pitchBendSemitones);
}

void KhaenVoice::noteTimbreChanged()
{
    float timbre = getCurrentlyPlayingNote().timbre.asUnsignedFloat();
    m_timbreSmooth.setTargetValue(timbre);
}

void KhaenVoice::noteKeyStateChanged() {}

//==============================================================================
void KhaenVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                   int startSample,
                                   int numSamples)
{
    if (!m_model.isActive())
        return;

    auto* leftChan  = outputBuffer.getWritePointer(0);
    auto* rightChan = outputBuffer.getNumChannels() > 1
                    ? outputBuffer.getWritePointer(1) : nullptr;

    auto note = getCurrentlyPlayingNote();
    float baseMidiNote = static_cast<float>(note.initialNote);

    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        float pressure  = m_pressureSmooth.getNextValue();
        float timbre    = m_timbreSmooth.getNextValue();
        float pitchBend = m_pitchBendSmooth.getNextValue();

        m_model.setPitch(baseMidiNote + pitchBend);
        m_model.setBreathPressure(pressure);
        m_model.setLipControl(timbre);

        float sample = m_model.processSample();

        leftChan[i] += sample;
        if (rightChan)
            rightChan[i] += sample;
    }

    if (!m_model.isActive())
        clearCurrentNote();
}
