#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../MIDI/MidiCCManager.h"
#include <functional>

//==============================================================================
/**
 * KhaenKnob — Custom Vector Knob with Copper/Gold Aesthetic and MIDI Learn
 *
 * Design language:
 *  - Dark background (#0A0A0F) with copper arc (#C8965A) for active range
 *  - Subtle glow pulse when hovered (animated opacity)
 *  - Right-click MIDI Learn context menu
 *  - Visual MIDI CC badge (e.g. "CC 70")
 *  - Dynamic pulsing animation during MIDI Learn mode
 */
class KhaenKnob : public juce::Slider,
                  public MidiCCManager::Listener
{
public:
    enum class Style { Copper, Gold, Ghost };

    explicit KhaenKnob (Style style = Style::Copper);
    ~KhaenKnob() override;

    void setStyle (Style s) { m_style = s; repaint(); }

    void setMidiBinding (MidiCCManager* mgr, const juce::String& paramID, const juce::String& displayName = {});
    void setOpenMidiManagerCallback (std::function<void()> cb) { m_openMidiManagerCb = std::move(cb); }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

    // Override label display
    juce::String getTextFromValue (double value) override;

    // MidiCCManager::Listener overrides
    void midiMappingsChanged() override;
    void midiLearnStateChanged (const juce::String& paramID, bool isLearning) override;

private:
    void drawKnobBody    (juce::Graphics& g, juce::Rectangle<float> bounds, float normVal);
    void drawArc         (juce::Graphics& g, juce::Rectangle<float> bounds, float normVal);
    void drawPointer     (juce::Graphics& g, juce::Rectangle<float> bounds, float normVal);
    void drawGlow        (juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawLearnPulse  (juce::Graphics& g, juce::Rectangle<float> bounds);

    void showContextMenu();

    Style m_style       = Style::Copper;
    float m_hoverAlpha  = 0.f;
    bool  m_hovered     = false;

    // MIDI binding & learning
    MidiCCManager* m_midiCCManager = nullptr;
    juce::String   m_paramID;
    juce::String   m_displayName;
    std::function<void()> m_openMidiManagerCb;

    float m_learnAnimPhase = 0.f;

    // Timer-driven hover and MIDI learn pulse animation
    class KnobAnimTimer : public juce::Timer
    {
    public:
        KhaenKnob* owner = nullptr;
        void timerCallback() override;
    } m_animTimer;

    // Colours per style
    juce::Colour getActiveColour() const;
    juce::Colour getInactiveColour() const;
};

//==============================================================================
/**
 * LabelledKnob — KhaenKnob with integrated label and MIDI Learn binding
 */
class LabelledKnob : public juce::Component
{
public:
    KhaenKnob knob;
    juce::Label label;

    LabelledKnob (const juce::String& labelText,
                  KhaenKnob::Style style = KhaenKnob::Style::Copper);

    void setMidiBinding (MidiCCManager* mgr, const juce::String& paramID)
    {
        knob.setMidiBinding(mgr, paramID, label.getText());
    }

    void setOpenMidiManagerCallback (std::function<void()> cb)
    {
        knob.setOpenMidiManagerCallback(std::move(cb));
    }

    void resized() override;
};
