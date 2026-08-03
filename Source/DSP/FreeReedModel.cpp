#include "FreeReedModel.h"

//==============================================================================
FreeReedModel::FreeReedModel()
{
    reset();
}

//==============================================================================
void FreeReedModel::prepare (double sampleRate, int /*blockSize*/)
{
    m_sampleRate = static_cast<float>(sampleRate > 0.0 ? sampleRate : 44100.0);

    // Fast, crisp attack (~4ms for authentic Isan articulation) and natural smooth release (~100ms)
    m_attackRate         = 1.0f / std::max(1.f, 0.004f * m_sampleRate);
    m_releaseRate        = 1.0f / std::max(1.f, 0.100f * m_sampleRate);
    m_suctionDecayRate   = 1.0f / std::max(1.f, 0.022f * m_sampleRate); // 22ms suction transient scoop
    m_mechanicalPopDecay = 1.0f / std::max(1.f, 0.014f * m_sampleRate); // ~14ms mechanical bamboo/reed pop

    m_chiffFilter.setup(3600.f, 2.2f, m_sampleRate);
    updateFormants();
    reset();
}

//==============================================================================
void FreeReedModel::reset()
{
    m_dcBlocker.reset();
    m_formantChest.reset();
    m_formantPipe.reset();
    m_formantReed.reset();
    m_sympatheticTao.reset();
    m_chiffFilter.reset();

    m_phase             = 0.f;
    m_phaseIncrement    = 0.f;
    m_breathLfoPhase    = 0.f;
    m_breathJitter      = 0.f;
    m_active            = false;
    m_releasing         = false;
    m_amplitude         = 0.f;
    m_pressureSmooth    = 0.f;
    m_targetPressure    = 0.7f;
    m_baseMidiNote      = 60.f;
    m_lastMidiNote      = -1.f;
    m_inhaleBlend       = 0.f;
    m_suctionTransient  = 0.f;
    m_formantUpdateCounter = 0;

    m_mechanicalPopLevel = 0.f;
    m_mechanicalPopPhase = 0.f;
    m_jitterFiltered     = 0.f;
    m_shimmerFiltered    = 1.f;
    m_reedEigenPhase1    = 0.f;
    m_reedEigenPhase2    = 0.f;

    m_noiseB0 = m_noiseB1 = m_noiseB2 = 0.f;
}

//==============================================================================
void FreeReedModel::setParameters (const Parameters& p)
{
    m_params = p;
    updateFormants();
}

//==============================================================================
void FreeReedModel::updateFormants()
{
    // Continuous dynamic blend: 0.0 = pure Exhale, 1.0 = pure Inhale
    float wInhale = m_inhaleBlend;
    float wExhale = 1.0f - wInhale;

    // Master Khaen Acoustic Profile:
    // Exhale emphasizes open singing bamboo chimney radiation (1.4 - 2.8 kHz);
    // Inhale emphasizes deep wood windchest (Tao) chamber resonance (580 - 650 Hz, +8.5 to +12.5 dB)
    // and sharp bronze reed tongue bite under negative suction pressure (3.8 - 4.2 kHz, +9.5 to +14 dB).
    float fChestExhale = 520.f,  fChestInhale = 620.f;
    float fPipeExhale  = 1450.f, fPipeInhale  = 1050.f;
    float fReedExhale  = 2850.f, fReedInhale  = 3850.f;

    float fChestHz = wExhale * fChestExhale + wInhale * fChestInhale;
    float fPipeHz  = wExhale * fPipeExhale  + wInhale * fPipeInhale;
    float fReedHz  = wExhale * fReedExhale  + wInhale * fReedInhale;

    float qChest   = wExhale * 2.4f + wInhale * (3.6f + m_params.suctionBite * 0.8f);
    float qPipe    = wExhale * 3.6f + wInhale * 2.4f;
    float qReed    = wExhale * 2.8f + wInhale * (4.2f + m_params.suctionBite * 0.8f);

    float gainChest = wExhale * 3.5f + wInhale * (8.5f + m_params.suctionBite * 4.0f);
    float gainPipe  = wExhale * 7.5f + wInhale * 2.5f;
    float gainReed  = wExhale * 5.0f + wInhale * (9.5f + m_params.suctionBite * 4.5f)
                      + m_params.reedStiffness * 3.0f;

    switch (m_params.material)
    {
        case Material::Bamboo:
            // Standard traditional bamboo calibration
            break;
        case Material::AgedBamboo:
            fChestHz -= 40.f; fPipeHz -= 80.f; fReedHz -= 150.f;
            qChest += 0.3f;
            break;
        case Material::Jade:
            fChestHz += 100.f; fPipeHz += 150.f; fReedHz += 300.f;
            gainReed += 1.5f;
            break;
        case Material::Brass:
            fChestHz += 60.f; fPipeHz += 100.f; fReedHz += 200.f;
            gainReed += 2.0f;
            break;
        case Material::PVC:
            fChestHz += 20.f; fPipeHz += 40.f; fReedHz -= 100.f;
            qPipe -= 0.5f;
            break;
        case Material::DarkWood:
            fChestHz -= 60.f; fPipeHz -= 100.f; fReedHz -= 200.f;
            break;
        case Material::Metal:
            fChestHz += 80.f; fPipeHz += 180.f; fReedHz += 350.f;
            gainReed += 2.5f;
            break;
    }

    // Bore diameter modulates the acoustic pipe cavity resonance
    float boreShift = (m_params.boreDiameterMm - 14.f) * 16.f;
    fPipeHz = std::max(200.f, fPipeHz - boreShift);

    // For lower notes (e.g. C2 = 65 Hz), scale pipe formant organically so it strictly harmonizes
    if (m_reedFreq > 20.f && m_reedFreq < 260.f)
    {
        float targetHarmonic = std::round(fPipeHz / m_reedFreq) * m_reedFreq;
        float blend = juce::jlimit(0.0f, 1.0f, (260.f - m_reedFreq) / 200.f);
        fPipeHz = (1.0f - blend) * fPipeHz + blend * targetHarmonic;
    }

    m_formantChest.setup(fChestHz, qChest, gainChest, m_sampleRate);
    m_formantPipe.setup(fPipeHz,   qPipe,  gainPipe,  m_sampleRate);
    m_formantReed.setup(fReedHz,   qReed,  gainReed,  m_sampleRate);

    // Sympathetic Tao wooden windchest chamber body resonance (320-380 Hz)
    float fTaoHz = 320.f + 50.f * wInhale;
    float gainTao = 3.2f + 2.0f * wInhale;
    m_sympatheticTao.setup(fTaoHz, 2.2f, gainTao, m_sampleRate);

    // Suction chiff filter center frequency shifts with suction intensity
    float chiffCenter = 3200.f + 1200.f * wInhale;
    m_chiffFilter.setup(chiffCenter, 2.0f, m_sampleRate);
}

//==============================================================================
void FreeReedModel::setPitch (float midiNote)
{
    m_baseMidiNote = midiNote;
    if (std::abs(midiNote - m_lastMidiNote) < 0.0005f)
        return;

    m_lastMidiNote = midiNote;

    // Base 12-TET frequency calculation
    float freq = 440.f * std::pow(2.f, (midiNote - 69.f) / 12.f);
    m_reedFreq = juce::jlimit(20.f, m_sampleRate * 0.46f, freq);
    m_phaseIncrement = juce::MathConstants<float>::twoPi * m_reedFreq / m_sampleRate;

    updateFormants();
}

//==============================================================================
void FreeReedModel::setBreathPressure (float pressure)
{
    // Linearized acoustic energy mapping: preserves loudness in chords
    float baseP = 0.60f + 0.40f * juce::jlimit(0.0f, 1.0f, pressure);
    float blowP = 0.60f + 0.40f * juce::jlimit(0.0f, 1.0f, m_params.blowPressure);
    m_targetPressure = baseP * blowP;
}

void FreeReedModel::setLipControl (float slide)
{
    m_lipControl = juce::jlimit(0.f, 1.f, slide);
}

//==============================================================================
void FreeReedModel::noteOn (float midiNote)
{
    m_active         = true;
    m_releasing      = false;
    m_amplitude      = 0.001f;
    m_pressureSmooth = 0.20f;
    m_baseMidiNote   = midiNote;
    m_dcBlocker.reset();

    // Trigger mechanical transient pop & reed snap
    m_mechanicalPopLevel = 0.65f * juce::jlimit(0.4f, 1.0f, m_params.blowPressure + 0.3f);
    m_mechanicalPopPhase = 0.f;

    // Rhythmic breath stroke sync & Bass note suction lock:
    // When playing or sustaining a bass note (<= 60 / C4), lock immediately into continuous Inhale (Succión).
    if (midiNote <= 60.5f || s_hasSustainedBassNote)
    {
        s_globalBreathPhase = juce::MathConstants<float>::pi; // Full Inhale (Succión)
        m_respiratoryPhase  = juce::MathConstants<float>::pi;
        m_suctionTransient  = 1.0f;
    }
    else if (m_params.autoInhaleRiff)
    {
        s_riffAlternate = !s_riffAlternate;
        if (s_riffAlternate)
            s_globalBreathPhase = juce::MathConstants<float>::pi * 1.10f; // Snap to Inhale stroke
        else
            s_globalBreathPhase = 0.10f; // Snap to Exhale stroke
        m_respiratoryPhase = s_globalBreathPhase;
    }

    m_inhaleBlend = 0.5f * (1.0f - std::cos(m_respiratoryPhase));

    // If in Inhale phase, trigger punchy suction transient scoop (-28 cents)
    if (m_inhaleBlend >= 0.30f)
        m_suctionTransient = 1.0f;
    else
        m_suctionTransient = 0.0f;

    setPitch(midiNote);
    updateFormants();
}

void FreeReedModel::noteOff (float releaseTime)
{
    m_releasing   = true;
    m_releaseRate = 1.0f / std::max(1.f, juce::jmax(0.01f, releaseTime) * m_sampleRate);
}

void FreeReedModel::retriggerBreathPulse (float velocity, BreathDirection dir)
{
    if (!m_active)
        return;

    float velNorm = juce::jlimit(0.1f, 1.0f, velocity);

    if (dir == BreathDirection::Inhale)
    {
        m_respiratoryPhase  = juce::MathConstants<float>::pi; // Pure Inhale (Draw/Suction)
        s_globalBreathPhase = juce::MathConstants<float>::pi;
        m_inhaleBlend       = 1.0f;
        m_suctionTransient  = 1.0f; // Re-trigger suction pitch scoop & chiff
    }
    else // Exhale
    {
        m_respiratoryPhase  = 0.0f; // Pure Exhale (Blow/Soplo)
        s_globalBreathPhase = 0.0f;
        m_inhaleBlend       = 0.0f;
        m_suctionTransient  = 0.0f;
    }

    // Instant update of acoustic formant filters
    updateFormants();

    // Retrigger mechanical fingerhole pop
    m_mechanicalPopLevel = 0.55f * velNorm;
    m_mechanicalPopPhase = 0.f;

    // Rhythmic diaphragm attack impulse
    float pulseSurge = 0.35f * velNorm;
    m_pressureSmooth = std::min(1.4f, m_pressureSmooth + pulseSurge);

    // If voice was in release phase, revive it with full energy
    m_releasing = false;
    m_amplitude = std::max(m_amplitude, 0.5f * velNorm);
}

//==============================================================================
float FreeReedModel::processSample()
{
    if (!m_active)
        return 0.f;

    // 1. Advance continuous respiratory lung cycle (The Modus Operandi)
    // When a low note is held or active, maintain continuous suction (Inhale/Draw)
    if (s_hasSustainedBassNote || m_baseMidiNote <= 60.5f)
    {
        // Smoothly and swiftly glide/lock to pure Inhale (Succión) at PI
        float targetPhase = juce::MathConstants<float>::pi;
        float diff = targetPhase - m_respiratoryPhase;
        while (diff < -juce::MathConstants<float>::pi) diff += juce::MathConstants<float>::twoPi;
        while (diff >  juce::MathConstants<float>::pi) diff -= juce::MathConstants<float>::twoPi;
        m_respiratoryPhase += 0.005f * diff;
    }
    else
    {
        float breathCycleInc = juce::MathConstants<float>::twoPi * m_params.breathSpeedHz / m_sampleRate;
        m_respiratoryPhase += breathCycleInc;
        if (m_respiratoryPhase >= juce::MathConstants<float>::twoPi)
            m_respiratoryPhase -= juce::MathConstants<float>::twoPi;
    }

    s_globalBreathPhase = m_respiratoryPhase;

    // Smooth Inhale/Exhale cross-fade factor: [0.0 = pure Exhale .. 1.0 = pure Inhale]
    m_inhaleBlend = 0.5f * (1.0f - std::cos(m_respiratoryPhase));

    // Decaying suction transient pitch scoop
    if (m_suctionTransient > 0.0001f)
        m_suctionTransient = std::max(0.0f, m_suctionTransient - m_suctionDecayRate);

    // Mechanical bamboo fingerhole pop & bronze tongue release impulse
    float popSound = 0.f;
    if (m_mechanicalPopLevel > 0.0001f)
    {
        m_mechanicalPopPhase += juce::MathConstants<float>::twoPi * (420.f + 160.f * m_inhaleBlend) / m_sampleRate;
        if (m_mechanicalPopPhase >= juce::MathConstants<float>::twoPi)
            m_mechanicalPopPhase -= juce::MathConstants<float>::twoPi;

        popSound = m_mechanicalPopLevel * std::sin(m_mechanicalPopPhase);
        m_mechanicalPopLevel = std::max(0.0f, m_mechanicalPopLevel - m_mechanicalPopDecay);
    }

    // Refresh dynamic formants every 16 samples for peak CPU efficiency
    if (++m_formantUpdateCounter >= 16)
    {
        m_formantUpdateCounter = 0;
        updateFormants();
    }

    // 2. Smooth breath pressure tracking
    float targetP = m_releasing ? 0.f : m_targetPressure;
    constexpr float kPressureSmoothing = 0.015f;
    m_pressureSmooth += kPressureSmoothing * (targetP - m_pressureSmooth);
    float Pm = m_pressureSmooth;

    // 3. Dynamic Aerodynamic Pitch Pulling & Micro-Jitter (Natural Acoustic Life)
    // Physical free reeds bend slightly with pressure (~6-12 cents on forte)
    float pressurePitchPull = 0.08f * (Pm - 0.60f);
    float suctionCentOffset = -28.0f * m_params.suctionBite * m_suctionTransient;
    float effectiveNote = m_baseMidiNote + (suctionCentOffset / 100.0f) + pressurePitchPull;
    float effectiveFreq = 440.f * std::pow(2.f, (effectiveNote - 69.f) / 12.f);
    m_reedFreq = juce::jlimit(20.f, m_sampleRate * 0.46f, effectiveFreq);

    // Cycle-to-cycle aero-elastic micro-jitter (~0.25% frequency perturbation)
    float rnd = nextRandomFloat();
    m_jitterFiltered = 0.982f * m_jitterFiltered + 0.018f * rnd;
    m_shimmerFiltered = 0.990f * m_shimmerFiltered + 0.010f * (1.0f + 0.035f * rnd);

    float jitteredFreq = m_reedFreq * (1.0f + 0.0025f * m_jitterFiltered);
    m_phaseIncrement = juce::MathConstants<float>::twoPi * jitteredFreq / m_sampleRate;

    m_phase += m_phaseIncrement;
    if (m_phase >= juce::MathConstants<float>::twoPi)
        m_phase -= juce::MathConstants<float>::twoPi;

    // Bronze reed cantilever mechanical eigenmodes (~6.27 f0 and ~17.5 f0)
    m_reedEigenPhase1 += m_phaseIncrement * 6.27f;
    if (m_reedEigenPhase1 >= juce::MathConstants<float>::twoPi)
        m_reedEigenPhase1 -= juce::MathConstants<float>::twoPi;

    m_reedEigenPhase2 += m_phaseIncrement * 17.5f;
    if (m_reedEigenPhase2 >= juce::MathConstants<float>::twoPi)
        m_reedEigenPhase2 -= juce::MathConstants<float>::twoPi;

    float bronzeEigenmodes = (0.040f * std::sin(m_reedEigenPhase1) + 0.015f * std::sin(m_reedEigenPhase2))
                           * (0.6f + 0.4f * m_params.reedStiffness);

    // 4. Multi-Harmonic Free-Reed Spectral Excitation (Odd & Even series)
    int maxHarmonics = std::min(28, static_cast<int>((m_sampleRate * 0.46f) / m_reedFreq));
    float nyquistLimit = m_sampleRate * 0.46f;
    float reedSignal = bronzeEigenmodes;

    // Inhale suction increases tongue vibration curvature and harmonic bite
    float inhaleBite = 1.0f + 0.85f * m_inhaleBlend * m_params.suctionBite;
    float stiffFactor = (0.75f + m_params.reedStiffness * 0.45f) * inhaleBite;

    for (int k = 1; k <= maxHarmonics; ++k)
    {
        float harmFreq = static_cast<float>(k) * m_reedFreq;
        float taper = juce::jlimit(0.f, 1.f, (nyquistLimit - harmFreq) / (m_reedFreq * 1.5f));
        if (taper <= 0.f) break;

        float amp = 0.f;
        switch (k)
        {
            case 1:  amp = 1.00f; break;
            case 2:  amp = 0.72f * stiffFactor; break;
            case 3:  amp = 0.52f; break;
            case 4:  amp = 0.40f * stiffFactor; break;
            case 5:  amp = 0.32f; break;
            case 6:  amp = 0.25f * stiffFactor; break;
            case 7:  amp = 0.20f; break;
            case 8:  amp = 0.16f * stiffFactor; break;
            default:
                amp = 0.16f * std::pow(8.0f / static_cast<float>(k), 1.15f) * ( (k % 2 == 0) ? stiffFactor : 1.0f );
                break;
        }

        // Slight natural dispersion across higher overtones for organic warmth
        float harmonicPhase = static_cast<float>(k) * m_phase + 0.035f * std::sin(static_cast<float>(k) * 0.5f);
        reedSignal += amp * taper * std::sin(harmonicPhase);
    }

    // 5. Smooth Acoustic Aerodynamic Slit Flow & Shimmer
    float reedSq = reedSignal * reedSignal;
    float aperture = 1.0f / (1.0f + 0.18f * reedSq);
    float flowVelocity = reedSignal * aperture;

    // Asymmetric hyperbolic excitation for rich bronze free-reed character
    float suctionSatDrive = 1.25f + 0.50f * m_inhaleBlend * m_params.suctionBite;
    float exciterSound = std::tanh(suctionSatDrive * flowVelocity + 0.14f * (flowVelocity * flowVelocity - 0.4f)) * m_shimmerFiltered;

    // 6. Turbulent Airflow Friction & Suction Chiff Noise
    // Pink-filtered acoustic air friction across the bamboo aperture and mouthbox
    float white = nextRandomFloat();
    m_noiseB0 = 0.99765f * m_noiseB0 + white * 0.0990460f;
    m_noiseB1 = 0.96300f * m_noiseB1 + white * 0.2965164f;
    m_noiseB2 = 0.57000f * m_noiseB2 + white * 1.0526913f;
    float pinkNoise = m_noiseB0 + m_noiseB1 + m_noiseB2 + white * 0.1848f;

    float chiffNoise = m_chiffFilter.process(pinkNoise);
    // Suction chiff is distinctly louder and textured during Inhale strokes
    float chiffGain = (0.010f + 0.085f * m_inhaleBlend * m_params.suctionBite) * Pm;

    // 7. Organic Diaphragm Breath Micro-Dynamics
    m_breathLfoPhase += 5.5f * juce::MathConstants<float>::twoPi / m_sampleRate;
    if (m_breathLfoPhase >= juce::MathConstants<float>::twoPi)
        m_breathLfoPhase -= juce::MathConstants<float>::twoPi;

    m_breathJitter = 0.97f * m_breathJitter + 0.03f * nextRandomFloat();

    // Diaphragm respiratory pressure modulation (breathing swell)
    float lungPressureSwell = 0.92f + 0.16f * (1.0f - 0.3f * m_inhaleBlend) * std::sin(m_respiratoryPhase);
    float breathVibrato = lungPressureSwell * (1.0f + 0.008f * std::sin(m_breathLfoPhase) + 0.006f * m_breathJitter);

    // Combine exciter, turbulent chiff, and mechanical bamboo pop
    float rawAcoustic = (exciterSound + chiffNoise * chiffGain + popSound * 0.45f) * breathVibrato * Pm;

    // 8. Quad Acoustic Body Resonators (Tao Windchest + Sympathetic Tao Body + Bamboo Chimney + Reed Buzz)
    float fChest = m_formantChest.process(rawAcoustic);
    float fTao   = m_sympatheticTao.process(fChest);
    float fPipe  = m_formantPipe.process(fTao);
    float fReed  = m_formantReed.process(fPipe);

    // 9. Output DC-blocking and Clean Acoustic Gain Staging
    float cleanAcoustic = m_dcBlocker.process(fReed);
    float outSample = cleanAcoustic * 0.44f;

    // Gentle soft-limiter only at extreme safety peaks
    if (std::abs(outSample) > 0.95f)
        outSample = std::tanh(outSample);

    // 10. Natural Anti-Pop Amplitude Envelope
    if (!m_releasing)
    {
        m_amplitude = std::min(1.f, m_amplitude + m_attackRate);
    }
    else
    {
        m_amplitude = std::max(0.f, m_amplitude - m_releaseRate);
        if (m_amplitude <= 0.0001f)
        {
            m_active = false;
            m_amplitude = 0.f;
        }
    }

    return outSample * m_amplitude;
}
