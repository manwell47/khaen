#pragma once

#include <JuceHeader.h>

class MidiLed : public juce::Component, public juce::Timer
{
public:
    MidiLed()
    {
        startTimerHz(30);
    }

    ~MidiLed() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
        auto center = bounds.getCentre();

        // Base dark
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.f, radius * 2.f);

        if (m_brightness > 0.01f)
        {
            // Amber glow
            juce::Colour amber(255, 180, 50);
            g.setColour(amber.withAlpha(m_brightness));
            g.fillEllipse(center.x - radius, center.y - radius, radius * 2.f, radius * 2.f);

            // Inner core glow
            g.setColour(juce::Colours::white.withAlpha(m_brightness * 0.5f));
            g.fillEllipse(center.x - radius*0.5f, center.y - radius*0.5f, radius, radius);
        }
        
        // Ring
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.f, radius * 2.f, 1.5f);
    }

    void timerCallback() override
    {
        if (m_checkActivityCallback && m_checkActivityCallback())
        {
            m_brightness = 1.0f;
        }
        else
        {
            m_brightness *= 0.8f; // Decay
        }

        if (m_brightness > 0.01f || m_wasBright)
        {
            repaint();
        }
        
        m_wasBright = (m_brightness > 0.01f);
    }

    std::function<bool()> m_checkActivityCallback;

private:
    float m_brightness {0.0f};
    bool m_wasBright {false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLed)
};
