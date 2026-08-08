#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
 * HelpDialog — Modal Overlay providing an introduction to the Khaen,
 * playing instructions, performance keyswitches, and detailed parameter manual.
 */
class HelpDialog : public juce::Component,
                   public juce::Button::Listener
{
public:
    HelpDialog();
    ~HelpDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setCloseCallback (std::function<void()> cb) { m_closeCallback = std::move(cb); }
    void buttonClicked (juce::Button* b) override;

private:
    void switchSection (int sectionIndex);
    void updateTextContent();

    juce::TextButton m_closeBtn { "X" };

    juce::TextButton m_tabIntro   { "INTRODUCCION" };
    juce::TextButton m_tabPerform { "COMO USAR" };
    juce::TextButton m_tabParams  { "MANUAL PARAMETROS" };
    juce::TextButton m_tabFxVis   { "EFECTOS & VISUALIZADOR" };

    juce::TextEditor m_contentArea;
    int m_activeSection { 0 };

    std::function<void()> m_closeCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpDialog)
};
