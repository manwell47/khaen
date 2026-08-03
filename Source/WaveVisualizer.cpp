#include "WaveVisualizer.h"
#include "../PluginProcessor.h"

//==============================================================================
WaveVisualizer::WaveVisualizer (KhaenAudioProcessor& proc)
    : m_proc(proc)
{
    m_displayBuf.fill(0.f);
    startTimerHz(30);
}

WaveVisualizer::~WaveVisualizer()
{
    stopTimer();
}

//==============================================================================
void WaveVisualizer::resized() { repaint(); }

void WaveVisualizer::timerCallback()
{
    collectSamples();
    repaint();
}

//==============================================================================
void WaveVisualizer::collectSamples()
{
    // Drain audio processor ring buffer into display buffer
    for (int i = 0; i < 16; ++i) // collect up to 16 samples per frame
    {
        float s = m_proc.getVisualizerSample();
        m_displayBuf[m_displayWrite] = s;
        m_displayWrite = (m_displayWrite + 1) % kDisplaySamples;
    }

    float rms = m_proc.getOutputRms();
    m_peakDecay = std::max(m_peakDecay * 0.97f, rms);
}

//==============================================================================
void WaveVisualizer::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // ── Background ──────────────────────────────────────────────────────────
    auto bgGrad = juce::ColourGradient::vertical(
        juce::Colour(0xFF111118), b.getY(),
        juce::Colour(0xFF0A0A0F), b.getBottom());
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(b, 6.f);

    // Border
    g.setColour(juce::Colour(0xFF2A2A3A));
    g.drawRoundedRectangle(b.reduced(0.5f), 6.f, 1.f);

    drawGrid(g);
    drawGlow(g);
    drawWaveform(g);
    drawRmsBar(g);
}

//==============================================================================
void WaveVisualizer::drawGrid (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xFF1E1E2E));
    // Centre line
    g.drawHorizontalLine(static_cast<int>(b.getCentreY()), b.getX() + 8, b.getRight() - 8);
    // Vertical guides
    for (int i = 1; i < 4; ++i)
    {
        float x = b.getX() + b.getWidth() * (i / 4.f);
        g.drawVerticalLine(static_cast<int>(x),
                           b.getY() + 8, b.getBottom() - 8);
    }
}

//==============================================================================
void WaveVisualizer::drawWaveform (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(8.f, 4.f);
    float cx = b.getCentreY();
    float scaleY = b.getHeight() * 0.42f;
    float stepX  = b.getWidth() / static_cast<float>(kDisplaySamples - 1);

    juce::Path path;
    bool started = false;

    for (int i = 0; i < kDisplaySamples; ++i)
    {
        int   idx = (m_displayWrite + i) % kDisplaySamples;
        float s   = juce::jlimit(-1.f, 1.f, m_displayBuf[idx]);
        float x   = b.getX() + i * stepX;
        float y   = cx - s * scaleY;

        if (!started) { path.startNewSubPath(x, y); started = true; }
        else           path.lineTo(x, y);
    }

    // Glow stroke (wider, dimmer)
    g.setColour(juce::Colour(0xFFC8965A).withAlpha(0.15f));
    g.strokePath(path, juce::PathStrokeType(4.f,
                 juce::PathStrokeType::curved,
                 juce::PathStrokeType::rounded));

    // Main stroke
    g.setColour(juce::Colour(0xFFC8965A).withAlpha(0.85f));
    g.strokePath(path, juce::PathStrokeType(1.5f,
                 juce::PathStrokeType::curved,
                 juce::PathStrokeType::rounded));
}

//==============================================================================
void WaveVisualizer::drawGlow (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float alpha = juce::jlimit(0.f, 0.3f, m_peakDecay * 0.8f);

    auto glow = juce::ColourGradient(
        juce::Colour(0xFFC8965A).withAlpha(alpha),
        b.getCentreX(), b.getCentreY(),
        juce::Colour(0x00C8965A),
        b.getCentreX(), b.getY(), true);

    g.setGradientFill(glow);
    g.fillRoundedRectangle(b, 6.f);
}

//==============================================================================
void WaveVisualizer::drawRmsBar (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float rms    = m_proc.getOutputRms();
    float barW   = juce::jlimit(0.f, b.getWidth() - 16.f, rms * (b.getWidth() - 16.f) * 2.f);
    float barH   = 2.f;
    float barY   = b.getBottom() - 6.f;

    // Background track
    g.setColour(juce::Colour(0xFF1E1E2E));
    g.fillRoundedRectangle(b.getX() + 8.f, barY, b.getWidth() - 16.f, barH, 1.f);

    // Active bar
    auto barColor = juce::Colour::fromHSV(0.08f, 0.7f,
        juce::jmap(rms, 0.f, 0.8f, 0.5f, 1.f), 1.f);
    g.setColour(barColor);
    g.fillRoundedRectangle(b.getX() + 8.f, barY, barW, barH, 1.f);
}
