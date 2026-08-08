#include "WaveVisualizer.h"
#include "../PluginProcessor.h"
#include <cmath>
#include <vector>
#include <algorithm>

//==============================================================================
namespace
{
    // Catmull-Rom to Cubic Bézier smooth spline builder for ultra-fluid curves
    void buildSmoothSpline (juce::Path& path, const std::vector<juce::Point<float>>& points)
    {
        if (points.empty()) return;
        if (points.size() == 1)
        {
            path.startNewSubPath(points[0]);
            return;
        }

        path.startNewSubPath(points[0]);
        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            auto p0 = (i == 0) ? points[0] : points[i - 1];
            auto p1 = points[i];
            auto p2 = points[i + 1];
            auto p3 = (i + 2 < points.size()) ? points[i + 2] : p2;

            float c1x = p1.x + (p2.x - p0.x) / 6.0f;
            float c1y = p1.y + (p2.y - p0.y) / 6.0f;
            float c2x = p2.x - (p3.x - p1.x) / 6.0f;
            float c2y = p2.y - (p3.y - p1.y) / 6.0f;

            path.cubicTo(c1x, c1y, c2x, c2y, p2.x, p2.y);
        }
    }

    // High-resolution cubic interpolation across 128 discrete frequency bands
    float interpolateBand (const std::array<float, SpectralAnalyzer::kNumBands>& data, float normPos)
    {
        float pos = normPos * static_cast<float>(SpectralAnalyzer::kNumBands - 1);
        int idx = static_cast<int>(pos);
        float frac = pos - static_cast<float>(idx);

        if (idx < 0) return data[0];
        if (idx >= SpectralAnalyzer::kNumBands - 1) return data[SpectralAnalyzer::kNumBands - 1];

        float y0 = (idx > 0) ? data[static_cast<size_t>(idx - 1)] : data[static_cast<size_t>(idx)];
        float y1 = data[static_cast<size_t>(idx)];
        float y2 = data[static_cast<size_t>(idx + 1)];
        float y3 = (idx + 2 < SpectralAnalyzer::kNumBands) ? data[static_cast<size_t>(idx + 2)] : y2;

        // Catmull-Rom cubic polynomial
        float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float a2 = -0.5f * y0 + 0.5f * y2;
        float a3 = y1;

        float val = ((a0 * frac + a1) * frac + a2) * frac + a3;
        return std::max(0.0f, val);
    }
}

//==============================================================================
WaveVisualizer::WaveVisualizer (KhaenAudioProcessor& proc)
    : m_proc(proc)
{
    m_displayBuf.fill(0.f);
    startTimerHz(60); // 60 FPS smooth animation
}

WaveVisualizer::~WaveVisualizer()
{
    stopTimer();
}

//==============================================================================
void WaveVisualizer::resized()
{
    auto b = getLocalBounds().toFloat();
    float totalBtnW = 400.0f;
    float btnW = totalBtnW / 4.0f;
    float btnH = 23.0f;
    float startX = b.getCentreX() - (totalBtnW * 0.5f);
    float btnY = b.getY() + 8.0f;

    for (int i = 0; i < 4; ++i)
    {
        m_modeBtnBounds[static_cast<size_t>(i)] = juce::Rectangle<float>(startX + i * btnW, btnY, btnW - 3.0f, btnH);
    }

    repaint();
}

void WaveVisualizer::mouseUp (const juce::MouseEvent& e)
{
    auto pos = e.position;
    if (m_modeBtnBounds[0].contains(pos))
    {
        setVisualMode(VisualMode::Modal2D);
    }
    else if (m_modeBtnBounds[1].contains(pos))
    {
        setVisualMode(VisualMode::AuroraRTA);
    }
    else if (m_modeBtnBounds[2].contains(pos))
    {
        setVisualMode(VisualMode::AeroVectorial);
    }
    else if (m_modeBtnBounds[3].contains(pos))
    {
        setVisualMode(VisualMode::ReedMatrix);
    }
}

void WaveVisualizer::setVisualMode (VisualMode mode)
{
    if (m_currentMode != mode)
    {
        m_currentMode = mode;
        repaint();
    }
}

void WaveVisualizer::timerCallback()
{
    updateDisplay();
}

void WaveVisualizer::updateDisplay()
{
    double sr = m_proc.getSampleRate() > 0.0 ? m_proc.getSampleRate() : 44100.0;
    m_proc.getSpectralAnalyzer().processSpectralData(sr);
    
    collectSamples();
    m_animationPhase += 0.04f;
    if (m_animationPhase > juce::MathConstants<float>::twoPi)
        m_animationPhase -= juce::MathConstants<float>::twoPi;

    repaint();
}

void WaveVisualizer::setDisplayBuffer (const float* samples, int numSamples, float rms)
{
    if (samples != nullptr && numSamples > 0)
    {
        int count = std::min(numSamples, kDisplaySamples);
        for (int i = 0; i < count; ++i)
            m_displayBuf[i] = samples[i];
        for (int i = count; i < kDisplaySamples; ++i)
            m_displayBuf[i] = 0.0f;
        m_displayWrite = 0;
    }
    m_peakDecay = juce::jlimit(0.0f, 1.0f, rms);
    repaint();
}

void WaveVisualizer::collectSamples()
{
    // Auto decay peak
    m_peakDecay = std::max(0.0f, m_peakDecay * 0.94f);
}

//==============================================================================
void WaveVisualizer::loadBackgroundImage()
{
    if (m_bgImageLoaded) return;
    m_bgImageLoaded = true;

    if (BinaryData::Breath_of_Laos_pngSize > 0)
    {
        m_bgImage = juce::ImageFileFormat::loadFrom(BinaryData::Breath_of_Laos_png, static_cast<size_t>(BinaryData::Breath_of_Laos_pngSize));
    }

    if (!m_bgImage.isValid())
    {
        auto cwdFile = juce::File::getCurrentWorkingDirectory().getChildFile("Breath of Laos.png");
        if (cwdFile.existsAsFile())
            m_bgImage = juce::ImageFileFormat::loadFrom(cwdFile);
        else
        {
            auto exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Breath of Laos.png");
            if (exeFile.existsAsFile())
                m_bgImage = juce::ImageFileFormat::loadFrom(exeFile);
        }
    }
}

void WaveVisualizer::drawBackground (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    loadBackgroundImage();

    // ── Ultra-Sleek Cyber Glass Background ───────────────────────────────────
    auto bgGrad = juce::ColourGradient::vertical(
        juce::Colour(0xFF090C14), b.getY(),
        juce::Colour(0xFF05070B), b.getBottom());
    bgGrad.addColour(0.45, juce::Colour(0xFF0C101A));
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(b, 8.0f);

    // ── Atmospheric "Breath of Laos" Artwork Background (Elegant Opacity) ────
    if (m_bgImage.isValid())
    {
        juce::Graphics::ScopedSaveState sss(g);
        
        juce::Path clipPath;
        clipPath.addRoundedRectangle(b, 8.0f);
        g.reduceClipRegion(clipPath);

        g.setOpacity(0.22f);
        g.drawImage(m_bgImage, b, juce::RectanglePlacement::fillDestination);

        // Soft Radial Vignette to seamlessly blend the artwork into the dark cyber glass
        g.setOpacity(1.0f);
        auto vignetteGrad = juce::ColourGradient(
            juce::Colour(0x0005070B), b.getCentreX(), b.getCentreY(),
            juce::Colour(0xD805070B), b.getX(), b.getY(), true);
        vignetteGrad.addColour(0.50, juce::Colour(0x2005070B));
        g.setGradientFill(vignetteGrad);
        g.fillRoundedRectangle(b, 8.0f);
    }

    // Inner subtle glow border
    g.setColour(juce::Colour(0x2800E5FF));
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);
}

//==============================================================================
void WaveVisualizer::paint (juce::Graphics& g)
{
    drawBackground(g);
    drawGrid(g);

    switch (m_currentMode)
    {
        case VisualMode::Modal2D:
            drawModal2D(g);
            break;
        case VisualMode::AuroraRTA:
            drawAuroraRTA(g);
            break;
        case VisualMode::AeroVectorial:
            drawAeroVectorial(g);
            break;
        case VisualMode::ReedMatrix:
            drawReedMatrix(g);
            break;
    }

    // ── Top Header Controls & HUD Elements ───────────────────────────────────
    drawModeSelector(g);
    drawRmsBar(g);
    drawBreathHud(g);
    drawNoteHud(g);
}

//==============================================================================
void WaveVisualizer::drawGrid (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(12.f, 8.f);
    float baseY  = b.getBottom() - 16.f;
    float topY   = b.getY() + 38.f;
    float totalH = baseY - topY;

    // Subtle horizontal dB reference grid lines (-12dB, -24dB, -36dB, -48dB)
    static const float kDbRatios[] = { 0.20f, 0.40f, 0.60f, 0.80f };
    for (float r : kDbRatios)
    {
        float gy = topY + totalH * r;
        g.setColour(juce::Colour(0x0C304260));
        g.drawHorizontalLine(static_cast<int>(gy), b.getX(), b.getRight());
    }

    // Baseline
    g.setColour(juce::Colour(0x35304260));
    g.drawHorizontalLine(static_cast<int>(baseY), b.getX(), b.getRight());

    // ── Frequency Scale Axis & Labels (Modal2D & AuroraRTA) ──────────────────
    if (m_currentMode == VisualMode::Modal2D || m_currentMode == VisualMode::AuroraRTA)
    {
        struct FreqMarker { float freq; const char* label; };
        static const FreqMarker markers[] = {
            { 50.0f,    "50Hz"  },
            { 100.0f,   "100Hz" },
            { 250.0f,   "250Hz" },
            { 500.0f,   "500Hz" },
            { 1000.0f,  "1kHz"  },
            { 2500.0f,  "2.5kHz"},
            { 6000.0f,  "6kHz"  },
            { 16000.0f, "16kHz" }
        };

        float logMin = std::log10(25.0f);
        float logMax = std::log10(20000.0f);

        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));

        for (const auto& m : markers)
        {
            float norm = (std::log10(m.freq) - logMin) / (logMax - logMin);
            norm = juce::jlimit(0.0f, 1.0f, norm);
            float fx = b.getX() + norm * b.getWidth();

            // Subtle vertical tick
            g.setColour(juce::Colour(0x286E748B));
            g.drawLine(fx, baseY, fx, baseY + 3.0f, 1.0f);

            // Sleek frequency label
            juce::Rectangle<float> lblRect(fx - 22.0f, baseY + 3.0f, 44.0f, 11.0f);
            g.setColour(juce::Colour(0xFF5A6075));
            g.drawText(m.label, lblRect, juce::Justification::centred, false);
        }
    }
}

//==============================================================================
void WaveVisualizer::drawModeSelector (juce::Graphics& g)
{
    static const char* kModeTitles[4] = { "3D MODAL", "AURORA RTA", "AERO-VECTORIAL", "REED MATRIX" };

    for (int i = 0; i < 4; ++i)
    {
        auto rect = m_modeBtnBounds[static_cast<size_t>(i)];
        bool isSelected = (static_cast<int>(m_currentMode) == i);

        if (isSelected)
        {
            // Active glowing glass pill
            auto pillGrad = juce::ColourGradient::vertical(
                juce::Colour(0x5000E5FF), rect.getY(),
                juce::Colour(0x200084A8), rect.getBottom());
            g.setGradientFill(pillGrad);
            g.fillRoundedRectangle(rect, 5.0f);

            // Glow border
            g.setColour(juce::Colour(0xFF00E5FF));
            g.drawRoundedRectangle(rect, 5.0f, 1.2f);

            g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFFFFFFFF));
            g.drawText(kModeTitles[i], rect, juce::Justification::centred, false);
        }
        else
        {
            // Inactive pill
            g.setColour(juce::Colour(0x4012141E));
            g.fillRoundedRectangle(rect, 5.0f);

            g.setColour(juce::Colour(0xFF282C3C));
            g.drawRoundedRectangle(rect, 5.0f, 0.8f);

            g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
            g.setColour(juce::Colour(0xFF7A8098));
            g.drawText(kModeTitles[i], rect, juce::Justification::centred, false);
        }
    }
}

//==============================================================================
// Mode 1: 2D Modal Harmonic Waterfall / Resonator Fins
// Ultra-smooth Catmull-Rom Spline continuous curved surface with multi-depth shading
//==============================================================================
void WaveVisualizer::drawModal2D (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(12.f, 8.f);
    const auto& analyzer = m_proc.getSpectralAnalyzer();
    const int numSlices = SpectralAnalyzer::kDepthSlices; // 20 depth slices
    const int kCurveResolution = 128; // High-resolution interpolated points for smooth spline

    float frontY = b.getBottom() - 16.f;
    float backY  = b.getY() + 38.f;
    float centerX = b.getCentreX();

    std::vector<juce::Point<float>> points;
    points.reserve(kCurveResolution);

    // Render back-to-front for realistic depth occlusion
    for (int z = numSlices - 1; z >= 0; --z)
    {
        float t = static_cast<float>(z) / static_cast<float>(numSlices - 1); // 0.0 (front) .. 1.0 (back)
        float depthFactor = std::pow(t, 0.82f);

        float sliceY = frontY - (frontY - backY) * depthFactor;
        float sliceWidth = (b.getWidth() - 20.f) * (1.0f - depthFactor * 0.44f);
        float sliceStartX = centerX - (sliceWidth * 0.5f);
        float maxPeakH = (b.getHeight() * 0.62f) * (1.0f - depthFactor * 0.48f);

        const auto& sliceData = analyzer.getHistorySlice(z);

        // Generate ultra-smooth points along the spectrum
        points.clear();
        for (int i = 0; i < kCurveResolution; ++i)
        {
            float normPos = static_cast<float>(i) / static_cast<float>(kCurveResolution - 1);
            float amp = interpolateBand(sliceData, normPos);

            float vx = sliceStartX + normPos * sliceWidth;
            float vy = sliceY - amp * maxPeakH;
            points.push_back({ vx, vy });
        }

        // Build continuous cubic spline path
        juce::Path crestPath;
        buildSmoothSpline(crestPath, points);

        juce::Path fillPath = crestPath;
        fillPath.lineTo(sliceStartX + sliceWidth, sliceY);
        fillPath.lineTo(sliceStartX, sliceY);
        fillPath.closeSubPath();

        // ── Depth Shading & Color-Coded Harmonic Intensity Gradients ────────
        float alpha = juce::jmap(1.0f - t, 0.0f, 1.0f, 0.10f, 0.70f);

        if (z == 0) // Front / Instantaneous Harmonic Wavefront
        {
            auto frontFill = juce::ColourGradient::vertical(
                juce::Colour(0xFF00E5FF).withAlpha(0.75f), sliceY - maxPeakH,
                juce::Colour(0xFF0A1832).withAlpha(0.15f), sliceY);
            frontFill.addColour(0.30, juce::Colour(0xFFFFD54F).withAlpha(0.60f)); // Harmonic intensity flare
            frontFill.addColour(0.65, juce::Colour(0xFF00B4D8).withAlpha(0.40f));

            g.setGradientFill(frontFill);
            g.fillPath(fillPath);

            // Ethereal outer glow halo
            g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.40f));
            g.strokePath(crestPath, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Sharp crystalline white crest
            g.setColour(juce::Colour(0xFFFFFFFF));
            g.strokePath(crestPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else // Receding Depth Slices
        {
            auto depthFill = juce::ColourGradient::vertical(
                juce::Colour(0xFF00ACC1).withAlpha(alpha * 0.85f), sliceY - maxPeakH * 0.7f,
                juce::Colour(0xFF080D18).withAlpha(0.04f), sliceY);
            depthFill.addColour(0.45, juce::Colour(0xFF1E88E5).withAlpha(alpha * 0.40f));

            g.setGradientFill(depthFill);
            g.fillPath(fillPath);

            // Smooth luminous crest
            g.setColour(juce::Colour(0xFF26C6DA).withAlpha(alpha * 0.90f));
            g.strokePath(crestPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
}

//==============================================================================
// Mode 2: Real-Time High-Resolution Spectral Aurora with Formant Holograms
// Smooth 192-point Catmull-Rom spline with high-density laser needles and floating HUD chips
//==============================================================================
void WaveVisualizer::drawAuroraRTA (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(14.f, 8.f);
    const auto& analyzer = m_proc.getSpectralAnalyzer();
    const auto& spectrum = analyzer.getCurrentSpectrum();
    const int kNumBeams = 96; // High density Aurora laser needles
    const int kCurveResolution = 192; // Ultra-smooth continuous spline

    float baseY = b.getBottom() - 16.f;
    float maxH  = b.getHeight() - 65.f;
    float totalW = b.getWidth();
    float beamW = totalW / static_cast<float>(kNumBeams);

    // 1. High-Density Vertical Aurora Laser Needles
    for (int k = 0; k < kNumBeams; ++k)
    {
        float normPos = static_cast<float>(k) / static_cast<float>(kNumBeams - 1);
        float amp = interpolateBand(spectrum, normPos);
        if (amp < 0.003f) continue;

        float colX = b.getX() + k * beamW;
        float h = amp * maxH;
        float topY = baseY - h;

        // Radiant multi-stop Aurora vertical beam gradient
        auto beamGrad = juce::ColourGradient::vertical(
            juce::Colour(0xFFFFD54F).withAlpha(amp * 0.85f), topY,
            juce::Colour(0xFF00E5FF).withAlpha(amp * 0.15f), baseY);
        beamGrad.addColour(0.35, juce::Colour(0xFF00E676).withAlpha(amp * 0.65f));
        beamGrad.addColour(0.70, juce::Colour(0xFF00B0FF).withAlpha(amp * 0.40f));

        g.setGradientFill(beamGrad);
        g.fillRect(colX, topY, beamW - 0.6f, h);
    }

    // 2. Continuous Silky Spline Wavefront
    std::vector<juce::Point<float>> points;
    points.reserve(kCurveResolution);

    for (int i = 0; i < kCurveResolution; ++i)
    {
        float normPos = static_cast<float>(i) / static_cast<float>(kCurveResolution - 1);
        float amp = interpolateBand(spectrum, normPos);

        float vx = b.getX() + normPos * totalW;
        float vy = baseY - amp * maxH;
        points.push_back({ vx, vy });
    }

    juce::Path wavePath;
    buildSmoothSpline(wavePath, points);

    // Radiant cyan bloom stroke
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.40f));
    g.strokePath(wavePath, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Diamond crystalline crest line
    g.setColour(juce::Colour(0xFFFFFFFF));
    g.strokePath(wavePath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 3. Acoustic Formant Holographic Target Nodes
    struct FormantInfo { float normPos; const char* name; const char* freq; juce::Colour col; };
    static const FormantInfo formants[4] = {
        { 0.32f, "F1: TAO",  "450Hz",  juce::Colour(0xFFFFB300) }, // Windchest chamber
        { 0.58f, "F2: PIPE", "1.4kHz", juce::Colour(0xFF00E5FF) }, // Bamboo chimney bore
        { 0.65f, "F(VT)",    "1.8kHz", juce::Colour(0xFFE040FB) }, // Vocal tract coupling
        { 0.74f, "F3: REED", "3.2kHz", juce::Colour(0xFF00E676) }  // Bronze free-reed
    };

    for (const auto& fmt : formants)
    {
        float amp = interpolateBand(spectrum, fmt.normPos);
        float fx = b.getX() + fmt.normPos * totalW;
        float fy = baseY - amp * maxH;

        // Laser crosshair vertical line
        g.setColour(fmt.col.withAlpha(0.35f));
        juce::Path guidePath;
        guidePath.startNewSubPath(fx, b.getY() + 38.f);
        guidePath.lineTo(fx, baseY);
        g.strokePath(guidePath, juce::PathStrokeType(0.9f));

        // Luminous diamond target reticle on the wave
        float dSize = 4.5f;
        juce::Path diamond;
        diamond.startNewSubPath(fx, fy - dSize);
        diamond.lineTo(fx + dSize, fy);
        diamond.lineTo(fx, fy + dSize);
        diamond.lineTo(fx - dSize, fy);
        diamond.closeSubPath();

        g.setColour(fmt.col);
        g.fillPath(diamond);

        // Glass HUD tag badge
        juce::Rectangle<float> tagRect(fx - 25.f, b.getY() + 37.f, 50.f, 15.f);
        g.setColour(juce::Colour(0xB00D101A));
        g.fillRoundedRectangle(tagRect, 3.5f);
        g.setColour(fmt.col.withAlpha(0.7f));
        g.drawRoundedRectangle(tagRect, 3.5f, 0.9f);

        g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f).withStyle("Bold")));
        g.setColour(fmt.col);
        g.drawText(fmt.name, tagRect, juce::Justification::centred, false);
    }
}

//==============================================================================
// Mode 3: Aero-Vectorial Mode (Left side of reference)
// 2D Wireframe silhouettes of 16 bamboo pipes with dynamic neon inner core glow
// and acoustic vertex displacement / vibration wobble modulated by Stiffness & Resonance.
//==============================================================================
void WaveVisualizer::drawAeroVectorial (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(16.f, 8.f);
    const auto& analyzer = m_proc.getSpectralAnalyzer();
    const auto& pipeAmps = analyzer.getPipeAmplitudes();

    static const char* kPipeNotes[16] = {
        "A3", "C4", "D4", "E4", "G4", "A4", "B4", "C5",
        "D5", "E5", "F#5", "G5", "A5", "B5", "C6", "D6"
    };

    // Authentic Khaen Pipe Length Ratios
    static const float pipeLengthRatios[16] = {
        0.95f, 0.88f, 0.82f, 0.77f, 0.72f, 0.66f, 0.61f, 0.56f,
        0.52f, 0.47f, 0.43f, 0.39f, 0.35f, 0.32f, 0.29f, 0.26f
    };

    float baseY    = b.getBottom() - 20.f;
    float maxPipeH = b.getHeight() - 72.f;

    // 8 Columns Layout
    float availableW = b.getWidth() - 90.f;
    float spacing    = availableW / 7.0f;
    float startX     = b.getCentreX() - (3.5f * spacing);
    float basePipeRadius = std::min(10.5f, spacing * 0.22f);

    // DSP Parameters for dynamic animation
    auto getParamSafe = [this](const char* paramId, float defaultVal) -> float {
        if (auto* p = m_proc.getAPVTS().getRawParameterValue(paramId))
            return *p;
        return defaultVal;
    };

    float stiffness   = getParamSafe("reedStiffness", 0.65f);
    float resonance   = getParamSafe("boreReflection", 0.95f);
    float breathSpeed = getParamSafe("breathSpeed", 1.20f);

    // ── 0. Top Mode Title Watermark ──────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.85f));
    g.drawText("AERO-VECTORIAL VIEW (Press. Dynamics)", juce::Rectangle<float>(b.getX() + 4.f, b.getY() + 38.f, 260.f, 15.f), juce::Justification::left, false);

    // ── 1. Base Horizontal Airflow Manifold ──────────────────────────────────
    g.setColour(juce::Colour(0x3000E5FF));
    g.drawHorizontalLine(static_cast<int>(baseY + 3.0f), startX - 25.0f, startX + 7.0f * spacing + 25.0f);
    g.setColour(juce::Colour(0x6000E5FF));
    g.drawHorizontalLine(static_cast<int>(baseY), startX - 20.0f, startX + 7.0f * spacing + 20.0f);

    // ── 2. Render 16 Wireframe Bamboo Pipes in Staggered Pairs ───────────────
    // Row 0 = Back Row (Pipes 8..15), Row 1 = Front Row (Pipes 0..7)
    for (int row = 0; row < 2; ++row)
    {
        float rowOffsetY = (row == 0) ? -10.0f : 0.0f;
        float rowOffsetX = (row == 0) ? spacing * 0.32f : 0.0f;
        float pipeRadius = (row == 0) ? (basePipeRadius * 0.85f) : basePipeRadius;
        float rowAlpha   = (row == 0) ? 0.65f : 1.0f;

        for (int col = 0; col < 8; ++col)
        {
            int p = (row == 0) ? (col + 8) : col;
            float amp = pipeAmps[static_cast<size_t>(p)];
            bool isActive = (amp > 0.020f);

            float cx = startX + col * spacing + rowOffsetX;
            float cy = baseY + rowOffsetY;
            float pipeH = maxPipeH * pipeLengthRatios[p];
            float topY = cy - pipeH;

            // ── Dynamic Vibration Wobble (Vertex Displacement) ───────────────
            // Bound to Stiffness (stiffer = less deflection) and Resonance (higher = wider oscillation)
            float wobbleAmp = isActive ? (amp * (1.20f - stiffness * 0.75f) * (0.6f + resonance * 1.5f) * 3.5f) : 0.0f;

            // Compute outline points with sinusoidal displacement
            const int kNumSlices = 14;
            std::vector<juce::Point<float>> leftWall;
            std::vector<juce::Point<float>> rightWall;
            leftWall.reserve(kNumSlices + 1);
            rightWall.reserve(kNumSlices + 1);

            for (int s = 0; s <= kNumSlices; ++s)
            {
                float fract = static_cast<float>(s) / static_cast<float>(kNumSlices);
                float y = topY + fract * pipeH;

                float displacement = 0.0f;
                if (wobbleAmp > 0.001f)
                {
                    displacement = std::sin(m_animationPhase * 5.0f + (cy - y) * 0.05f + static_cast<float>(p) * 1.3f) * wobbleAmp;
                }

                leftWall.push_back({ cx - pipeRadius + displacement, y });
                rightWall.push_back({ cx + pipeRadius + displacement, y });
            }

            // ── Dynamic Inner Neon Core Glow ─────────────────────────────────
            // Bound to Breath Speed and Note Amplitude
            if (isActive)
            {
                float coreW = pipeRadius * (0.40f + breathSpeed * 0.50f) * std::min(1.0f, amp * 1.6f);
                float coreH = pipeH * std::min(1.0f, 0.25f + amp * 0.75f);
                float coreTopY = cy - coreH;

                juce::Rectangle<float> coreRect(cx - coreW, coreTopY, coreW * 2.0f, coreH);

                auto coreGrad = juce::ColourGradient::vertical(
                    juce::Colour(0xFF00E5FF).withAlpha(amp * 0.90f * rowAlpha), coreTopY,
                    juce::Colour(0xFFFF9100).withAlpha(amp * 0.95f * rowAlpha), cy);
                coreGrad.addColour(0.35, juce::Colour(0xFFFFD54F).withAlpha(amp * 0.85f * rowAlpha));

                g.setGradientFill(coreGrad);
                g.fillRoundedRectangle(coreRect, 3.0f);

                // Top Aperture Radiant Neon Flare
                float flareR = pipeRadius * (1.2f + amp * 1.5f);
                auto flareGrad = juce::ColourGradient(
                    juce::Colour(0xFF00E5FF).withAlpha(amp * 0.80f * rowAlpha), cx, topY,
                    juce::Colour(0x0000E5FF), cx, topY - flareR, true);
                flareGrad.addColour(0.35, juce::Colour(0xFFFFD54F).withAlpha(amp * 0.60f * rowAlpha));
                g.setGradientFill(flareGrad);
                g.fillEllipse(cx - flareR, topY - flareR * 0.5f, flareR * 2.0f, flareR);
            }

            // ── Wireframe Silhouette Vector Paths ────────────────────────────
            juce::Path pipeOutline;
            pipeOutline.startNewSubPath(leftWall[0]);
            for (size_t s = 1; s < leftWall.size(); ++s)
                pipeOutline.lineTo(leftWall[s]);
            pipeOutline.lineTo(rightWall.back());
            for (int s = static_cast<int>(rightWall.size()) - 1; s >= 0; --s)
                pipeOutline.lineTo(rightWall[static_cast<size_t>(s)]);
            pipeOutline.closeSubPath();

            // Background fill for pipe body
            g.setColour(juce::Colour(0xFF0C101A).withAlpha(0.70f * rowAlpha));
            g.fillPath(pipeOutline);

            // Wireframe stroke
            if (isActive)
            {
                // Active luminous neon glow outline
                g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.35f * rowAlpha));
                g.strokePath(pipeOutline, juce::PathStrokeType(3.0f));
                g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.90f * rowAlpha));
                g.strokePath(pipeOutline, juce::PathStrokeType(1.2f));
            }
            else
            {
                // Inactive cyber-wireframe line
                g.setColour(juce::Colour(0x4000E5FF).withAlpha(0.40f * rowAlpha));
                g.strokePath(pipeOutline, juce::PathStrokeType(1.0f));
            }

            // ── Bamboo Segments & Technical Lattice Wireframe Lines ──────────
            for (float nodeRatio : { 0.28f, 0.55f, 0.78f })
            {
                int sIdx = static_cast<int>(nodeRatio * kNumSlices);
                auto lp = leftWall[static_cast<size_t>(sIdx)];
                auto rp = rightWall[static_cast<size_t>(sIdx)];

                // Horizontal wireframe joint ring
                g.setColour(isActive ? juce::Colour(0xFFFFD54F).withAlpha(0.85f * rowAlpha) : juce::Colour(0x3500E5FF).withAlpha(rowAlpha));
                g.drawLine(lp.x, lp.y, rp.x, rp.y, 1.0f);

                // Subtle diagonal cross-lattice wireframe
                if (sIdx > 2)
                {
                    auto prevL = leftWall[static_cast<size_t>(sIdx - 2)];
                    auto prevR = rightWall[static_cast<size_t>(sIdx - 2)];
                    g.setColour(juce::Colour(0x1800E5FF).withAlpha(rowAlpha));
                    g.drawLine(prevL.x, prevL.y, rp.x, rp.y, 0.8f);
                    g.drawLine(prevR.x, prevR.y, lp.x, lp.y, 0.8f);
                }
            }

            // ── Top Vector Aperture Ring ─────────────────────────────────────
            float topEllipseH = pipeRadius * 0.50f;
            g.setColour(isActive ? juce::Colour(0xFF00E5FF).withAlpha(0.95f * rowAlpha) : juce::Colour(0x4000E5FF).withAlpha(rowAlpha));
            g.drawEllipse(cx - pipeRadius, topY - topEllipseH * 0.5f, pipeRadius * 2.0f, topEllipseH, 1.2f);

            // ── Front Row Pitch Labels ───────────────────────────────────────
            if (row == 1)
            {
                juce::Rectangle<float> lblRect(cx - spacing * 0.48f, cy + 5.f, spacing * 0.96f, 13.f);
                if (isActive)
                {
                    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold")));
                    g.setColour(juce::Colour(0xFFFFD54F));
                }
                else
                {
                    g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
                    g.setColour(juce::Colour(0xFF6E748B));
                }
                g.drawText(kPipeNotes[p], lblRect, juce::Justification::centred, false);
            }
        }
    }
}

//==============================================================================
// Mode 4: Reed Matrix Mode (Right side of reference)
// Isometric 2D grid of diamond/rhombus-shaped nodes representing the physical
// Khaen windchest (Tao) layout with pulsating air pressure expansion halos.
//==============================================================================
void WaveVisualizer::drawReedMatrix (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(16.f, 8.f);
    const auto& analyzer = m_proc.getSpectralAnalyzer();
    const auto& pipeAmps = analyzer.getPipeAmplitudes();

    static const char* kPipeNotes[16] = {
        "A3", "C4", "D4", "E4", "G4", "A4", "B4", "C5",
        "D5", "E5", "F#5", "G5", "A5", "B5", "C6", "D6"
    };

    // ── 0. Top Mode Title Watermark ──────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
    g.setColour(juce::Colour(0xFFFFB300).withAlpha(0.85f));
    g.drawText("REED MATRIX VIEW (Topological Flow)", juce::Rectangle<float>(b.getX() + 4.f, b.getY() + 38.f, 260.f, 15.f), juce::Justification::left, false);

    // ── 1. Isometric Grid Geometry ───────────────────────────────────────────
    // 4x4 Diamond Matrix Layout
    float gridCenterX = b.getCentreX();
    float gridCenterY = b.getCentreY() + 14.0f;
    float availableW  = b.getWidth() - 90.0f;
    float availableH  = b.getHeight() - 70.0f;
    float dx = std::min(54.0f, availableW / 6.2f); // Isometric X step
    float dy = std::min(27.0f, availableH / 6.2f); // Isometric Y step

    float rw = dx * 0.52f; // Rhombus half-width
    float rh = dy * 0.56f; // Rhombus half-height

    struct MatrixNode {
        int index;
        int row;
        int col;
        float cx;
        float cy;
        float amp;
        juce::Colour color;
    };

    std::array<MatrixNode, 16> nodes;

    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            int idx = r * 4 + c;
            float ncx = gridCenterX + (c - r) * dx;
            float ncy = gridCenterY + (c + r - 3.0f) * dy;
            float amp = pipeAmps[static_cast<size_t>(idx)];

            // Color Palette Mapping by Octave & Register
            juce::Colour noteCol;
            if (idx < 4)
            {
                // Low / Drone (Octave 3: A3, C4, D4, E4) -> Turquoise Cyan & Emerald
                noteCol = juce::Colour(0xFF00E5FF);
            }
            else if (idx < 10)
            {
                // Mid / Vocal Register (Octave 4: G4, A4, B4, C5, D5, E5) -> Solar Amber & Gold
                noteCol = juce::Colour(0xFFFFB300);
            }
            else
            {
                // High / Lead Register (Octave 5+: F#5, G5, A5, B5, C6, D6) -> Vivid Neon Purple / Orchid
                noteCol = juce::Colour(0xFFE040FB);
            }

            nodes[static_cast<size_t>(idx)] = { idx, r, c, ncx, ncy, amp, noteCol };
        }
    }

    // ── 2. Interconnecting Vector Network (Airflow Routing Lines) ────────────
    for (const auto& n : nodes)
    {
        // Connect to right neighbor in matrix (c + 1)
        if (n.col < 3)
        {
            const auto& rightNeighbor = nodes[static_cast<size_t>(n.row * 4 + (n.col + 1))];
            bool isBusActive = (n.amp > 0.020f || rightNeighbor.amp > 0.020f);

            g.setColour(isBusActive ? juce::Colour(0x6000E5FF) : juce::Colour(0x20304260));
            g.drawLine(n.cx, n.cy, rightNeighbor.cx, rightNeighbor.cy, isBusActive ? 1.4f : 0.8f);
        }

        // Connect to down neighbor in matrix (r + 1)
        if (n.row < 3)
        {
            const auto& downNeighbor = nodes[static_cast<size_t>((n.row + 1) * 4 + n.col)];
            bool isBusActive = (n.amp > 0.020f || downNeighbor.amp > 0.020f);

            g.setColour(isBusActive ? juce::Colour(0x6000E5FF) : juce::Colour(0x20304260));
            g.drawLine(n.cx, n.cy, downNeighbor.cx, downNeighbor.cy, isBusActive ? 1.4f : 0.8f);
        }
    }

    // ── 3. Render Diamond Nodes (Back-to-Front in isometric depth order) ─────
    // Sort nodes by cy for clean painter's algorithm rendering
    std::array<size_t, 16> renderOrder;
    for (size_t i = 0; i < 16; ++i) renderOrder[i] = i;
    std::sort(renderOrder.begin(), renderOrder.end(), [&nodes](size_t a, size_t b) {
        return nodes[a].cy < nodes[b].cy;
    });

    for (size_t idx : renderOrder)
    {
        const auto& n = nodes[idx];
        bool isActive = (n.amp > 0.020f);

        // ── 3A. Pulsating Soft Radial Air Pressure Expansion Halo ────────────
        // Rendered behind the active node to represent airflow expansion
        if (isActive)
        {
            float pulse = 0.15f * std::sin(m_animationPhase * 4.0f + static_cast<float>(n.index) * 0.9f);
            float haloR = (rw * 1.8f) * (1.0f + n.amp * 1.4f + pulse);

            auto haloGrad = juce::ColourGradient(
                n.color.withAlpha(n.amp * 0.65f), n.cx, n.cy,
                n.color.withAlpha(0.0f), n.cx, n.cy - haloR, true);
            haloGrad.addColour(0.35, n.color.withAlpha(n.amp * 0.35f));

            g.setGradientFill(haloGrad);
            g.fillEllipse(n.cx - haloR, n.cy - haloR * 0.60f, haloR * 2.0f, haloR * 1.20f);
        }

        // ── 3B. Rhombus Geometry (Diamond Vector Path) ───────────────────────
        juce::Path rhombus;
        rhombus.startNewSubPath(n.cx, n.cy - rh);      // Top
        rhombus.lineTo(n.cx + rw, n.cy);               // Right
        rhombus.lineTo(n.cx, n.cy + rh);               // Bottom
        rhombus.lineTo(n.cx - rw, n.cy);               // Left
        rhombus.closeSubPath();

        // 3D Isometric Drop Bevel / Elevation Base
        juce::Path bevel;
        bevel.startNewSubPath(n.cx - rw, n.cy);
        bevel.lineTo(n.cx, n.cy + rh);
        bevel.lineTo(n.cx + rw, n.cy);
        bevel.lineTo(n.cx + rw, n.cy + 3.5f);
        bevel.lineTo(n.cx, n.cy + rh + 3.5f);
        bevel.lineTo(n.cx - rw, n.cy + 3.5f);
        bevel.closeSubPath();

        g.setColour(juce::Colour(0xFF090D16));
        g.fillPath(bevel);

        // ── 3C. Tile Face Fill & Lighting ────────────────────────────────────
        if (isActive)
        {
            // Illuminated solid center
            auto tileGrad = juce::ColourGradient::vertical(
                n.color.withAlpha(0.85f), n.cy - rh,
                juce::Colour(0xFF0C1420), n.cy + rh);
            tileGrad.addColour(0.40, n.color.withAlpha(0.60f));
            g.setGradientFill(tileGrad);
            g.fillPath(rhombus);

            // Radiant Neon Border
            g.setColour(n.color);
            g.strokePath(rhombus, juce::PathStrokeType(1.6f));

            // Central Luminous Jewel / Button
            float jewelR = 5.5f;
            auto jewelGrad = juce::ColourGradient(
                juce::Colour(0xFFFFFFFF), n.cx, n.cy,
                n.color, n.cx + jewelR, n.cy, true);
            g.setGradientFill(jewelGrad);
            g.fillEllipse(n.cx - jewelR, n.cy - jewelR * 0.65f, jewelR * 2.0f, jewelR * 1.3f);
        }
        else
        {
            // Inactive dark glass slate
            g.setColour(juce::Colour(0xFF131724));
            g.fillPath(rhombus);

            g.setColour(juce::Colour(0x3500E5FF));
            g.strokePath(rhombus, juce::PathStrokeType(0.9f));
        }

        // ── 3D. Pitch & Note Badge ───────────────────────────────────────────
        if (isActive)
        {
            // Floating Neon Pitch Tag above node
            juce::Rectangle<float> tagRect(n.cx - 16.0f, n.cy - rh - 13.0f, 32.0f, 12.0f);
            g.setColour(juce::Colour(0xE00A0E18));
            g.fillRoundedRectangle(tagRect, 3.0f);
            g.setColour(n.color.withAlpha(0.80f));
            g.drawRoundedRectangle(tagRect, 3.0f, 0.8f);

            g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f).withStyle("Bold")));
            g.setColour(n.color.brighter(0.2f));
            g.drawText(kPipeNotes[n.index], tagRect, juce::Justification::centred, false);
        }
        else
        {
            // Dim note label inside node
            g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f)));
            g.setColour(juce::Colour(0xFF5A6075));
            g.drawText(kPipeNotes[n.index], juce::Rectangle<float>(n.cx - 15.0f, n.cy - 5.0f, 30.0f, 10.0f), juce::Justification::centred, false);
        }
    }
}

//==============================================================================
void WaveVisualizer::drawRmsBar (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float rms  = m_proc.getOutputRms();
    float barW = juce::jlimit(0.f, b.getWidth() - 16.f, rms * (b.getWidth() - 16.f) * 2.2f);
    float barH = 2.0f;
    float barY = b.getBottom() - 4.f;

    // Track
    g.setColour(juce::Colour(0xFF161824));
    g.fillRoundedRectangle(b.getX() + 8.f, barY, b.getWidth() - 16.f, barH, 1.f);

    // Active RMS glow gradient
    if (barW > 1.f)
    {
        auto rmsGrad = juce::ColourGradient::horizontal(
            juce::Colour(0xFF00E5FF), b.getX() + 8.f,
            juce::Colour(0xFFFFB300), b.getX() + 8.f + barW);
        g.setGradientFill(rmsGrad);
        g.fillRoundedRectangle(b.getX() + 8.f, barY, barW, barH, 1.f);
    }
}

//==============================================================================
void WaveVisualizer::drawNoteHud (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float badgeW = 105.f;
    float badgeH = 23.f;
    juce::Rectangle<float> badgeRect (b.getRight() - badgeW - 10.f, b.getY() + 8.f, badgeW, badgeH);

    g.setColour(juce::Colour(0x8010121C));
    g.fillRoundedRectangle(badgeRect, 5.f);

    int note = static_cast<int>(std::round(m_proc.getActiveNoteNumber()));
    bool isActive = m_proc.isInstrumentActive();

    if (!isActive || note < 0)
    {
        g.setColour(juce::Colour(0x406E748B));
        g.drawRoundedRectangle(badgeRect, 5.f, 1.f);

        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        g.setColour(juce::Colour(0xFF5A6075));
        g.drawText("NOTE TRACKER", badgeRect, juce::Justification::centred, false);
        return;
    }

    static const char* kNotes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int oct = (note / 12) - 1;
    int semi = note % 12;
    float freq = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);

    g.setColour(juce::Colour(0x6000E5FF));
    g.drawRoundedRectangle(badgeRect, 5.f, 1.f);

    g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
    g.setColour(juce::Colour(0xFFE0F7FA));
    juce::String txt = juce::String(kNotes[semi]) + juce::String(oct) + "  " + juce::String(static_cast<int>(std::round(freq))) + " Hz";
    g.drawText(txt, badgeRect, juce::Justification::centred, false);
}

//==============================================================================
void WaveVisualizer::drawBreathHud (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    float badgeW = 160.f;
    float badgeH = 23.f;
    juce::Rectangle<float> badgeRect (b.getX() + 10.f, b.getY() + 8.f, badgeW, badgeH);

    g.setColour(juce::Colour(0x8010121C));
    g.fillRoundedRectangle(badgeRect, 5.f);

    bool isActive = m_proc.isInstrumentActive();
    float breathPhase = m_proc.getBreathPhase();
    bool isInhale = m_proc.isBreathInhale();
    bool isSustainedSuction = FreeReedModel::hasSustainedBassNote();
    float ksFlash = m_proc.getKeyswitchPulseFlash();
    int lastKs = m_proc.getLastKeyswitchNote();

    if (!isActive)
    {
        juce::Colour restColor = juce::Colour(0xFF6E748B);
        g.setColour(restColor.withAlpha(0.35f));
        g.drawRoundedRectangle(badgeRect, 5.f, 1.f);

        float dotRadius = 3.f;
        g.setColour(restColor.withAlpha(0.5f));
        g.fillEllipse(badgeRect.getX() + 10.f - dotRadius, badgeRect.getCentreY() - dotRadius, dotRadius * 2.f, dotRadius * 2.f);

        g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
        g.setColour(restColor.brighter(0.25f));
        g.drawText("REST", badgeRect.withTrimmedLeft(20.f).withTrimmedRight(6.f), juce::Justification::centredLeft, false);
        return;
    }

    // Dynamic Color Theme: Solar Amber (#FFB300) for EXHALE (Blow), Cyan (#00E5FF) for INHALE (Draw)
    juce::Colour themeColor = isInhale ? juce::Colour(0xFF00E5FF) : juce::Colour(0xFFFFB300);
    g.setColour(themeColor.withAlpha(isSustainedSuction ? 0.80f : (ksFlash > 0.05f ? 0.70f : 0.45f)));
    g.drawRoundedRectangle(badgeRect, 5.f, 1.f);

    float dotRadius = isSustainedSuction ? 4.f : 3.5f;
    float dotCenterX = badgeRect.getX() + 10.f;
    float dotCenterY = badgeRect.getCentreY();
    g.setColour(themeColor);
    g.fillEllipse(dotCenterX - dotRadius, dotCenterY - dotRadius, dotRadius * 2.f, dotRadius * 2.f);

    if (ksFlash > 0.01f)
    {
        float ringRadius = dotRadius + 5.f * (1.0f - ksFlash);
        g.setColour(themeColor.withAlpha(ksFlash * 0.85f));
        g.drawEllipse(dotCenterX - ringRadius, dotCenterY - ringRadius, ringRadius * 2.f, ringRadius * 2.f, 1.2f);
    }

    g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
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

    g.drawText(modeStr, badgeRect.withTrimmedLeft(20.f).withTrimmedRight(6.f), juce::Justification::centredLeft, false);

    // Dynamic Respiratory Gauge Bar
    float normPhase = breathPhase / juce::MathConstants<float>::twoPi;
    normPhase = juce::jlimit(0.f, 1.f, normPhase);
    float gaugeW = (badgeW - 16.f) * (isSustainedSuction ? 1.0f : normPhase);
    g.setColour(themeColor.withAlpha(0.85f));
    g.fillRoundedRectangle(badgeRect.getX() + 8.f, badgeRect.getBottom() - 3.0f, gaugeW, 1.5f, 1.f);
}
