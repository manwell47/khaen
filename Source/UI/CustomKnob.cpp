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
    static const juce::Colour cyanGlow   = juce::Colour(0xFF4AC4E2);
}

//==============================================================================
KhaenKnob::KhaenKnob (Style style)
    : juce::Slider(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox),
      m_style(style)
{
    m_animTimer.owner = this;
    setVelocityBasedMode(false);
}

KhaenKnob::~KhaenKnob()
{
    m_animTimer.stopTimer();
    if (m_midiCCManager != nullptr)
        m_midiCCManager->removeListener(this);
}

void KhaenKnob::setMidiBinding (MidiCCManager* mgr, const juce::String& paramID, const juce::String& displayName)
{
    if (m_midiCCManager != nullptr)
        m_midiCCManager->removeListener(this);

    m_midiCCManager = mgr;
    m_paramID = paramID;
    m_displayName = displayName.isNotEmpty() ? displayName : paramID;

    if (m_midiCCManager != nullptr)
        m_midiCCManager->addListener(this);

    repaint();
}

//==============================================================================
void KhaenKnob::KnobAnimTimer::timerCallback()
{
    if (!owner) return;

    float target = owner->m_hovered ? 1.f : 0.f;
    owner->m_hoverAlpha += (target - owner->m_hoverAlpha) * 0.12f;

    bool isLearning = (owner->m_midiCCManager != nullptr && 
                       owner->m_midiCCManager->isLearningParam(owner->m_paramID));

    if (isLearning)
    {
        owner->m_learnAnimPhase += 0.08f;
        if (owner->m_learnAnimPhase > juce::MathConstants<float>::twoPi)
            owner->m_learnAnimPhase -= juce::MathConstants<float>::twoPi;
    }

    if (!isLearning && !owner->m_hovered && std::abs(owner->m_hoverAlpha) < 0.01f)
    {
        owner->m_hoverAlpha = 0.f;
        stopTimer();
    }

    owner->repaint();
}

void KhaenKnob::mouseEnter (const juce::MouseEvent&)
{
    m_hovered = true;
    m_animTimer.startTimerHz(60);
}

void KhaenKnob::mouseExit (const juce::MouseEvent&)
{
    m_hovered = false;
    // timer continues until alpha reaches 0 (or while learning)
}

void KhaenKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showContextMenu();
        return;
    }

    if (m_midiCCManager != nullptr && m_midiCCManager->isLearningParam(m_paramID))
    {
        m_midiCCManager->stopLearning();
        repaint();
        return;
    }

    juce::Slider::mouseDown(e);
}

//==============================================================================
void KhaenKnob::showContextMenu()
{
    if (m_midiCCManager == nullptr || m_paramID.isEmpty())
        return;

    juce::PopupMenu menu;
    int currentCC = m_midiCCManager->getCCForParam(m_paramID);
    bool isCurrentlyLearning = m_midiCCManager->isLearningParam(m_paramID);

    menu.addSectionHeader(m_displayName.toUpperCase() + " - MIDI CC");

    // 1. MIDI Learn Toggle
    if (isCurrentlyLearning)
    {
        menu.addItem(1, "Cancel MIDI Learn");
    }
    else
    {
        menu.addItem(1, "MIDI Learn (Wait for CC...)");
    }

    // 2. Clear CC
    if (currentCC >= 0)
    {
        menu.addItem(2, "Unassign MIDI CC (CC " + juce::String(currentCC) + ")");
    }

    menu.addSeparator();

    // 3. Quick CC Submenu
    juce::PopupMenu ccMenu;
    ccMenu.addItem(101, "CC 1 (Modulation Wheel)",    true, currentCC == 1);
    ccMenu.addItem(102, "CC 2 (Breath Controller)",   true, currentCC == 2);
    ccMenu.addItem(107, "CC 7 (Channel Volume)",       true, currentCC == 7);
    ccMenu.addItem(110, "CC 10 (Pan)",                 true, currentCC == 10);
    ccMenu.addItem(111, "CC 11 (Expression)",          true, currentCC == 11);
    ccMenu.addItem(171, "CC 71 (Resonance / Timbre)",  true, currentCC == 71);
    ccMenu.addItem(174, "CC 74 (Brightness / Cutoff)", true, currentCC == 74);
    ccMenu.addItem(191, "CC 91 (Reverb Send)",         true, currentCC == 91);
    ccMenu.addItem(193, "CC 93 (Delay Send)",          true, currentCC == 93);

    menu.addSubMenu("Assign to CC...", ccMenu);

    menu.addSeparator();

    // 4. MIDI Hardware Presets
    menu.addItem(403, "Standard MIDI Preset (CC 1, 2, 11...)");

    // 5. Open Matrix Dialog
    if (m_openMidiManagerCb)
    {
        menu.addSeparator();
        menu.addItem(500, "Open MIDI Mapping Matrix...");
    }

    auto* mgr = m_midiCCManager;
    auto paramID = m_paramID;
    auto openCb = m_openMidiManagerCb;

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [this, mgr, paramID, openCb](int result)
        {
            if (result == 0 || mgr == nullptr) return;

            if (result == 1)
            {
                if (mgr->isLearningParam(paramID))
                    mgr->stopLearning();
                else
                    mgr->startLearning(paramID);
            }
            else if (result == 2)
            {
                mgr->clearMapping(paramID);
            }
            else if (result >= 100 && result < 200)
            {
                int cc = result - 100;
                mgr->setMapping(paramID, cc, 0);
            }
            else if (result == 403)
            {
                mgr->applyStandardMidiCC();
            }
            else if (result == 500)
            {
                if (openCb) openCb();
            }

            repaint();
        });
}

//==============================================================================
void KhaenKnob::midiMappingsChanged()
{
    if (m_midiCCManager != nullptr && !m_midiCCManager->isLearningParam(m_paramID))
        m_animTimer.stopTimer();
    repaint();
}

void KhaenKnob::midiLearnStateChanged (const juce::String& paramID, bool isLearning)
{
    if (paramID == m_paramID)
    {
        if (isLearning)
            m_animTimer.startTimerHz(60);
        else
            m_animTimer.stopTimer();
        repaint();
    }
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

    bool isLearning = (m_midiCCManager != nullptr && m_midiCCManager->isLearningParam(m_paramID));
    if (isLearning)
    {
        drawLearnPulse(g, bounds);
    }
}

//==============================================================================
void KhaenKnob::drawKnobBody (juce::Graphics& g,
                              juce::Rectangle<float> bounds, float /*normVal*/)
{
    float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    float r  = std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    auto grad = juce::ColourGradient::vertical(
        KhaenColors::panel.brighter(0.05f),
        bounds.getY(),
        KhaenColors::bg,
        bounds.getBottom());

    g.setGradientFill(grad);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);

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

    // Active arc
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

    g.setColour(getActiveColour());
    g.drawLine(px0, py0, px, py, 2.0f);

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
void KhaenKnob::drawLearnPulse (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    float r  = std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    float pulse = 0.5f + 0.5f * std::sin(m_learnAnimPhase);
    float glowR = r * (1.10f + 0.12f * pulse);

    // 1. Soft glowing circular inner aura (integrated seamlessly into knob face)
    auto innerGlow = juce::ColourGradient(
        KhaenColors::cyanGlow.withAlpha(0.22f * pulse + 0.08f),
        cx, cy,
        KhaenColors::cyanGlow.withAlpha(0.0f),
        cx + r * 0.9f, cy, true);
    g.setGradientFill(innerGlow);
    g.fillEllipse(cx - r * 0.9f, cy - r * 0.9f, r * 1.8f, r * 1.8f);

    // 2. Cyan pulsating outer ring
    g.setColour(KhaenColors::cyanGlow.withAlpha(0.40f * pulse + 0.20f));
    g.drawEllipse(cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f, 1.8f);

    // 3. Clean glowing text floating organically without any rectangular box border
    g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
    g.setColour(KhaenColors::cyanGlow.brighter(0.6f).withAlpha(0.80f + 0.20f * pulse));
    g.drawText("LEARN", bounds, juce::Justification::centred, false);
}

//==============================================================================
// LabelledKnob

LabelledKnob::LabelledKnob (const juce::String& labelText, KhaenKnob::Style style)
    : knob(style)
{
    addAndMakeVisible(knob);
    addAndMakeVisible(label);

    label.setText(labelText.toUpperCase(), juce::dontSendNotification);
    label.setFont(juce::Font(9.5f));
    label.setMinimumHorizontalScale(0.65f);
    label.setColour(juce::Label::textColourId, KhaenColors::textDim);
    label.setJustificationType(juce::Justification::centred);
}

void LabelledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds(b.removeFromBottom(16));
    knob.setBounds(b);
}
