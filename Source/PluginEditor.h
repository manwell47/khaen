#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/CustomKnob.h"
#include "UI/WaveVisualizer.h"
#include "UI/MidiLed.h"

//==============================================================================
/**
 * KhaenEditor — Main Plugin GUI
 *
 * Layout (1280×800, aspect-locked 16:10):
 *
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │  HEADER: Preset Selector | Logo | MIDI Led                  │  60px
 *  ├──────────┬──────────────────────────────────────────────────┤
 *  │  MACROS  │  VISUALIZER  |  PHYSICS PARAMS                  │  500px
 *  │  Air     │  WaveVis     |  Reed Stiff / Bore / Material    │
 *  │  Emboc.  │              |                                   │
 *  │  Reson.  │              |                                   │
 *  ├──────────┴──────────────────────────────────────────────────┤
 *  │  FX TABS: [DELAY] [HARMONIZER] [CRYSTALLIZER]              │  240px
 *  │  (active tab panel content)                                 │
 *  └─────────────────────────────────────────────────────────────┘
 */
class KhaenEditor : public juce::AudioProcessorEditor,
                    private juce::ComboBox::Listener,
                    private juce::Button::Listener
{
public:
    explicit KhaenEditor (KhaenAudioProcessor&);
    ~KhaenEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void switchTab (int tabIndex);

private:
    //==========================================================================
    // APVTS attachment helpers
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    //==========================================================================
    void buildHeader();
    void buildMacroPanel();
    void buildPhysicsPanel();
    void buildFxPanel();
    void buildDelayPanel();
    void buildHarmonizerPanel();
    void buildCrystalPanel();
    void buildReverbPanel();

    void paintHeader      (juce::Graphics&, juce::Rectangle<int>, float);
    void paintFxArea      (juce::Graphics&, juce::Rectangle<int>, float);

    void comboBoxChanged (juce::ComboBox*) override;
    void buttonClicked   (juce::Button*)   override;

    //==========================================================================
    KhaenAudioProcessor& m_proc;

    // ── Header ────────────────────────────────────────────────────────────────
    juce::ComboBox    m_presetBox;
    juce::TextButton  m_prevPreset { "<" };
    juce::TextButton  m_nextPreset { ">" };
    MidiLed           m_midiLed;
    juce::Label       m_midiLabel;

    // ── Macro knobs ───────────────────────────────────────────────────────────
    LabelledKnob m_knobAir        { "AIR",        KhaenKnob::Style::Copper };
    LabelledKnob m_knobEmboc      { "EMBOUCHURE", KhaenKnob::Style::Copper };
    LabelledKnob m_knobResonance  { "RESONANCE",  KhaenKnob::Style::Gold   };

    std::unique_ptr<SliderAtt> m_attAir, m_attEmboc, m_attResonance;

    // ── Visualizer ────────────────────────────────────────────────────────────
    WaveVisualizer m_waveVis;

    // ── Physics panel ─────────────────────────────────────────────────────────
    LabelledKnob m_knobStiffness   { "STIFFNESS",    KhaenKnob::Style::Copper };
    LabelledKnob m_knobBore        { "BORE DIA",     KhaenKnob::Style::Gold   };
    LabelledKnob m_knobBreathSpeed { "BREATH SPD",   KhaenKnob::Style::Copper };
    LabelledKnob m_knobSuctionBite { "SUCTION BITE", KhaenKnob::Style::Gold   };
    juce::ComboBox     m_matCombo;
    juce::Label        m_matLabel;
    juce::ToggleButton m_autoRiffBtn { "AUTO-RIFF" };

    std::unique_ptr<SliderAtt> m_attStiffness, m_attBore, m_attBreathSpeed, m_attSuctionBite;
    std::unique_ptr<ComboAtt>  m_attMaterial;
    std::unique_ptr<ButtonAtt> m_attAutoRiff;

    // ── FX Tab bar ────────────────────────────────────────────────────────────
    juce::TextButton m_tabDelay       { "DELAY"        };
    juce::TextButton m_tabHarmonizer  { "HARMONIZER"   };
    juce::TextButton m_tabCrystal     { "CRYSTALLIZER" };
    juce::TextButton m_tabReverb      { "REVERB (LAOS)" };
    int              m_activeTab      = 0;

    // ── Delay panel ───────────────────────────────────────────────────────────
    juce::Component m_delayPanel;
    LabelledKnob    m_knobDelTime  { "TIME",     KhaenKnob::Style::Copper };
    LabelledKnob    m_knobDelFb    { "FEEDBACK", KhaenKnob::Style::Copper };
    LabelledKnob    m_knobDelMix   { "MIX",      KhaenKnob::Style::Gold   };
    LabelledKnob    m_knobDelWow   { "WOW",      KhaenKnob::Style::Ghost  };
    LabelledKnob    m_knobDelFlut  { "FLUTTER",  KhaenKnob::Style::Ghost  };
    juce::ToggleButton m_pingPong       { "PING-PONG" };
    juce::ToggleButton m_delayBypassBtn { "BYPASS" };

    std::unique_ptr<SliderAtt> m_attDelTime, m_attDelFb, m_attDelMix,
                               m_attDelWow,  m_attDelFlut;
    std::unique_ptr<ButtonAtt> m_attPingPong, m_attDelayBypass;

    // ── Harmonizer panel ──────────────────────────────────────────────────────
    juce::Component m_harmPanel;
    LabelledKnob    m_knobHarmShift  { "SHIFT",     KhaenKnob::Style::Copper };
    LabelledKnob    m_knobHarmDet    { "FINE (CT)", KhaenKnob::Style::Ghost  };
    LabelledKnob    m_knobHarmSpread { "SPREAD",    KhaenKnob::Style::Ghost  };
    LabelledKnob    m_knobHarmMix    { "MIX",       KhaenKnob::Style::Gold   };
    juce::ToggleButton m_harmBypassBtn { "BYPASS" };

    std::unique_ptr<SliderAtt> m_attHarmShift, m_attHarmMix,
                               m_attHarmSpread, m_attHarmDet;
    std::unique_ptr<ButtonAtt> m_attHarmBypass;

    // ── Crystallizer panel ────────────────────────────────────────────────────
    juce::Component m_crystalPanel;
    LabelledKnob    m_knobCrysGrain { "GRAIN",    KhaenKnob::Style::Copper };
    LabelledKnob    m_knobCrysDelay { "DELAY",    KhaenKnob::Style::Copper };
    LabelledKnob    m_knobCrysFb    { "FEEDBACK", KhaenKnob::Style::Gold   };
    LabelledKnob    m_knobCrysMix   { "MIX",      KhaenKnob::Style::Gold   };
    LabelledKnob    m_knobCrysPitch { "PITCH",    KhaenKnob::Style::Ghost  };
    juce::ComboBox  m_crystalMode;
    juce::Label     m_crystalModeLabel;
    juce::TextButton   m_freezeBtn     { "FREEZE" };
    juce::ToggleButton m_crysBypassBtn { "BYPASS" };

    std::unique_ptr<SliderAtt> m_attCrysGrain, m_attCrysDelay, m_attCrysFb,
                               m_attCrysMix,   m_attCrysPitch;
    std::unique_ptr<ComboAtt>  m_attCrysMode;
    std::unique_ptr<ButtonAtt> m_attFreeze, m_attCrysBypass;

    // ── Reverb panel (Laos Locations) ─────────────────────────────────────────
    juce::Component m_reverbPanel;
    LabelledKnob    m_knobRevSize   { "ROOM SIZE",   KhaenKnob::Style::Copper };
    LabelledKnob    m_knobRevDecay  { "DECAY(RT60)", KhaenKnob::Style::Copper };
    LabelledKnob    m_knobRevDamp   { "DAMPING",     KhaenKnob::Style::Ghost  };
    LabelledKnob    m_knobRevWidth  { "WIDTH",       KhaenKnob::Style::Ghost  };
    LabelledKnob    m_knobRevMix    { "MIX",         KhaenKnob::Style::Gold   };
    juce::ComboBox  m_reverbPresetCombo;
    juce::Label     m_reverbPresetLabel;
    juce::ToggleButton m_reverbBypassBtn { "BYPASS" };

    std::unique_ptr<SliderAtt> m_attRevSize, m_attRevDecay, m_attRevDamp,
                               m_attRevWidth, m_attRevMix;
    std::unique_ptr<ComboAtt>  m_attRevPreset;
    std::unique_ptr<ButtonAtt> m_attRevBypass;

    // Bounds constrainer for resizable window
    juce::ComponentBoundsConstrainer m_constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KhaenEditor)
};
