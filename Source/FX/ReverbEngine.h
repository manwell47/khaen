#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

//==============================================================================
/**
 * ReverbEngine — Authentic Acoustic Space Reverb with Laos Location Presets
 *
 * Implements a high-density modulated 8-comb stereo diffuser network with
 * early reflection taps, all-pass spatializers, frequency-dependent damping,
 * and stereo width spread.
 *
 * Includes 5 Laos Location Presets:
 *  0: Wat Phu (Ancient Temple)      — Champasak sandstone temple, RT60 ~2.8s
 *  1: Luang Prabang Sanctuary       — Historic teakwood meditation hall, RT60 ~1.8s
 *  2: Tham Kong Lor (Giant Cave)    — Subterranean limestone river cavern, RT60 ~5.5s
 *  3: That Luang Pavilion           — Golden stupa open ceremonial courtyard, RT60 ~1.4s
 *  4: Mekong River Grotto           — High river alcove with mist & lush diffusion, RT60 ~3.2s
 */
class ReverbEngine
{
public:
    enum class LaosPreset
    {
        WatPhu = 0,
        LuangPrabang,
        ThamKongLor,
        ThatLuang,
        MekongGrotto,
        Manual
    };

    struct Parameters
    {
        float size     = 0.70f;  // [0.1 .. 1.0] — space geometry / room size
        float decay    = 0.70f;  // [0.1 .. 0.98] — RT60 decay time
        float damp     = 0.40f;  // [0.0 .. 1.0] — high frequency air absorption
        float width    = 0.85f;  // [0.0 .. 1.0] — stereo spread
        float mix      = 0.00f;  // [0.0 .. 1.0] — dry/wet
        int   preset   = 0;      // 0..4 (Laos locations)
        bool  bypass   = false;  // true = dry pass-through
    };

    ReverbEngine();
    ~ReverbEngine() = default;

    void prepare (double sampleRate, int blockSize);
    void reset();
    void setParameters (const Parameters& p);

    /** Apply a Laos Location Preset to the parameters struct */
    static void applyLaosPreset (int presetIndex, Parameters& p);
    static juce::String getLaosPresetName (int index);

    void process (juce::AudioBuffer<float>& buffer);

private:
    // Comb filter with 1-pole low-pass feedback & LFO modulation
    struct ModulatedComb
    {
        std::vector<float> buffer;
        int   writePos = 0;
        float delayLen = 1000.f;
        float filterState = 0.f;
        float lfoPhase = 0.f;
        float lfoRate = 0.5f;
        float lfoDepth = 8.f;

        void prepare (int maxDelaySamples, float baseDelay, float rateHz, float depthSamples)
        {
            buffer.assign(maxDelaySamples + 32, 0.f);
            writePos = 0;
            delayLen = baseDelay;
            filterState = 0.f;
            lfoRate = rateHz;
            lfoDepth = depthSamples;
        }

        void reset()
        {
            std::fill(buffer.begin(), buffer.end(), 0.f);
            writePos = 0;
            filterState = 0.f;
        }

        inline float process (float input, float feedback, float damp, float sampleRate)
        {
            lfoPhase += juce::MathConstants<float>::twoPi * lfoRate / sampleRate;
            if (lfoPhase >= juce::MathConstants<float>::twoPi)
                lfoPhase -= juce::MathConstants<float>::twoPi;

            float modDelay = delayLen + lfoDepth * std::sin(lfoPhase);
            float readPos = static_cast<float>(writePos) - modDelay;
            int bufSize = static_cast<int>(buffer.size());
            while (readPos < 0.f) readPos += bufSize;

            int r0 = static_cast<int>(readPos) % bufSize;
            int r1 = (r0 + 1) % bufSize;
            float frac = readPos - std::floor(readPos);
            float delayed = (1.f - frac) * buffer[r0] + frac * buffer[r1];

            // 1-pole low-pass damping
            filterState = delayed * (1.f - damp) + filterState * damp;
            buffer[writePos] = input + filterState * feedback;

            writePos = (writePos + 1) % bufSize;
            return delayed;
        }
    };

    // All-pass diffuser stage
    struct AllPass
    {
        std::vector<float> buffer;
        int writePos = 0;
        int delayLen = 500;
        float feedback = 0.5f;

        void prepare (int delaySamples, float fb = 0.5f)
        {
            buffer.assign(std::max(16, delaySamples + 1), 0.f);
            writePos = 0;
            delayLen = delaySamples;
            feedback = fb;
        }

        void reset()
        {
            std::fill(buffer.begin(), buffer.end(), 0.f);
            writePos = 0;
        }

        inline float process (float input)
        {
            int bufSize = static_cast<int>(buffer.size());
            int readPos = (writePos - delayLen + bufSize) % bufSize;
            float bufOut = buffer[readPos];
            float v = input + bufOut * feedback;
            buffer[writePos] = v;
            writePos = (writePos + 1) % bufSize;
            return bufOut - input * feedback;
        }
    };

    Parameters m_params;
    double m_sampleRate = 44100.0;

    // 8 Modulated Comb Filters (4 Left, 4 Right)
    std::array<ModulatedComb, 4> m_combsL;
    std::array<ModulatedComb, 4> m_combsR;

    // 4 Stereo All-Pass Diffusers (2 Left, 2 Right)
    std::array<AllPass, 2> m_allpassL;
    std::array<AllPass, 2> m_allpassR;

    // Smooth parameter transitions
    juce::SmoothedValue<float> m_smoothMix;
    juce::SmoothedValue<float> m_smoothDamp;
};
