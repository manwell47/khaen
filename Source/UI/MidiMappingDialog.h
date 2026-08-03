#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../MIDI/MidiCCManager.h"
#include <vector>

//==============================================================================
/**
 * MidiMappingDialog — Visual MIDI CC Mapping Matrix Overlay
 *
 * Provides a luxury modal interface to inspect, MIDI-learn, clear,
 * and apply presets (including Akai MPK Mini) for all synth parameters.
 */
class MidiMappingDialog : public juce::Component,
                          public MidiCCManager::Listener
{
public:
    explicit MidiMappingDialog (MidiCCManager& midiCCManager);
    ~MidiMappingDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setCloseCallback (std::function<void()> cb) { m_closeCallback = std::move(cb); }

    // MidiCCManager::Listener overrides
    void midiMappingsChanged() override;
    void midiLearnStateChanged (const juce::String& paramID, bool isLearning) override;

private:
    struct ParamItem
    {
        juce::String category;
        juce::String paramID;
        juce::String displayName;
    };

    class RowComponent : public juce::Component
    {
    public:
        RowComponent (MidiMappingDialog& owner, const ParamItem& item);
        void paint (juce::Graphics& g) override;
        void resized() override;
        void updateState();

        const ParamItem item;

    private:
        MidiMappingDialog& m_owner;
        juce::TextButton m_learnBtn { "LEARN" };
        juce::TextButton m_clearBtn { "CLEAR" };
    };

    class ListContentComponent : public juce::Component
    {
    public:
        ListContentComponent (MidiMappingDialog& owner, const std::vector<ParamItem>& items);
        void resized() override;
        void updateAllRows();

        std::vector<std::unique_ptr<RowComponent>> rows;
    };

    void buildParameterList();
    void applyRowStyling();

    //==========================================================================
    MidiCCManager& m_midiCCManager;
    std::function<void()> m_closeCallback;

    std::vector<ParamItem> m_paramItems;

    juce::Label      m_titleLabel;
    juce::TextButton m_closeBtn       { "X" };
    juce::TextButton m_stdMidiBtn     { "Standard MIDI (1, 2, 11...)" };
    juce::TextButton m_clearAllBtn    { "Clear All" };
    juce::TextButton m_saveDefaultBtn { "Save Default" };
    juce::TextButton m_loadDefaultBtn { "Load Default" };

    juce::Viewport m_viewport;
    std::unique_ptr<ListContentComponent> m_content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMappingDialog)
};
