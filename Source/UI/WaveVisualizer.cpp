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
    drawNoteHud(g);
    drawBreathHud(g);
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

//==============================================================================
void WaveVisualizer::drawNoteHud (juce::Graphics& g)
{
    float activeNote = m_proc.getActiveNoteNumber();
    auto b = getLocalBounds().toFloat();

    // Top-right glass pill badge
    float badgeW = 140.f;
    float badgeH = 26.f;
    juce::Rectangle<float> badgeRect (b.getRight() - badgeW - 10.f, b.getY() + 8.f, badgeW, badgeH);

    // Glass pill background
    g.setColour(juce::Colour(0x80101018));
    g.fillRoundedRectangle(badgeRect, 6.f);
    g.setColour(juce::Colour(0x40C8965A));
    g.drawRoundedRectangle(badgeRect, 6.f, 1.f);

    if (activeNote >= 0.f && activeNote <= 127.f)
    {
        static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int noteInt = static_cast<int>(std::round(activeNote));
        int noteClass = (noteInt % 12 + 12) % 12;
        int octave = (noteInt / 12) - 1;
        float cents = (activeNote - static_cast<float>(noteInt)) * 100.f;
        float freqHz = 440.0f * std::pow(2.0f, (activeNote - 69.0f) / 12.0f);

        juce::String noteStr = juce::String(kNoteNames[noteClass]) + juce::String(octave);
        juce::String centsStr = (cents >= 0.f ? "+" : "") + juce::String(static_cast<int>(cents)) + "ct";
        juce::String freqStr = (freqHz < 100.f ? juce::String(freqHz, 1) : juce::String(static_cast<int>(std::round(freqHz)))) + " Hz";

        // Draw glowing note name (Left side of badge)
        g.setFont(juce::Font(juce::FontOptions().withHeight(15.f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFFFFD070));
        g.drawText(noteStr, badgeRect.withWidth(55.f).translated(8.f, 0.f), juce::Justification::centredLeft, false);

        // Draw frequency and micro-tuning cents (Right side of badge)
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.f)));
        g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.90f));
        juce::String fullMeta = freqStr + " " + centsStr;
        g.drawText(fullMeta, badgeRect.reduced(8.f, 0.f), juce::Justification::centredRight, false);
    }
    else
    {
        // Standby state
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.f)));
        g.setColour(juce::Colour(0xFF888899).withAlpha(0.6f));
        g.drawText("NOTE TRACKER", badgeRect, juce::Justification::centred, false);
    }
}

//==============================================================================
void WaveVisualizer::drawBreathHud (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float badgeW = 165.f;
    float badgeH = 26.f;
    juce::Rectangle<float> badgeRect (b.getX() + 10.f, b.getY() + 8.f, badgeW, badgeH);

    // Glass pill background
    g.setColour(juce::Colour(0x80101018));
    g.fillRoundedRectangle(badgeRect, 6.f);

    float breathPhase = m_proc.getBreathPhase();
    bool isInhale = m_proc.isBreathInhale();
    bool isSustainedSuction = FreeReedModel::hasSustainedBassNote();
    float ksFlash = m_proc.getKeyswitchPulseFlash();
    int lastKs = m_proc.getLastKeyswitchNote();

    juce::Colour themeColor = isInhale ? juce::Colour(0xFF4EE2D2) : juce::Colour(0xFFFFC04D);
    g.setColour(themeColor.withAlpha(isSustainedSuction ? 0.75f : (ksFlash > 0.05f ? 0.65f : 0.40f)));
    g.drawRoundedRectangle(badgeRect, 6.f, 1.f);

    // Pulse indicator dot
    float dotRadius = isSustainedSuction ? 4.5f : 4.f;
    float dotCenterY = badgeRect.getCentreY();
    float dotCenterX = badgeRect.getX() + 14.f;
    g.setColour(themeColor);
    g.fillEllipse(dotCenterX - dotRadius, dotCenterY - dotRadius, dotRadius * 2.f, dotRadius * 2.f);

    // Dynamic keyswitch pulse ripple ring
    if (ksFlash > 0.01f)
    {
        float ringRadius = dotRadius + 5.f * (1.0f - ksFlash);
        g.setColour(themeColor.withAlpha(ksFlash * 0.85f));
        g.drawEllipse(dotCenterX - ringRadius, dotCenterY - ringRadius, ringRadius * 2.f, ringRadius * 2.f, 1.2f);
    }

    // Breath mode label
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.5f).withStyle("Bold")));
    g.setColour(themeColor);
    
    juce::String modeStr;
    if (ksFlash > 0.15f && lastKs >= 24)
    {
        modeStr = (isInhale ? "KEY: INHALE (D1)" : "KEY: EXHALE (C1)");
    }
    else if (isSustainedSuction)
    {
        modeStr = "INHALE (SUCTION)";
    }
    else
    {
        modeStr = isInhale ? "INHALE (DRAW)" : "EXHALE (BLOW)";
    }

    g.drawText(modeStr, badgeRect.withTrimmedLeft(26.f).withTrimmedRight(6.f), juce::Justification::centredLeft, false);

    // Dynamic respiratory cycle micro-gauge along bottom
    float normPhase = breathPhase / juce::MathConstants<float>::twoPi;
    normPhase = juce::jlimit(0.f, 1.f, normPhase);
    float gaugeW = (badgeW - 16.f) * (isSustainedSuction ? 1.0f : normPhase);
    g.setColour(themeColor.withAlpha(0.85f));
    g.fillRoundedRectangle(badgeRect.getX() + 8.f, badgeRect.getBottom() - 3.5f, gaugeW, 2.f, 1.f);
}

