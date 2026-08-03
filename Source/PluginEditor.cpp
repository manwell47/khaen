#include "PluginEditor.h"

//==============================================================================
// Colour palette matching luxury copper/bamboo dark aesthetic
namespace
{
    static const juce::Colour kBg         = juce::Colour(0xFF0A0A0F);
    static const juce::Colour kPanel      = juce::Colour(0xFF14141E);
    static const juce::Colour kCopper     = juce::Colour(0xFFC8965A);
    static const juce::Colour kCopperDim  = juce::Colour(0xFF8B6914);
    static const juce::Colour kGold       = juce::Colour(0xFFE8C97A);
    static const juce::Colour kGhost      = juce::Colour(0xFF6A7A9A);
    static const juce::Colour kTextPrim   = juce::Colour(0xFFE0E0E8);
    static const juce::Colour kTextDim    = juce::Colour(0xFF7A7A8C);
    static const juce::Colour kBorder     = juce::Colour(0xFF222230);

    constexpr int kDefaultW = 1280;
    constexpr int kDefaultH = 800;
}

//==============================================================================
KhaenEditor::KhaenEditor (KhaenAudioProcessor& p)
    : AudioProcessorEditor(p),
      m_proc(p),
      m_waveVis(p)
{
    // Window configuration
    setResizable(true, true);
    m_constrainer.setFixedAspectRatio(16.0 / 10.0);
    m_constrainer.setSizeLimits(960, 600, 1920, 1200);
    setConstrainer(&m_constrainer);
    setSize(kDefaultW, kDefaultH);

    buildHeader();
    buildMacroPanel();
    buildPhysicsPanel();
    buildFxPanel();

    addAndMakeVisible(m_waveVis);

    // Initialise tab selection
    switchTab(0);
}

KhaenEditor::~KhaenEditor()
{
    m_presetBox.removeListener(this);
    m_prevPreset.removeListener(this);
    m_nextPreset.removeListener(this);
    m_tabDelay.removeListener(this);
    m_tabHarmonizer.removeListener(this);
    m_tabCrystal.removeListener(this);
    m_tabReverb.removeListener(this);
}

//==============================================================================
void KhaenEditor::buildHeader()
{
    addAndMakeVisible(m_presetBox);
    addAndMakeVisible(m_prevPreset);
    addAndMakeVisible(m_nextPreset);

    // Populate presets
    auto names = m_proc.getPresetManager().getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        m_presetBox.addItem(names[i], i + 1);

    m_presetBox.setSelectedId(1, juce::dontSendNotification);
    m_presetBox.addListener(this);

    m_prevPreset.addListener(this);
    m_nextPreset.addListener(this);

    // Preset box appearance
    m_presetBox.setColour(juce::ComboBox::backgroundColourId,  kPanel);
    m_presetBox.setColour(juce::ComboBox::textColourId,        kTextPrim);
    m_presetBox.setColour(juce::ComboBox::arrowColourId,       kCopper);
    m_presetBox.setColour(juce::ComboBox::outlineColourId,     kCopperDim.withAlpha(0.5f));

    m_prevPreset.setColour(juce::TextButton::buttonColourId,   kPanel);
    m_prevPreset.setColour(juce::TextButton::textColourOffId,  kCopper);
    m_nextPreset.setColour(juce::TextButton::buttonColourId,   kPanel);
    m_nextPreset.setColour(juce::TextButton::textColourOffId,  kCopper);

    // MIDI activity LED and label
    addAndMakeVisible(m_midiLed);
    addAndMakeVisible(m_midiLabel);

    m_midiLed.m_checkActivityCallback = [this]() {
        return m_proc.consumeMidiActivity();
    };

    m_midiLabel.setText("MIDI", juce::dontSendNotification);
    m_midiLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    m_midiLabel.setColour(juce::Label::textColourId, kTextDim);
    m_midiLabel.setJustificationType(juce::Justification::centredRight);
}

//==============================================================================
void KhaenEditor::buildMacroPanel()
{
    auto& apvts = m_proc.getAPVTS();

    addAndMakeVisible(m_knobAir);
    addAndMakeVisible(m_knobEmboc);
    addAndMakeVisible(m_knobResonance);

    m_knobAir.knob.setRange(0.0, 1.0, 0.01);
    m_knobEmboc.knob.setRange(0.0, 1.0, 0.01);
    m_knobResonance.knob.setRange(0.0, 1.0, 0.01);

    m_attAir       = std::make_unique<SliderAtt>(apvts, "macroAir",        m_knobAir.knob);
    m_attEmboc     = std::make_unique<SliderAtt>(apvts, "macroEmbouchure",  m_knobEmboc.knob);
    m_attResonance = std::make_unique<SliderAtt>(apvts, "macroResonance",   m_knobResonance.knob);
}

//==============================================================================
void KhaenEditor::buildPhysicsPanel()
{
    auto& apvts = m_proc.getAPVTS();

    addAndMakeVisible(m_knobStiffness);
    addAndMakeVisible(m_knobBore);
    addAndMakeVisible(m_knobBreathSpeed);
    addAndMakeVisible(m_knobSuctionBite);
    addAndMakeVisible(m_matLabel);
    addAndMakeVisible(m_matCombo);
    addAndMakeVisible(m_autoRiffBtn);

    m_knobStiffness.knob.setRange(0.0, 1.0, 0.01);
    m_knobBore.knob.setRange(8.0, 30.0, 0.1);
    m_knobBreathSpeed.knob.setRange(0.20, 3.0, 0.05);
    m_knobSuctionBite.knob.setRange(0.0, 1.0, 0.01);

    // Material selector
    m_matLabel.setText("MATERIAL", juce::dontSendNotification);
    m_matLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    m_matLabel.setColour(juce::Label::textColourId, kTextDim);
    m_matLabel.setJustificationType(juce::Justification::centred);

    juce::StringArray mats { "Bamboo", "Aged Bamboo", "Jade", "Brass", "PVC", "Dark Wood", "Metal" };
    for (int i = 0; i < mats.size(); ++i)
        m_matCombo.addItem(mats[i], i + 1);

    m_matCombo.setColour(juce::ComboBox::backgroundColourId,  kPanel);
    m_matCombo.setColour(juce::ComboBox::textColourId,        kTextPrim);
    m_matCombo.setColour(juce::ComboBox::arrowColourId,       kCopper);
    m_matCombo.setColour(juce::ComboBox::outlineColourId,     kCopperDim.withAlpha(0.5f));

    m_autoRiffBtn.setColour(juce::ToggleButton::textColourId, kTextDim);
    m_autoRiffBtn.setColour(juce::ToggleButton::tickColourId,   kCopper);

    m_attStiffness   = std::make_unique<SliderAtt>(apvts, "reedStiffness",  m_knobStiffness.knob);
    m_attBore        = std::make_unique<SliderAtt>(apvts, "boreDiameter",   m_knobBore.knob);
    m_attBreathSpeed = std::make_unique<SliderAtt>(apvts, "breathSpeed",    m_knobBreathSpeed.knob);
    m_attSuctionBite = std::make_unique<SliderAtt>(apvts, "suctionBite",    m_knobSuctionBite.knob);
    m_attMaterial    = std::make_unique<ComboAtt> (apvts, "material",       m_matCombo);
    m_attAutoRiff    = std::make_unique<ButtonAtt>(apvts, "autoInhaleRiff", m_autoRiffBtn);
}

//==============================================================================
void KhaenEditor::buildFxPanel()
{
    addAndMakeVisible(m_tabDelay);
    addAndMakeVisible(m_tabHarmonizer);
    addAndMakeVisible(m_tabCrystal);
    addAndMakeVisible(m_tabReverb);

    m_tabDelay.addListener(this);
    m_tabHarmonizer.addListener(this);
    m_tabCrystal.addListener(this);
    m_tabReverb.addListener(this);

    auto styleTab = [](juce::TextButton& btn)
    {
        btn.setColour(juce::TextButton::buttonColourId,   kPanel.darker(0.1f));
        btn.setColour(juce::TextButton::buttonOnColourId, kPanel.brighter(0.05f));
        btn.setColour(juce::TextButton::textColourOffId,  kTextDim);
        btn.setColour(juce::TextButton::textColourOnId,   kCopper);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(1001);
    };

    styleTab(m_tabDelay);
    styleTab(m_tabHarmonizer);
    styleTab(m_tabCrystal);
    styleTab(m_tabReverb);

    m_tabDelay.setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(m_delayPanel);
    addAndMakeVisible(m_harmPanel);
    addAndMakeVisible(m_crystalPanel);
    addAndMakeVisible(m_reverbPanel);

    buildDelayPanel();
    buildHarmonizerPanel();
    buildCrystalPanel();
    buildReverbPanel();
}

//==============================================================================
void KhaenEditor::buildDelayPanel()
{
    auto& apvts = m_proc.getAPVTS();

    m_delayPanel.addAndMakeVisible(m_knobDelTime);
    m_delayPanel.addAndMakeVisible(m_knobDelFb);
    m_delayPanel.addAndMakeVisible(m_knobDelMix);
    m_delayPanel.addAndMakeVisible(m_knobDelWow);
    m_delayPanel.addAndMakeVisible(m_knobDelFlut);
    m_delayPanel.addAndMakeVisible(m_pingPong);
    m_delayPanel.addAndMakeVisible(m_delayBypassBtn);

    m_knobDelTime.knob.setRange(10.0, 2000.0, 1.0);
    m_knobDelFb.knob.setRange(0.0, 0.95, 0.01);
    m_knobDelMix.knob.setRange(0.0, 1.0, 0.01);
    m_knobDelWow.knob.setRange(0.0, 1.0, 0.01);
    m_knobDelFlut.knob.setRange(0.0, 1.0, 0.01);

    m_pingPong.setColour(juce::ToggleButton::textColourId, kTextDim);
    m_pingPong.setColour(juce::ToggleButton::tickColourId,   kCopper);

    m_delayBypassBtn.setColour(juce::ToggleButton::textColourId, kTextDim);
    m_delayBypassBtn.setColour(juce::ToggleButton::tickColourId,   kCopper);

    m_attDelTime     = std::make_unique<SliderAtt>(apvts, "delayTime",     m_knobDelTime.knob);
    m_attDelFb       = std::make_unique<SliderAtt>(apvts, "delayFeedback", m_knobDelFb.knob);
    m_attDelMix      = std::make_unique<SliderAtt>(apvts, "delayMix",      m_knobDelMix.knob);
    m_attDelWow      = std::make_unique<SliderAtt>(apvts, "delayWow",      m_knobDelWow.knob);
    m_attDelFlut     = std::make_unique<SliderAtt>(apvts, "delayFlutter",  m_knobDelFlut.knob);
    m_attPingPong    = std::make_unique<ButtonAtt>(apvts, "delayPingPong", m_pingPong);
    m_attDelayBypass = std::make_unique<ButtonAtt>(apvts, "delayBypass",   m_delayBypassBtn);
}

//==============================================================================
void KhaenEditor::buildHarmonizerPanel()
{
    auto& apvts = m_proc.getAPVTS();

    m_harmPanel.addAndMakeVisible(m_knobHarmShift);
    m_harmPanel.addAndMakeVisible(m_knobHarmDet);
    m_harmPanel.addAndMakeVisible(m_knobHarmSpread);
    m_harmPanel.addAndMakeVisible(m_knobHarmMix);
    m_harmPanel.addAndMakeVisible(m_harmBypassBtn);

    m_knobHarmShift.knob.setRange(-24.0, 24.0, 1.0);
    m_knobHarmDet.knob.setRange(-50.0, 50.0, 0.5);
    m_knobHarmSpread.knob.setRange(0.0, 1.0, 0.01);
    m_knobHarmMix.knob.setRange(0.0, 1.0, 0.01);

    m_harmBypassBtn.setColour(juce::ToggleButton::textColourId, kTextDim);
    m_harmBypassBtn.setColour(juce::ToggleButton::tickColourId,   kCopper);

    m_attHarmShift  = std::make_unique<SliderAtt>(apvts, "harmonShift",  m_knobHarmShift.knob);
    m_attHarmDet    = std::make_unique<SliderAtt>(apvts, "harmonDetune", m_knobHarmDet.knob);
    m_attHarmSpread = std::make_unique<SliderAtt>(apvts, "harmonVoices", m_knobHarmSpread.knob);
    m_attHarmMix    = std::make_unique<SliderAtt>(apvts, "harmonMix",    m_knobHarmMix.knob);
    m_attHarmBypass = std::make_unique<ButtonAtt>(apvts, "harmonBypass", m_harmBypassBtn);
}

//==============================================================================
void KhaenEditor::buildCrystalPanel()
{
    auto& apvts = m_proc.getAPVTS();

    m_crystalPanel.addAndMakeVisible(m_knobCrysGrain);
    m_crystalPanel.addAndMakeVisible(m_knobCrysDelay);
    m_crystalPanel.addAndMakeVisible(m_knobCrysFb);
    m_crystalPanel.addAndMakeVisible(m_knobCrysMix);
    m_crystalPanel.addAndMakeVisible(m_knobCrysPitch);
    m_crystalPanel.addAndMakeVisible(m_crystalMode);
    m_crystalPanel.addAndMakeVisible(m_crystalModeLabel);
    m_crystalPanel.addAndMakeVisible(m_freezeBtn);
    m_crystalPanel.addAndMakeVisible(m_crysBypassBtn);

    m_knobCrysGrain.knob.setRange(20.0, 500.0, 1.0);
    m_knobCrysDelay.knob.setRange(10.0, 1000.0, 1.0);
    m_knobCrysFb.knob.setRange(0.0, 0.90, 0.01);
    m_knobCrysMix.knob.setRange(0.0, 1.0, 0.01);
    m_knobCrysPitch.knob.setRange(-24.0, 24.0, 0.5);

    m_crystalMode.addItem("Reverse Echo",    1);
    m_crystalMode.addItem("Pitch Feedback",  2);
    m_crystalMode.addItem("Glitched Freeze", 3);

    m_crystalModeLabel.setText("MODE", juce::dontSendNotification);
    m_crystalModeLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    m_crystalModeLabel.setColour(juce::Label::textColourId, kTextDim);
    m_crystalModeLabel.setJustificationType(juce::Justification::centred);

    m_crystalMode.setColour(juce::ComboBox::backgroundColourId,  kPanel);
    m_crystalMode.setColour(juce::ComboBox::textColourId,        kTextPrim);
    m_crystalMode.setColour(juce::ComboBox::arrowColourId,       kCopper);
    m_crystalMode.setColour(juce::ComboBox::outlineColourId,     kCopperDim.withAlpha(0.5f));

    m_freezeBtn.setClickingTogglesState(true);
    m_freezeBtn.setColour(juce::TextButton::buttonColourId,   kPanel);
    m_freezeBtn.setColour(juce::TextButton::buttonOnColourId, kCopper);
    m_freezeBtn.setColour(juce::TextButton::textColourOffId,  kCopper);
    m_freezeBtn.setColour(juce::TextButton::textColourOnId,   kBg);

    m_crysBypassBtn.setColour(juce::ToggleButton::textColourId, kTextDim);
    m_crysBypassBtn.setColour(juce::ToggleButton::tickColourId,   kCopper);

    m_attCrysGrain  = std::make_unique<SliderAtt>(apvts, "crystalGrainMs",  m_knobCrysGrain.knob);
    m_attCrysDelay  = std::make_unique<SliderAtt>(apvts, "crystalDelayMs",  m_knobCrysDelay.knob);
    m_attCrysFb     = std::make_unique<SliderAtt>(apvts, "crystalFeedback", m_knobCrysFb.knob);
    m_attCrysMix    = std::make_unique<SliderAtt>(apvts, "crystalMix",      m_knobCrysMix.knob);
    m_attCrysPitch  = std::make_unique<SliderAtt>(apvts, "crystalPitch",    m_knobCrysPitch.knob);
    m_attCrysMode   = std::make_unique<ComboAtt> (apvts, "crystalMode",     m_crystalMode);
    m_attFreeze     = std::make_unique<ButtonAtt>(apvts, "crystalFreeze",   m_freezeBtn);
    m_attCrysBypass = std::make_unique<ButtonAtt>(apvts, "crystalBypass",   m_crysBypassBtn);
}

//==============================================================================
void KhaenEditor::buildReverbPanel()
{
    auto& apvts = m_proc.getAPVTS();

    m_reverbPanel.addAndMakeVisible(m_knobRevSize);
    m_reverbPanel.addAndMakeVisible(m_knobRevDecay);
    m_reverbPanel.addAndMakeVisible(m_knobRevDamp);
    m_reverbPanel.addAndMakeVisible(m_knobRevWidth);
    m_reverbPanel.addAndMakeVisible(m_knobRevMix);
    m_reverbPanel.addAndMakeVisible(m_reverbPresetCombo);
    m_reverbPanel.addAndMakeVisible(m_reverbPresetLabel);
    m_reverbPanel.addAndMakeVisible(m_reverbBypassBtn);

    m_knobRevSize.knob.setRange(0.10, 1.0, 0.01);
    m_knobRevDecay.knob.setRange(0.10, 0.98, 0.01);
    m_knobRevDamp.knob.setRange(0.0, 1.0, 0.01);
    m_knobRevWidth.knob.setRange(0.0, 1.0, 0.01);
    m_knobRevMix.knob.setRange(0.0, 1.0, 0.01);

    m_reverbPresetCombo.addItem("Wat Phu (Ancient Temple)",   1);
    m_reverbPresetCombo.addItem("Luang Prabang Sanctuary",    2);
    m_reverbPresetCombo.addItem("Tham Kong Lor (Giant Cave)", 3);
    m_reverbPresetCombo.addItem("That Luang Pavilion",        4);
    m_reverbPresetCombo.addItem("Mekong River Grotto",        5);

    m_reverbPresetLabel.setText("LAOS SPACE", juce::dontSendNotification);
    m_reverbPresetLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    m_reverbPresetLabel.setColour(juce::Label::textColourId, kTextDim);
    m_reverbPresetLabel.setJustificationType(juce::Justification::centred);

    m_reverbPresetCombo.setColour(juce::ComboBox::backgroundColourId,  kPanel);
    m_reverbPresetCombo.setColour(juce::ComboBox::textColourId,        kTextPrim);
    m_reverbPresetCombo.setColour(juce::ComboBox::arrowColourId,       kCopper);
    m_reverbPresetCombo.setColour(juce::ComboBox::outlineColourId,     kCopperDim.withAlpha(0.5f));

    m_reverbBypassBtn.setColour(juce::ToggleButton::textColourId, kTextDim);
    m_reverbBypassBtn.setColour(juce::ToggleButton::tickColourId,   kCopper);

    m_reverbPresetCombo.onChange = [this]()
    {
        int selectedIdx = m_reverbPresetCombo.getSelectedItemIndex();
        if (selectedIdx >= 0 && selectedIdx <= 4)
        {
            ReverbEngine::Parameters rp;
            ReverbEngine::applyLaosPreset(selectedIdx, rp);
            auto& ap = m_proc.getAPVTS();
            if (auto* p = ap.getParameter("reverbSize"))  p->setValueNotifyingHost(p->convertTo0to1(rp.size));
            if (auto* p = ap.getParameter("reverbDecay")) p->setValueNotifyingHost(p->convertTo0to1(rp.decay));
            if (auto* p = ap.getParameter("reverbDamp"))  p->setValueNotifyingHost(p->convertTo0to1(rp.damp));
            if (auto* p = ap.getParameter("reverbWidth")) p->setValueNotifyingHost(p->convertTo0to1(rp.width));
            if (auto* p = ap.getParameter("reverbMix"))   p->setValueNotifyingHost(p->convertTo0to1(rp.mix));
        }
    };

    m_attRevSize    = std::make_unique<SliderAtt>(apvts, "reverbSize",   m_knobRevSize.knob);
    m_attRevDecay   = std::make_unique<SliderAtt>(apvts, "reverbDecay",  m_knobRevDecay.knob);
    m_attRevDamp    = std::make_unique<SliderAtt>(apvts, "reverbDamp",   m_knobRevDamp.knob);
    m_attRevWidth   = std::make_unique<SliderAtt>(apvts, "reverbWidth",  m_knobRevWidth.knob);
    m_attRevMix     = std::make_unique<SliderAtt>(apvts, "reverbMix",    m_knobRevMix.knob);
    m_attRevPreset  = std::make_unique<ComboAtt> (apvts, "reverbPreset", m_reverbPresetCombo);
    m_attRevBypass  = std::make_unique<ButtonAtt>(apvts, "reverbBypass", m_reverbBypassBtn);
}

//==============================================================================
void KhaenEditor::switchTab (int tab)
{
    m_activeTab = tab;
    m_tabDelay.setToggleState(tab == 0, juce::dontSendNotification);
    m_tabHarmonizer.setToggleState(tab == 1, juce::dontSendNotification);
    m_tabCrystal.setToggleState(tab == 2, juce::dontSendNotification);
    m_tabReverb.setToggleState(tab == 3, juce::dontSendNotification);

    m_delayPanel.setVisible(tab == 0);
    m_harmPanel.setVisible(tab == 1);
    m_crystalPanel.setVisible(tab == 2);
    m_reverbPanel.setVisible(tab == 3);

    repaint();
}

//==============================================================================
void KhaenEditor::comboBoxChanged (juce::ComboBox* box)
{
    if (box == &m_presetBox)
        m_proc.loadPreset(m_presetBox.getSelectedItemIndex());
}

void KhaenEditor::buttonClicked (juce::Button* btn)
{
    if (btn == &m_tabDelay)      switchTab(0);
    if (btn == &m_tabHarmonizer) switchTab(1);
    if (btn == &m_tabCrystal)    switchTab(2);
    if (btn == &m_tabReverb)     switchTab(3);

    if (btn == &m_prevPreset)
    {
        int idx = juce::jmax(0, m_presetBox.getSelectedItemIndex() - 1);
        m_presetBox.setSelectedItemIndex(idx);
        m_proc.loadPreset(idx);
    }
    if (btn == &m_nextPreset)
    {
        int idx = juce::jmin(m_proc.getPresetManager().getNumPresets() - 1,
                             m_presetBox.getSelectedItemIndex() + 1);
        m_presetBox.setSelectedItemIndex(idx);
        m_proc.loadPreset(idx);
    }
}

//==============================================================================
void KhaenEditor::resized()
{
    auto b = getLocalBounds();
    float scale = static_cast<float>(getWidth()) / kDefaultW;

    auto header  = b.removeFromTop(juce::roundToInt(60 * scale));
    auto fxPanel = b.removeFromBottom(juce::roundToInt(240 * scale));
    auto content = b; // middle area

    // ── Header ────────────────────────────────────────────────────────────────
    {
        auto h = header.reduced(juce::roundToInt(12 * scale), juce::roundToInt(12 * scale));

        // Right side: MIDI LED + Label
        auto midiArea = h.removeFromRight(juce::roundToInt(70 * scale));
        m_midiLed.setBounds(midiArea.removeFromRight(juce::roundToInt(20 * scale)).reduced(2));
        m_midiLabel.setBounds(midiArea);

        h.removeFromRight(juce::roundToInt(16 * scale));

        // Left side: Preset selector + Prev/Next
        int comboW = juce::roundToInt(200 * scale);
        m_presetBox.setBounds(h.removeFromLeft(comboW));
        h.removeFromLeft(juce::roundToInt(6 * scale));
        m_prevPreset.setBounds(h.removeFromLeft(juce::roundToInt(32 * scale)));
        h.removeFromLeft(juce::roundToInt(4 * scale));
        m_nextPreset.setBounds(h.removeFromLeft(juce::roundToInt(32 * scale)));
    }

    // ── Content: Macros (Left) | Visualizer (Center) | Physics (Right) ────────
    {
        int sideW = juce::roundToInt(220 * scale);

        // Symmetric left macro area
        auto macroArea = content.removeFromLeft(sideW);
        int  knobH = macroArea.getHeight() / 3;
        m_knobAir.setBounds(macroArea.removeFromTop(knobH).reduced(juce::roundToInt(14 * scale), juce::roundToInt(10 * scale)));
        m_knobEmboc.setBounds(macroArea.removeFromTop(knobH).reduced(juce::roundToInt(14 * scale), juce::roundToInt(10 * scale)));
        m_knobResonance.setBounds(macroArea.reduced(juce::roundToInt(14 * scale), juce::roundToInt(10 * scale)));

        // Symmetric right physics area
        auto physArea = content.removeFromRight(sideW).reduced(juce::roundToInt(10 * scale), 0);
        int  physW    = physArea.getWidth();
        
        // 2x2 Symmetrical Grid for Physics Knobs
        int rowH = juce::roundToInt(120 * scale);
        int colW = physW / 2;

        // Row 1: STIFFNESS & BORE DIA (Symmetrical 2-column)
        auto row1 = physArea.removeFromTop(rowH);
        m_knobStiffness.setBounds(row1.removeFromLeft(colW).reduced(juce::roundToInt(4 * scale), juce::roundToInt(4 * scale)));
        m_knobBore.setBounds(row1.reduced(juce::roundToInt(4 * scale), juce::roundToInt(4 * scale)));

        // Row 2: BREATH SPD & SUCTION BITE (Symmetrical 2-column)
        auto row2 = physArea.removeFromTop(rowH);
        m_knobBreathSpeed.setBounds(row2.removeFromLeft(colW).reduced(juce::roundToInt(4 * scale), juce::roundToInt(4 * scale)));
        m_knobSuctionBite.setBounds(row2.reduced(juce::roundToInt(4 * scale), juce::roundToInt(4 * scale)));
        
        // Bottom: Material Combo & Auto-Riff button (Symmetrically centered)
        int comboW = juce::roundToInt(170 * scale);
        int marginX = (physW - comboW) / 2;
        auto comboArea = physArea.withTrimmedLeft(marginX).withTrimmedRight(marginX);
        
        comboArea.removeFromTop(juce::roundToInt(4 * scale));
        m_matLabel.setBounds(comboArea.removeFromTop(juce::roundToInt(14 * scale)));
        m_matCombo.setBounds(comboArea.removeFromTop(juce::roundToInt(24 * scale)));
        comboArea.removeFromTop(juce::roundToInt(6 * scale));
        
        int btnW = juce::roundToInt(130 * scale);
        int btnMargin = (comboW - btnW) / 2;
        m_autoRiffBtn.setBounds(comboArea.withTrimmedLeft(btnMargin).withTrimmedRight(btnMargin).removeFromTop(juce::roundToInt(24 * scale)));

        // Visualizer fills remaining center area
        m_waveVis.setBounds(content.reduced(juce::roundToInt(8 * scale)));
    }

    // ── FX Panel (Symmetric Knobs & Centered Controls) ────────────────────────
    {
        auto tabBar = fxPanel.removeFromTop(juce::roundToInt(36 * scale));
        int  tabW   = tabBar.getWidth() / 4;
        m_tabDelay.setBounds(tabBar.removeFromLeft(tabW));
        m_tabHarmonizer.setBounds(tabBar.removeFromLeft(tabW));
        m_tabCrystal.setBounds(tabBar.removeFromLeft(tabW));
        m_tabReverb.setBounds(tabBar);

        m_delayPanel.setBounds(fxPanel);
        m_harmPanel.setBounds(fxPanel);
        m_crystalPanel.setBounds(fxPanel);
        m_reverbPanel.setBounds(fxPanel);

        int panelW  = fxPanel.getWidth();
        int panelH  = fxPanel.getHeight();
        int knobW   = juce::roundToInt(105 * scale);
        int knobH   = juce::roundToInt(125 * scale);
        int centerY = panelH / 2;

        // Helper lambda to calculate exact symmetrical centered rectangle for a column
        auto getColRect = [&](int colIdx, int totalCols, int w, int h) -> juce::Rectangle<int>
        {
            int colW = panelW / totalCols;
            int cx   = colIdx * colW + colW / 2;
            return juce::Rectangle<int>(cx - w / 2, centerY - h / 2, w, h);
        };

        // 1. Delay panel layout (6 columns symmetrically centered)
        {
            m_knobDelTime.setBounds(getColRect(0, 6, knobW, knobH));
            m_knobDelFb.setBounds(getColRect(1, 6, knobW, knobH));
            m_knobDelMix.setBounds(getColRect(2, 6, knobW, knobH));
            m_knobDelWow.setBounds(getColRect(3, 6, knobW, knobH));
            m_knobDelFlut.setBounds(getColRect(4, 6, knobW, knobH));

            // Ping-Pong toggle & Bypass toggle centered in column 5
            int ppW = juce::roundToInt(115 * scale);
            int ppH = juce::roundToInt(70 * scale);
            auto col5 = getColRect(5, 6, ppW, ppH);
            m_pingPong.setBounds(col5.removeFromTop(juce::roundToInt(32 * scale)));
            col5.removeFromTop(juce::roundToInt(6 * scale));
            m_delayBypassBtn.setBounds(col5.removeFromTop(juce::roundToInt(32 * scale)));
        }

        // 2. Harmonizer panel layout (5 columns symmetrically centered)
        {
            m_knobHarmShift.setBounds(getColRect(0, 5, knobW, knobH));
            m_knobHarmDet.setBounds(getColRect(1, 5, knobW, knobH));
            m_knobHarmSpread.setBounds(getColRect(2, 5, knobW, knobH));
            m_knobHarmMix.setBounds(getColRect(3, 5, knobW, knobH));

            int bypW = juce::roundToInt(115 * scale);
            int bypH = juce::roundToInt(32 * scale);
            m_harmBypassBtn.setBounds(getColRect(4, 5, bypW, bypH));
        }

        // 3. Crystal panel layout (6 columns symmetrically centered)
        {
            m_knobCrysGrain.setBounds(getColRect(0, 6, knobW, knobH));
            m_knobCrysDelay.setBounds(getColRect(1, 6, knobW, knobH));
            m_knobCrysFb.setBounds(getColRect(2, 6, knobW, knobH));
            m_knobCrysMix.setBounds(getColRect(3, 6, knobW, knobH));
            m_knobCrysPitch.setBounds(getColRect(4, 6, knobW, knobH));

            // Mode selector, Freeze button & Bypass toggle centered in column 5
            int modeW = juce::roundToInt(135 * scale);
            int modeH = juce::roundToInt(115 * scale);
            auto modeRect = getColRect(5, 6, modeW, modeH);

            m_crystalModeLabel.setBounds(modeRect.removeFromTop(juce::roundToInt(16 * scale)));
            m_crystalMode.setBounds(modeRect.removeFromTop(juce::roundToInt(24 * scale)));
            modeRect.removeFromTop(juce::roundToInt(4 * scale));
            m_freezeBtn.setBounds(modeRect.removeFromTop(juce::roundToInt(26 * scale)));
            modeRect.removeFromTop(juce::roundToInt(4 * scale));
            m_crysBypassBtn.setBounds(modeRect.removeFromTop(juce::roundToInt(26 * scale)));
        }

        // 4. Reverb panel layout (6 columns symmetrically centered)
        {
            m_knobRevSize.setBounds(getColRect(0, 6, knobW, knobH));
            m_knobRevDecay.setBounds(getColRect(1, 6, knobW, knobH));
            m_knobRevDamp.setBounds(getColRect(2, 6, knobW, knobH));
            m_knobRevWidth.setBounds(getColRect(3, 6, knobW, knobH));
            m_knobRevMix.setBounds(getColRect(4, 6, knobW, knobH));

            // Preset selector & Bypass toggle centered in column 5
            int revPreW = juce::roundToInt(150 * scale);
            int revPreH = juce::roundToInt(85 * scale);
            auto revPreRect = getColRect(5, 6, revPreW, revPreH);

            m_reverbPresetLabel.setBounds(revPreRect.removeFromTop(juce::roundToInt(16 * scale)));
            m_reverbPresetCombo.setBounds(revPreRect.removeFromTop(juce::roundToInt(26 * scale)));
            revPreRect.removeFromTop(juce::roundToInt(6 * scale));
            m_reverbBypassBtn.setBounds(revPreRect.removeFromTop(juce::roundToInt(26 * scale)));
        }
    }
}

//==============================================================================
void KhaenEditor::paint (juce::Graphics& g)
{
    float scale = static_cast<float>(getWidth()) / kDefaultW;
    auto  b     = getLocalBounds();
    auto  header= b.removeFromTop(juce::roundToInt(60 * scale));
    auto  fxArea= b.removeFromBottom(juce::roundToInt(240 * scale));

    // ── Global background ────────────────────────────────────────────────────
    g.fillAll(kBg);

    // ── Header area ──────────────────────────────────────────────────────────
    paintHeader(g, header, scale);

    // ── Content area backgrounds (Symmetrical side panels) ───────────────────
    int sideW = juce::roundToInt(220 * scale);

    auto macroArea = b.removeFromLeft(sideW);
    {
        auto bg = juce::ColourGradient::vertical(
            kPanel.brighter(0.03f), (float)macroArea.getY(),
            kBg, (float)macroArea.getBottom());
        g.setGradientFill(bg);
        g.fillRect(macroArea);

        // Right edge separator
        g.setColour(kBorder);
        g.fillRect(macroArea.getRight() - 1, macroArea.getY(), 1, macroArea.getHeight());
    }

    auto physArea = b.removeFromRight(sideW);
    {
        auto bg2 = juce::ColourGradient::vertical(
            kPanel, (float)physArea.getY(),
            kBg, (float)physArea.getBottom());
        g.setGradientFill(bg2);
        g.fillRect(physArea);

        // Left edge separator
        g.setColour(kBorder);
        g.fillRect(physArea.getX(), physArea.getY(), 1, physArea.getHeight());
    }

    // ── FX panel area ────────────────────────────────────────────────────────
    paintFxArea(g, fxArea, scale);

    // ── Separator lines ──────────────────────────────────────────────────────
    g.setColour(kCopperDim.withAlpha(0.35f));
    g.fillRect(0, juce::roundToInt(60 * scale), getWidth(), 1);

    g.setColour(kBorder);
    g.fillRect(0, getHeight() - juce::roundToInt(240 * scale), getWidth(), 1);

    // ── KHAEN logo wordmark ───────────────────────────────────────────────────
    auto logoArea = getLocalBounds().removeFromTop(juce::roundToInt(60 * scale));
    logoArea.removeFromRight(juce::roundToInt(160 * scale));
    logoArea.removeFromLeft(juce::roundToInt(260 * scale));

    g.setFont(juce::Font(22.0f * scale, juce::Font::bold).withExtraKerningFactor(0.18f));
    g.setColour(kCopper);
    g.drawFittedText("K H A E N", logoArea.reduced(6), juce::Justification::centred, 1);

    // Subtitle
    g.setFont(juce::Font(9.0f * scale, juce::Font::plain));
    g.setColour(kTextDim);
    g.drawFittedText("PHYSICAL MODEL SYNTHESIZER",
                     logoArea.translated(0, juce::roundToInt(16 * scale)).reduced(6),
                     juce::Justification::centred, 1);
}

void KhaenEditor::paintHeader (juce::Graphics& g, juce::Rectangle<int> area, float /*scale*/)
{
    auto bg = juce::ColourGradient::vertical(
        kPanel.brighter(0.06f), (float)area.getY(),
        kPanel,                  (float)area.getBottom());
    g.setGradientFill(bg);
    g.fillRect(area);
}

void KhaenEditor::paintFxArea (juce::Graphics& g, juce::Rectangle<int> area, float scale)
{
    auto bg = juce::ColourGradient::vertical(
        kPanel, (float)area.getY(),
        kPanel.darker(0.1f), (float)area.getBottom());
    g.setGradientFill(bg);
    g.fillRect(area);

    // Tab bar background
    auto tabBar = area.removeFromTop(juce::roundToInt(36 * scale));
    g.setColour(kBg);
    g.fillRect(tabBar);

    // Active tab copper underline
    int tabW = area.getWidth() / 4;
    int tx = area.getX() + m_activeTab * tabW;
    int ty = tabBar.getBottom() - juce::roundToInt(2 * scale);
    g.setColour(kCopper);
    g.fillRect(tx, ty, tabW, juce::roundToInt(2 * scale));
}
