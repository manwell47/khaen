#include "PresetManager.h"

//==============================================================================
PresetManager::PresetManager()
{
    buildPresets();
}

//==============================================================================
void PresetManager::buildPresets()
{
    using M = FreeReedModel::Material;

    m_presets = {
        // ── 1. Classic Khaen (Pure Authentic Acoustic) ───────────────────────
        {
            "Classic Khaen",
            { 0.65f, 14.f, 0.60f, 0.30f, 0.97f, 1.20f, 0.65f, M::Bamboo, true },
            400.f, 0.30f, 0.0f, false,    // delay mix = 0 (dry)
            0.f, 0.0f,                    // harmonizer mix = 0 (dry)
            120.f, 350.f, 0.35f, 0.0f, 0, // crystallizer mix = 0 (dry)
            0.50f, 0.50f, 0.50f
        },
        // ── 2. Breath of Laos ───────────────────────────────────────────────
        {
            "Breath of Laos",
            { 0.50f, 16.f, 0.65f, 0.20f, 0.96f, 0.95f, 0.75f, M::AgedBamboo, true },
            500.f, 0.45f, 0.22f, true,
            5.f, 0.30f,
            150.f, 400.f, 0.42f, 0.28f, 0,
            0.65f, 0.40f, 0.55f
        },
        // ── 3. Jade Reed ────────────────────────────────────────────────────
        {
            "Jade Reed",
            { 0.80f, 12.f, 0.50f, 0.25f, 0.975f, 1.40f, 0.50f, M::Jade, true },
            300.f, 0.30f, 0.15f, false,
            12.f, 0.20f,
            90.f, 250.f, 0.35f, 0.18f, 1,
            0.45f, 0.65f, 0.42f
        },
        // ── 4. Copper Tongue ────────────────────────────────────────────────
        {
            "Copper Tongue",
            { 0.90f, 10.f, 0.60f, 0.35f, 0.98f, 1.60f, 0.80f, M::Brass, true },
            250.f, 0.28f, 0.14f, false,
            7.f, 0.15f,
            80.f, 200.f, 0.30f, 0.12f, 1,
            0.55f, 0.75f, 0.38f
        },
        // ── 5. Ghost Pipe ───────────────────────────────────────────────────
        {
            "Ghost Pipe",
            { 0.30f, 20.f, 0.40f, 0.15f, 0.94f, 0.60f, 0.70f, M::PVC, true },
            700.f, 0.55f, 0.30f, true,
            -5.f, 0.35f,
            200.f, 600.f, 0.50f, 0.35f, 2,
            0.35f, 0.30f, 0.70f
        },
        // ── 6. Deep Resonance ────────────────────────────────────────────────
        {
            "Deep Resonance",
            { 0.45f, 22.f, 0.70f, 0.18f, 0.95f, 0.80f, 0.85f, M::DarkWood, true },
            600.f, 0.50f, 0.25f, false,
            -12.f, 0.40f,
            180.f, 500.f, 0.48f, 0.30f, 1,
            0.70f, 0.35f, 0.75f
        },
        // ── 7. Crystal Air ───────────────────────────────────────────────────
        {
            "Crystal Air",
            { 0.70f, 13.f, 0.45f, 0.22f, 0.97f, 1.30f, 0.60f, M::Bamboo, true },
            400.f, 0.40f, 0.28f, true,
            7.f, 0.45f,
            100.f, 350.f, 0.45f, 0.40f, 0,
            0.48f, 0.55f, 0.60f
        },
        // ── 8. Storm Reed ────────────────────────────────────────────────────
        {
            "Storm Reed",
            { 0.95f, 11.f, 0.80f, 0.40f, 0.985f, 1.80f, 0.90f, M::Metal, true },
            180.f, 0.20f, 0.10f, false,
            5.f, 0.10f,
            60.f, 150.f, 0.25f, 0.08f, 1,
            0.80f, 0.85f, 0.30f
        },
        // ── 9. Silk Breath ───────────────────────────────────────────────────
        {
            "Silk Breath",
            { 0.40f, 18.f, 0.48f, 0.12f, 0.96f, 0.75f, 0.40f, M::Bamboo, true },
            550.f, 0.48f, 0.25f, true,
            4.f, 0.35f,
            160.f, 450.f, 0.44f, 0.32f, 0,
            0.42f, 0.38f, 0.65f
        },
        // ── 10. Void Drone ───────────────────────────────────────────────────
        {
            "Void Drone",
            { 0.20f, 28.f, 0.30f, 0.10f, 0.93f, 0.50f, 0.70f, M::PVC, true },
            900.f, 0.70f, 0.40f, true,
            -12.f, 0.50f,
            300.f, 800.f, 0.65f, 0.50f, 2,
            0.25f, 0.20f, 0.85f
        }
    };
}

//==============================================================================
const PresetManager::Preset& PresetManager::getPreset (int index) const
{
    int clamped = juce::jlimit(0, static_cast<int>(m_presets.size()) - 1, index);
    return m_presets[clamped];
}

//==============================================================================
juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : m_presets)
        names.add(p.name);
    return names;
}

//==============================================================================
void PresetManager::loadPreset (int index, juce::AudioProcessorValueTreeState& apvts)
{
    m_currentIndex = juce::jlimit(0, static_cast<int>(m_presets.size()) - 1, index);
    applyPhysicsToApvts(m_presets[m_currentIndex], apvts);
}

//==============================================================================
void PresetManager::applyPhysicsToApvts (const Preset& p,
                                          juce::AudioProcessorValueTreeState& apvts)
{
    auto setParam = [&apvts](const juce::String& id, float val)
    {
        if (auto* param = apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(val));
    };

    // Physics
    setParam("reedStiffness",    p.physParams.reedStiffness);
    setParam("boreDiameter",     p.physParams.boreDiameterMm);
    setParam("blowPressure",     p.physParams.blowPressure);
    setParam("lipDamping",       p.physParams.lipDamping);
    setParam("boreReflection",   p.physParams.boreReflection);
    setParam("material",         static_cast<float>(p.physParams.material));
    setParam("breathSpeed",      p.physParams.breathSpeedHz);
    setParam("suctionBite",      p.physParams.suctionBite);

    // Macros
    setParam("macroAir",         p.macroAir);
    setParam("macroEmbouchure",  p.macroEmbouchure);
    setParam("macroResonance",   p.macroResonance);

    // Delay
    setParam("delayTime",        p.delayTimeMs);
    setParam("delayFeedback",    p.delayFeedback);
    setParam("delayMix",         p.delayMix);
    setParam("delayPingPong",    p.delayPingPong ? 1.f : 0.f);
    setParam("delayBypass",      p.delayMix <= 0.001f ? 1.f : 0.f);

    // Harmonizer
    setParam("harmonShift",      p.harmonShift);
    setParam("harmonMix",        p.harmonMix);
    setParam("harmonBypass",     p.harmonMix <= 0.001f ? 1.f : 0.f);

    // Crystallizer
    setParam("crystalGrainMs",   p.crystalGrainMs);
    setParam("crystalDelayMs",   p.crystalDelayMs);
    setParam("crystalFeedback",  p.crystalFeedback);
    setParam("crystalMix",       p.crystalMix);
    setParam("crystalMode",      static_cast<float>(p.crystalMode));
    setParam("crystalBypass",    p.crystalMix <= 0.001f ? 1.f : 0.f);
}
