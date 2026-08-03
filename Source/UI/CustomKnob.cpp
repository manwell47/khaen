#include "CustomKnob.h"

//==============================================================================
// Khaen colour palette
namespace KhaenColors
{
    static const juce::Colour bg         = juce::Colour(0xFF0A0A0F);
    static const juce::Colour panel      = juce::Colour(0xFF1A1A24);
    static const juce::Colour copper     = juce::Colour(0xFFC8965A);
    static const juce::Colour copperDim  = juce::Colour(0xFF8B6914);
    static const juce::Colour gold       = juce::Colour(0xFFE8C97A);
    static const juce::Colour ghost      = juce::Colour(0xFF6A7A9A);
    static const juce::Colour textPrim   = juce::Colour(0xFFDDDDDD);
    static const juce::Colour textDim    = juce::Colour(0xFF666677);
}

//==============================================================================
KhaenKnob::KhaenKnob (Style style)
    : juce::Slider(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox),
      m_style(style)
{
    m_hoverTimer.owner = this;
    setVelocityBasedMode(false);
}

//==============================================================================
void KhaenKnob::HoverTimer::timerCallback()
{
    if (!owner) return;

    float target = owner->m_hovered ? 1.f : 0.f;
    owner->m_hoverAlpha += (target - owner->m_hoverAlpha) * 0.12f;

    if (std::abs(owner->m_hoverAlpha - target) < 0.01f)
    {
        owner->m_hoverAlpha = target;
        if (!owner->m_hovered) stopTimer();
    }
    owner->repaint();
}

void KhaenKnob::mouseEnter (const juce::MouseEvent&)
{
    m_hovered = true;
    m_hoverTimer.startTimerHz(60);
}

void KhaenKnob::mouseExit (const juce::MouseEvent&)
{
    m_hovered = false;
    // timer continues until alpha reaches 0
}

//==============================================================================
juce::Colour KhaenKnob::getActiveColour() const
{
    switch (m_style)
    {
        case Style::Copper: return KhaenColors::copper;
        case Style::Gold:   return KhaenColors::gold;
        case Style::Ghost:  return KhaenColors::ghost;
        default:            return KhaenColors::copper;
    }
}

juce::Colour KhaenKnob::getInactiveColour() const
{
    return KhaenColors::panel.brighter(0.08f);
}

//==============================================================================
juce::String KhaenKnob::getTextFromValue (double value)
{
    if (getMaximum() - getMinimum() <= 1.1)  // 0..1 normalised
        return juce::String(value, 2);
    if (std::abs(getMaximum()) <= 25.f)      // semitones
        return juce::String(value >= 0 ? "+" : "") + juce::String(value, 1);
    if (getMaximum() >= 100.f)              // milliseconds
        return juce::String(static_cast<int>(value)) + " ms";
    return juce::Slider::getTextFromValue(value);
}

//==============================================================================
void KhaenKnob::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(4.f);
    float normVal = static_cast<float>((getValue() - getMinimum()) / (getMaximum() - getMinimum()));

    drawKnobBody(g, bounds, normVal);
    drawArc(g, bounds, normVal);
    drawPointer(g, bounds, normVal);
    if (m_hoverAlpha > 0.01f)
        drawGlow(g, bounds);
}

//==============================================================================
void KhaenKnob::drawKnobBody (juce::Graphics& g,
                               juce::Rectangle<float> bounds, float /*normVal*/)
{
    // Outer dark ring
    float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    float r  = std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    // Subtle radial gradient for depth
    auto grad = juce::ColourGradient::vertical(
        KhaenColors::panel.brighter(0.05f),
        bounds.getY(),
        KhaenColors::bg,
        bounds.getBottom());

    g.setGradientFill(grad);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);

    // Thin border ring
    g.setColour(getActiveColour().withAlpha(0.25f));
    g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.2f);
}

//==============================================================================
void KhaenKnob::drawArc (juce::Graphics& g,
                          juce::Rectangle<float> bounds, float normVal)
{
    float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    float r  = std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;
    float arcR = r * 1.18f;

    static constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.75f;

    // Inactive track
    {
        juce::Path track;
        track.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2,
                     kStartAngle, kEndAngle, true);
        g.setColour(getInactiveColour());
        g.strokePath(track, juce::PathStrokeType(2.5f,
                     juce::PathStrokeType::curved,
                     juce::PathStrokeType::rounded));
    }

    // Active arc (copper/gold)
    float fillAngle = kStartAngle + normVal * (kEndAngle - kStartAngle);
    if (fillAngle > kStartAngle + 0.05f)
    {
        juce::Path fill;
        fill.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2,
                    kStartAngle, fillAngle, true);
        g.setColour(getActiveColour());
        g.strokePath(fill, juce::PathStrokeType(2.5f,
                     juce::PathStrokeType::curved,
                     juce::PathStrokeType::rounded));
    }
}

//==============================================================================
void KhaenKnob::drawPointer (juce::Graphics& g,
                              juce::Rectangle<float> bounds, float normVal)
{
    float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    float r  = std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    static constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
    static constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.75f;
    float angle = kStartAngle + normVal * (kEndAngle - kStartAngle);

    float px = cx + std::sin(angle) * r * 0.65f;
    float py = cy - std::cos(angle) * r * 0.65f;
    float px0 = cx + std::sin(angle) * r * 0.15f;
    float py0 = cy - std::cos(angle) * r * 0.15f;

    // Pointer line
    g.setColour(getActiveColour());
    g.drawLine(px0, py0, px, py, 2.0f);

    // Tip dot
    g.setColour(getActiveColour().brighter(0.3f));
    g.fillEllipse(px - 2.5f, py - 2.5f, 5.f, 5.f);
}

//==============================================================================
void KhaenKnob::drawGlow (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    float r  = std::min(bounds.getWidth(), bounds.getHeight()) * 0.55f;

    auto glow = juce::ColourGradient(
        getActiveColour().withAlpha(0.18f * m_hoverAlpha),
        cx, cy,
        getActiveColour().withAlpha(0.f),
        cx + r, cy, true);

    g.setGradientFill(glow);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
}

//==============================================================================
// LabelledKnob

LabelledKnob::LabelledKnob (const juce::String& labelText, KhaenKnob::Style style)
    : knob(style)
{
    addAndMakeVisible(knob);
    addAndMakeVisible(label);

    label.setText(labelText.toUpperCase(), juce::dontSendNotification);
    label.setFont(juce::Font(10.0f));
    label.setColour(juce::Label::textColourId, KhaenColors::textDim);
    label.setJustificationType(juce::Justification::centred);
}

void LabelledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds(b.removeFromBottom(16));
    knob.setBounds(b);
}
