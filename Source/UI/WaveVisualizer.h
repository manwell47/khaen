#pragma once
#include <JuceHeader.h>

class KhaenAudioProcessor;

//==============================================================================
/**
 * WaveVisualizer — Real-time Digital Waveguide Display
 *
 * Draws the output waveform as a glowing bamboo-reed oscilloscope trace.
 * Updates at 30fps via juce::Timer. Reads from the processor's visualizer
 * ring buffer (lock-free).
 *
 * Visual style:
 *  - Dark panel with subtle noise texture
 *  - Copper/amber waveform with bloom glow
 *  - Fade trail effect (older samples dimmer)
 *  - Subtle grid lines
 */
class WaveVisualizer : public juce::Component,
                       private juce::Timer
{
public:
    explicit WaveVisualizer (KhaenAudioProcessor& proc);
    ~WaveVisualizer() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    void collectSamples();
    void drawGrid       (juce::Graphics& g);
    void drawWaveform   (juce::Graphics& g);
    void drawGlow       (juce::Graphics& g);
    void drawRmsBar     (juce::Graphics& g);
    void drawNoteHud    (juce::Graphics& g);
    void drawBreathHud  (juce::Graphics& g);

    KhaenAudioProcessor& m_proc;

    static constexpr int kDisplaySamples = 512;
    std::array<float, kDisplaySamples> m_displayBuf {};
    int   m_displayWrite = 0;
    float m_peakDecay    = 0.f;
};
