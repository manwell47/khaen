#pragma once
#include <JuceHeader.h>

//==============================================================================
/**
 * KhaenKnob — Custom Vector Knob with Copper/Gold Aesthetic
 *
 * Design language:
 *  - Dark background (#0A0A0F) with copper arc (#C8965A) for active range
 *  - Subtle glow pulse when hovered (animated opacity)
 *  - Value display on drag
 *  - Smooth 60fps repaints on parameter change
 */
class KhaenKnob : public juce::Slider
{
public:
    enum class Style { Copper, Gold, Ghost };

    explicit KhaenKnob (Style style = Style::Copper);
    ~KhaenKnob() override = default;

    void setStyle (Style s) { m_style = s; repaint(); }

    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

    // Override label display
    juce::String getTextFromValue (double value) override;

private:
    void drawKnobBody    (juce::Graphics& g, juce::Rectangle<float> bounds, float normVal);
    void drawArc         (juce::Graphics& g, juce::Rectangle<float> bounds, float normVal);
    void drawPointer     ((juce::Graphics& g, juce::Rectangle<float> bounds, float normVal);
    void drawGlow        (juce::Graphics& g, juce::Rectangle<float> bounds);

    Style m_style       = Style::Copper;
    float m_hoverAlpha  = 0.f;
    bool  m_hovered     = false;

    // Timer-driven hover animation
    class HoverTimer : public juce::Timer
    {
    public:
        KhaenKnob* owner = nullptr;
        void timerCallback() override;
    } m_hoverTimer;

    // Colours per style
    juce::Colour getActiveColour() const;
    juce::Colour getInactiveColour() const;
};

//==============================================================================
/**
 * LabelledKnob — KhaenKnob with integrated label
 */
class LabelledKnob : public juce::Component
{
public:
    KhaenKnob knob;
    juce::Label label;

    LabelledKnob (const juce::String& labelText,
                  KhaenKnob::Style style = KhaenKnob::Style::Copper);

    void resized() override;
};
