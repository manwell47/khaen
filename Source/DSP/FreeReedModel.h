#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <algorithm>

//==============================================================================
/**
 * FreeReedModel — Authentic Master Khaen Physical-Spectral Free-Reed Synthesis Engine
 *
 * Core Modus Operandi & Calibrated 0:26–0:45 Reference Profile:
 *  1. Exact Phase-Accurate Free-Reed Multi-Harmonic Oscillator (100% pitch accurate across C1-C8)
 *  2. Continuous Dual-Action Respiratory Engine (Living Modus Operandi):
 *     - EXHALE (Blow / Soplo):
 *       * Bright, singing, forward acoustic projection
 *       * Bamboo chimney radiation formant (1.4 - 2.8 kHz) + upper harmonic sheen
 *     - INHALE (Draw / Succión / Aspiración):
 *       * Intense suction chiff & air friction whoosh (2.5 - 6.5 kHz colored turbulence)
 *       * Deep windchest (Tao) chamber resonance (580 - 720 Hz, +6.5 dB)
 *       * Bronze reed suction bite & initial transient pitch scoop (-18 cents resolving in ~20ms)
 *  3. Automatic Rhythmic Riff & Staccato Stroke Alternation:
 *     - Rapid notes, riffs, and bass strikes dynamically pump breath strokes
 *  4. Continuous Respiratory Diaphragm Swell on Sustained Chords:
 *     - Sustained notes smoothly breathe across Exhale/Inhale cycles with organic acoustic morphing
 *  5. Asymmetric Hyperbolic Bernoulli Slit Flow & Smooth Aperture Saturation
 *  6. Linearized Polyphonic Energy Distribution & Clean Headroom Staging
 */
class FreeReedModel
{
public:
    //==========================================================================
    enum class Material { Bamboo, AgedBamboo, Jade, Brass, PVC, DarkWood, Metal };
    enum class BreathDirection { Exhale, Inhale };
    enum class Articulation { Legato = 0, Portato = 1, Staccato = 2, SlapTongue = 3 };

    //==========================================================================
    struct Parameters
    {
        float reedStiffness   = 0.65f;   // [0..1] — higher = brighter bronze reed buzz
        float boreDiameterMm  = 14.0f;   // mm — acoustic impedance and resonance
        float blowPressure    = 0.60f;   // [0..1] — breath intensity
        float lipDamping      = 0.30f;   // [0..1] — embouchure tightness
        float boreReflection  = 0.97f;   // [0..1] — bore wall reflection coefficient
        float breathSpeedHz   = 1.20f;   // [0.2..3.0 Hz] — continuous respiratory cycle speed
        float suctionBite     = 0.65f;   // [0..1] — intensity of inhale suction chiff, scoop & chest boom
        Material material     = Material::Bamboo;
        bool  autoInhaleRiff  = true;    // Auto-alternate breath direction on riffs & pulses

        // Hunt-Crossley Articulation & Vocal Tract Parameters (Chatziioannou et al. 2019, Pàmies-Vilà et al. 2020)
        Articulation articulation = Articulation::Legato;
        float tongueContactMs = 35.0f;   // [10.0..100.0 ms] duration of tongue damping contact
        float vocalTract      = 0.50f;   // [0..1] oral cavity impedance tuning / vowel formants [u -> o -> a -> e -> i]
        float velocitySens    = 0.80f;   // [0..1] dynamic velocity sensitivity curve
    };

    //==========================================================================
    FreeReedModel();
    ~FreeReedModel() = default;

    void prepare (double sampleRate, int blockSize);
    void reset();

    /** Set physical model parameters */
    void setParameters (const Parameters& p);

    /** Set pitch (MIDI note number, supports fractional for MPE pitch bend) */
    void setPitch (float midiNote);

    /** Set breath pressure from MPE channel pressure / velocity [0..1] */
    void setBreathPressure (float pressure);

    /** Set lip control from MPE slide (Y-axis) [0..1] */
    void setLipControl (float slide);

    /** Render one sample of audio */
    float processSample();

    /** True once the model is actively producing sound */
    bool isActive() const noexcept { return m_active; }
    bool isInhale() const noexcept { return m_inhaleBlend >= 0.5f; }
    float getInhaleBlend() const noexcept { return m_inhaleBlend; }

    void noteOn (float midiNote = 60.f, float velocity = 0.8f);
    void noteOff (float releaseTime = 0.10f);
    void retriggerBreathPulse (float velocity, BreathDirection dir);

    // Global shared respiratory state for polyphonic coherence & HUD reporting
    static float getGlobalBreathPhase() noexcept { return s_globalBreathPhase; }
    static bool isGlobalInhale() noexcept { return s_globalBreathPhase >= juce::MathConstants<float>::pi * 0.5f && s_globalBreathPhase <= juce::MathConstants<float>::pi * 1.5f; }
    static void setSustainedBassNote (bool hasBass) noexcept { s_hasSustainedBassNote = hasBass; }
    static bool hasSustainedBassNote() noexcept { return s_hasSustainedBassNote; }
    static void setBaseOctaveNote (int baseOct) noexcept { s_baseOctaveNote = baseOct; }
    static int  getBaseOctaveNote() noexcept { return s_baseOctaveNote; }

private:
    //==========================================================================
    struct DCBlocker
    {
        float x1 = 0.f, y1 = 0.f;
        float process (float x)
        {
            float y = x - x1 + 0.995f * y1;
            x1 = x; y1 = y;
            return y;
        }
        void reset() { x1 = y1 = 0.f; }
    };

    //==========================================================================
    // 2nd-order Parametric Peaking EQ Filter for Acoustic Formant Resonances
    struct PeakFilter
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f;
        float a1 = 0.f, a2 = 0.f;
        float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;

        void setup (float freqHz, float q, float gainDb, float sampleRate)
        {
            freqHz = juce::jlimit(20.f, sampleRate * 0.45f, freqHz);
            q = std::max(0.1f, q);

            float A = std::pow(10.f, gainDb / 40.f);
            float w0 = juce::MathConstants<float>::twoPi * freqHz / sampleRate;
            float alpha = std::sin(w0) / (2.f * q);
            float cosw0 = std::cos(w0);

            float a0 = 1.f + alpha / A;
            b0 = (1.f + alpha * A) / a0;
            b1 = (-2.f * cosw0) / a0;
            b2 = (1.f - alpha * A) / a0;
            a1 = (-2.f * cosw0) / a0;
            a2 = (1.f - alpha / A) / a0;
        }

        float process (float x)
        {
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }

        void reset() { x1 = x2 = y1 = y2 = 0.f; }
    };

    // 2nd-order Bandpass Filter for Acoustic Air Friction & Suction Chiff
    struct BandpassFilter
    {
        float b0 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;

        void setup (float centerHz, float q, float sampleRate)
        {
            centerHz = juce::jlimit(50.f, sampleRate * 0.45f, centerHz);
            q = std::max(0.1f, q);
            float w0 = juce::MathConstants<float>::twoPi * centerHz / sampleRate;
            float alpha = std::sin(w0) / (2.f * q);
            float cosw0 = std::cos(w0);

            float a0 = 1.f + alpha;
            b0 = alpha / a0;
            b2 = -alpha / a0;
            a1 = (-2.f * cosw0) / a0;
            a2 = (1.f - alpha) / a0;
        }

        float process (float x)
        {
            float y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }

        void reset() { x1 = x2 = y1 = y2 = 0.f; }
    };

    /** Update body formants according to chosen Material & continuous Inhale/Exhale blend */
    void updateFormants();

    //==========================================================================
    // Acoustic Body Formants & Filters
    PeakFilter     m_formantChest;      // F1: Windchest Tao cavity
    PeakFilter     m_formantPipe;       // F2: Bamboo chimney column
    PeakFilter     m_formantReed;       // F3: Bronze reed buzz
    PeakFilter     m_formantVocalTract; // F_VT: Upstream Oral Cavity / Vocal Tract Resonator (Pàmies-Vilà et al. 2020)
    PeakFilter     m_sympatheticTao;    // Sympathetic wooden windchest cavity body resonance
    BandpassFilter m_chiffFilter;       // Suction chiff air friction (2.5 - 6.5 kHz)
    DCBlocker      m_dcBlocker;

    // Global shared respiratory state
    static inline float s_globalBreathPhase = 0.f;
    static inline bool  s_riffAlternate = false;
    static inline bool  s_hasSustainedBassNote = false;
    static inline int   s_baseOctaveNote = 48; // Base MIDI note of active 2-octave playing window
    static inline uint32_t s_lastTriggerTime = 0;

    // Continuous Respiratory Breath Engine state
    float m_respiratoryPhase = 0.f;
    float m_inhaleBlend      = 0.f;   // [0.0 = pure Exhale .. 1.0 = pure Inhale]
    float m_suctionTransient = 0.f;   // Decaying suction pitch scoop [1.0 -> 0.0]
    float m_suctionDecayRate = 0.005f;

    // Hunt-Crossley Nonlinear Collision & Articulation Damping (Chatziioannou et al. 2019)
    float m_tongueContactTimer     = 0.f;   // Samples remaining of tongue contact
    float m_articulationTimer      = -1.f;  // Note auto-choke duration timer in samples (Staccato/SlapTongue)
    float m_articulationTotalSamples= 0.f;
    float m_portatoDip             = 0.f;   // Portato initial re-articulation pressure dip
    float m_effectiveDamping       = 1.0f;  // Dynamic damping modifier gamma(t)
    float m_effectiveStiffness     = 1.0f;  // Dynamic stiffness modifier k(t)
    float m_slapPrePressure        = 0.f;   // Pre-pressurized chamber level for slap-tongue
    float m_tongueForce            = 0.f;   // Hunt-Crossley dissipative contact force

    // Mechanical Transient (Bamboo fingerhole pop & bronze reed inertia flick)
    float m_mechanicalPopLevel = 0.f;
    float m_mechanicalPopDecay = 0.015f;
    float m_mechanicalPopPhase = 0.f;

    // Micro-Aeroelastic Perturbation (Period Jitter & Amplitude Shimmer)
    float m_jitterFiltered   = 0.f;
    float m_shimmerFiltered  = 1.f;
    float m_reedEigenPhase1  = 0.f;   // Bronze cantilever 2nd mechanical mode (~6.27 f0)
    float m_reedEigenPhase2  = 0.f;   // Bronze cantilever 3rd mechanical mode (~17.5 f0)

    // Fast XorShift PRNG for audio-rate micro-chaos without thread contention
    uint32_t m_rngState = 0x853c49e6;
    inline float nextRandomFloat() noexcept
    {
        m_rngState ^= (m_rngState << 13);
        m_rngState ^= (m_rngState >> 17);
        m_rngState ^= (m_rngState << 5);
        return (static_cast<float>(m_rngState & 0x00ffffff) / 8388608.0f) - 1.0f; // [-1.0 .. +1.0]
    }

    // Pink / turbulent noise generator state
    float m_noiseB0 = 0.f, m_noiseB1 = 0.f, m_noiseB2 = 0.f;

    // Phase Accumulator & Oscillator state
    float m_phase          = 0.f;
    float m_phaseIncrement = 0.f;
    float m_reedFreq       = 440.f;
    float m_baseMidiNote   = 60.f;
    float m_lastMidiNote   = -1.f;

    // Breath & Acoustic dynamics
    float m_breathLfoPhase = 0.f;
    float m_breathJitter   = 0.f;
    float m_targetPressure = 0.7f;
    float m_pressureSmooth = 0.f;
    float m_lipControl     = 0.f;

    // Model parameters & sample rate
    Parameters m_params;
    float m_sampleRate     = 44100.f;
    int   m_formantUpdateCounter = 0;

    // Envelope
    bool  m_active       = false;
    bool  m_releasing    = false;
    float m_amplitude    = 0.f;
    float m_noteVelocity = 0.8f;
    float m_attackRate   = 0.005f;
    float m_releaseRate  = 0.001f;
};
