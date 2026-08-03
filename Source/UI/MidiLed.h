#pragma once

#include <JuceHeader.h>

class MidiLed : public juce::Component, public juce::Timer
{
public:
    MidiLed()
    {
        startTimerHz(60);
    }

    ~MidiLed() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.38f;
        auto center = bounds.getCentre();

        // Outer glow halo when active
        if (m_brightness > 0.02f)
        {
            juce::Colour amber(0xFFFFB032);
            g.setColour(amber.withAlpha(m_brightness * 0.45f));
            g.fillEllipse(center.x - radius * 1.5f, center.y - radius * 1.5f, radius * 3.0f, radius * 3.0f);
        }

        // Metallic dark socket/bezel
        g.setColour(juce::Colour(0xFF101018));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        // Inactive glass lens
        g.setColour(juce::Colour(0xFF382410).withAlpha(0.7f));
        g.fillEllipse(center.x - radius * 0.85f, center.y - radius * 0.85f, radius * 1.7f, radius * 1.7f);

        if (m_brightness > 0.01f)
        {
            // Active Amber/Gold illuminated LED
            juce::Colour amberCore(0xFFFFC040);
            g.setColour(amberCore.withAlpha(juce::jmin(1.0f, m_brightness)));
            g.fillEllipse(center.x - radius * 0.85f, center.y - radius * 0.85f, radius * 1.7f, radius * 1.7f);

            // Hot white-hot inner specular flare
            g.setColour(juce::Colours::white.withAlpha(m_brightness * 0.85f));
            g.fillEllipse(center.x - radius * 0.4f, center.y - radius * 0.4f, radius * 0.8f, radius * 0.8f);
        }

        // Copper bezel ring
        juce::Colour copperRing = (m_brightness > 0.1f) ? juce::Colour(0xFFE8C97A) : juce::Colour(0xFF8B6914).withAlpha(0.6f);
        g.setColour(copperRing);
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);
    }

    void timerCallback() override
    {
        if (m_checkActivityCallback && m_checkActivityCallback())
        {
            m_brightness = 1.0f;
        }
        else
        {
            m_brightness *= 0.88f; // Smooth natural phosphor decay
        }

        if (m_brightness > 0.005f || m_wasBright)
        {
            repaint();
        }

        m_wasBright = (m_brightness > 0.005f);
    }

    std::function<bool()> m_checkActivityCallback;

private:
    float m_brightness {0.0f};
    bool m_wasBright {false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLed)
};
