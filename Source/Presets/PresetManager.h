#pragma once
#include <JuceHeader.h>
#include "DSP/FreeReedModel.h"

//==============================================================================
/**
 * PresetManager — 10 Built-in Khaen Presets
 *
 * Presets are stored as ValueTree snapshots compatible with APVTS.
 * Each preset encodes physical model parameters for a distinct timbre.
 */
class PresetManager
{
public:
    struct Preset
    {
        juce::String            name;
        FreeReedModel::Parameters physParams;

        // FX chain defaults for this preset
        float delayTimeMs        = 400.f;
        float delayFeedback      = 0.35f;
        float delayMix           = 0.20f;
        bool  delayPingPong      = false;

        float harmonShift        = 7.f;
        float harmonMix          = 0.30f;

        float crystalGrainMs     = 120.f;
        float crystalDelayMs     = 350.f;
        float crystalFeedback    = 0.40f;
        float crystalMix         = 0.25f;
        int   crystalMode        = 0;

        // Master macro defaults
        float macroAir           = 0.5f;   // breath/attack macro
        float macroEmbouchure    = 0.5f;   // lip/stiffness macro
        float macroResonance     = 0.5f;   // bore/resonance macro
    };

    PresetManager();

    int           getNumPresets()            const { return static_cast<int>(m_presets.size()); }
    const Preset& getPreset (int index)      const;
    int           getCurrentPresetIndex()    const { return m_currentIndex; }

    void          loadPreset (int index, juce::AudioProcessorValueTreeState& apvts);
    void          saveUserPreset (const juce::String& name,
                                  juce::AudioProcessorValueTreeState& apvts);

    juce::StringArray getPresetNames() const;

private:
    std::vector<Preset> m_presets;
    int m_currentIndex = 0;

    void buildPresets();
    void applyPhysicsToApvts (const Preset& p,
                               juce::AudioProcessorValueTreeState& apvts);
};
