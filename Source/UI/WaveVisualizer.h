#pragma once
#include <JuceHeader.h>
#include <array>

class KhaenAudioProcessor;

//==============================================================================
/**
 * WaveVisualizer — Futuristic 2D/3D Spectral, Vector & Modal Visualizer Engine
 *
 * Provides 4 rich real-time visualization modes matching the Aurora RTA neon aesthetic:
 *
 * 1. Mode::Modal2D       — 3D Perspective Harmonic Waterfall / Resonator Fins
 * 2. Mode::AuroraRTA     — Real-time 128-Band Spectral Aurora with Formant Tracking
 * 3. Mode::AeroVectorial — 2D Wireframe Bamboo Pipes with Dynamic Neon Glow & Vertex Vibration
 * 4. Mode::ReedMatrix    — Isometric 2D Diamond Windchest Grid with Air Pressure Halos
 *
 * Fully lock-free, zero-allocation rendering at 60 FPS via juce::Timer.
 */
class WaveVisualizer : public juce::Component,
                       private juce::Timer
{
public:
    enum class VisualMode
    {
        Modal2D = 0,       // 3D Perspective Harmonic Waterfall / Resonator Fins
        AuroraRTA,         // Real-time 128-band Spectral Aurora with Formant Tracking
        AeroVectorial,     // 2D Wireframe Bamboo Pipes with Pressure Dynamics & Vibration Vertex Displacement
        ReedMatrix         // Isometric 2D Diamond/Rhombus Windchest Node Grid with Air Pressure Halos
    };

    explicit WaveVisualizer (KhaenAudioProcessor& proc);
    ~WaveVisualizer() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent& e) override;

    void setVisualMode (VisualMode mode);
    VisualMode getVisualMode() const noexcept { return m_currentMode; }

    void setDisplayBuffer (const float* samples, int numSamples, float rms);
    void updateDisplay();

private:
    void timerCallback() override;

    void loadBackgroundImage();
    void collectSamples();
    void drawBackground    (juce::Graphics& g);
    void drawGrid          (juce::Graphics& g);
    void drawModeSelector  (juce::Graphics& g);

    // Visualizer Modes
    void drawModal2D       (juce::Graphics& g);
    void drawAuroraRTA     (juce::Graphics& g);
    void drawAeroVectorial (juce::Graphics& g);
    void drawReedMatrix    (juce::Graphics& g);

    // HUD Elements
    void drawRmsBar        (juce::Graphics& g);
    void drawNoteHud       (juce::Graphics& g);
    void drawBreathHud     (juce::Graphics& g);

    KhaenAudioProcessor& m_proc;
    VisualMode m_currentMode = VisualMode::Modal2D;

    std::array<juce::Rectangle<float>, 4> m_modeBtnBounds;

    static constexpr int kDisplaySamples = 512;
    std::array<float, kDisplaySamples> m_displayBuf {};
    int   m_displayWrite = 0;
    float m_peakDecay    = 0.f;
    float m_animationPhase = 0.f;

    // Atmospheric artwork background
    juce::Image m_bgImage;
    bool m_bgImageLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveVisualizer)
};
