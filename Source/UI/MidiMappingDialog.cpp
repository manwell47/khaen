#include "MidiMappingDialog.h"

namespace
{
    static const juce::Colour kBg        = juce::Colour(0xFF0D0D14);
    static const juce::Colour kPanel     = juce::Colour(0xFF161622);
    static const juce::Colour kRowEven   = juce::Colour(0xFF14141E);
    static const juce::Colour kRowOdd    = juce::Colour(0xFF191926);
    static const juce::Colour kCopper    = juce::Colour(0xFFC8965A);
    static const juce::Colour kCopperDim = juce::Colour(0xFF8B6914);
    static const juce::Colour kGold      = juce::Colour(0xFFE8C97A);
    static const juce::Colour kGhost     = juce::Colour(0xFF6A7A9A);
    static const juce::Colour kTextPrim  = juce::Colour(0xFFEEEEEE);
    static const juce::Colour kTextDim   = juce::Colour(0xFF7A7A8E);
    static const juce::Colour kCyan      = juce::Colour(0xFF4AC4E2);
}

//==============================================================================
MidiMappingDialog::RowComponent::RowComponent (MidiMappingDialog& owner, const ParamItem& it)
    : item(it), m_owner(owner)
{
    addAndMakeVisible(m_learnBtn);
    addAndMakeVisible(m_clearBtn);

    m_learnBtn.setColour(juce::TextButton::buttonColourId,  kPanel.brighter(0.06f));
    m_learnBtn.setColour(juce::TextButton::textColourOffId, kCopper);

    m_clearBtn.setColour(juce::TextButton::buttonColourId,  kPanel);
    m_clearBtn.setColour(juce::TextButton::textColourOffId, kTextDim);

    m_learnBtn.onClick = [this]()
    {
        if (m_owner.m_midiCCManager.isLearningParam(item.paramID))
            m_owner.m_midiCCManager.stopLearning();
        else
            m_owner.m_midiCCManager.startLearning(item.paramID);

        updateState();
    };

    m_clearBtn.onClick = [this]()
    {
        m_owner.m_midiCCManager.clearMapping(item.paramID);
        updateState();
    };

    updateState();
}

void MidiMappingDialog::RowComponent::updateState()
{
    bool isLearning = m_owner.m_midiCCManager.isLearningParam(item.paramID);
    int cc = m_owner.m_midiCCManager.getCCForParam(item.paramID);

    if (isLearning)
    {
        m_learnBtn.setButtonText("WAITING...");
        m_learnBtn.setColour(juce::TextButton::buttonColourId,  kCyan.darker(0.5f));
        m_learnBtn.setColour(juce::TextButton::textColourOffId, kCyan.brighter(0.6f));
    }
    else
    {
        m_learnBtn.setButtonText("LEARN");
        m_learnBtn.setColour(juce::TextButton::buttonColourId,  kPanel.brighter(0.06f));
        m_learnBtn.setColour(juce::TextButton::textColourOffId, kCopper);
    }

    m_clearBtn.setEnabled(cc >= 0);
    m_clearBtn.setAlpha(cc >= 0 ? 1.0f : 0.4f);

    repaint();
}

void MidiMappingDialog::RowComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(3.0f, 1.0f);

    // Row category pill
    auto catArea = b.removeFromLeft(52.0f).reduced(1.0f, 2.0f);
    juce::Colour catCol = item.category == "MACRO"   ? kCopper
                        : item.category == "PHYSICS" ? kGold
                        : item.category == "DELAY"   ? kCyan
                        : item.category == "HARMON"  ? kGhost
                        : item.category == "CRYSTAL" ? juce::Colour(0xFFB072D8)
                        : juce::Colour(0xFF50C896);

    g.setColour(catCol.withAlpha(0.18f));
    g.fillRoundedRectangle(catArea, 3.0f);
    g.setColour(catCol);
    g.drawRoundedRectangle(catArea, 3.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions().withHeight(8.5f).withStyle("Bold")));
    g.drawText(item.category, catArea, juce::Justification::centred, false);

    // Parameter Name
    b.removeFromLeft(5.0f);
    auto nameArea = b.removeFromLeft(130.0f).reduced(2.0f, 0.0f);
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
    g.setColour(kTextPrim);
    g.drawText(item.displayName, nameArea, juce::Justification::centredLeft, true);

    // CC Value badge
    int cc = m_owner.m_midiCCManager.getCCForParam(item.paramID);
    bool isLearning = m_owner.m_midiCCManager.isLearningParam(item.paramID);

    b.removeFromLeft(5.0f);
    auto ccArea = b.removeFromLeft(82.0f).reduced(1.0f, 2.0f);
    if (isLearning)
    {
        g.setColour(kCyan.withAlpha(0.25f));
        g.fillRoundedRectangle(ccArea, 3.0f);
        g.setColour(kCyan);
        g.drawRoundedRectangle(ccArea, 3.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f).withStyle("Bold")));
        g.drawText("LEARN...", ccArea, juce::Justification::centred, false);
    }
    else if (cc >= 0)
    {
        g.setColour(kGold.withAlpha(0.18f));
        g.fillRoundedRectangle(ccArea, 3.0f);
        g.setColour(kGold);
        g.drawRoundedRectangle(ccArea, 3.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
        g.drawText("CC " + juce::String(cc), ccArea, juce::Justification::centred, false);
    }
    else
    {
        g.setColour(kTextDim.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Italic")));
        g.drawText("- Unset -", ccArea, juce::Justification::centred, false);
    }
}

void MidiMappingDialog::RowComponent::resized()
{
    auto b = getLocalBounds().reduced(2, 2);
    m_clearBtn.setBounds(b.removeFromRight(46).reduced(1, 1));
    b.removeFromRight(4);
    m_learnBtn.setBounds(b.removeFromRight(62).reduced(1, 1));
}

//==============================================================================
MidiMappingDialog::ListContentComponent::ListContentComponent (MidiMappingDialog& owner,
                                                               const std::vector<ParamItem>& items)
{
    for (const auto& it : items)
    {
        auto row = std::make_unique<RowComponent>(owner, it);
        addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }
}

void MidiMappingDialog::ListContentComponent::resized()
{
    int y = 0;
    const int rowH = 28;
    for (auto& row : rows)
    {
        row->setBounds(0, y, getWidth(), rowH);
        y += rowH;
    }
}

void MidiMappingDialog::ListContentComponent::updateAllRows()
{
    for (auto& row : rows)
        row->updateState();
    repaint();
}

//==============================================================================
MidiMappingDialog::MidiMappingDialog (MidiCCManager& midiCCManager)
    : m_midiCCManager(midiCCManager)
{
    buildParameterList();

    addAndMakeVisible(m_titleLabel);
    addAndMakeVisible(m_closeBtn);
    addAndMakeVisible(m_stdMidiBtn);
    addAndMakeVisible(m_clearAllBtn);
    addAndMakeVisible(m_saveDefaultBtn);
    addAndMakeVisible(m_loadDefaultBtn);

    m_titleLabel.setText("KHAEN — MIDI CC ASSIGNMENT MATRIX", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.5f).withStyle("Bold")));
    m_titleLabel.setColour(juce::Label::textColourId, kCopper);
    m_titleLabel.setJustificationType(juce::Justification::centredLeft);

    m_closeBtn.setColour(juce::TextButton::buttonColourId,  kPanel.brighter(0.08f));
    m_closeBtn.setColour(juce::TextButton::textColourOffId, kGold);
    m_closeBtn.onClick = [this]() { if (m_closeCallback) m_closeCallback(); };

    auto stylePresetBtn = [](juce::TextButton& btn, juce::Colour col)
    {
        btn.setColour(juce::TextButton::buttonColourId,  kPanel.brighter(0.05f));
        btn.setColour(juce::TextButton::textColourOffId, col);
    };

    stylePresetBtn(m_stdMidiBtn,     kCyan.brighter(0.2f));
    stylePresetBtn(m_clearAllBtn,    kGhost);
    stylePresetBtn(m_saveDefaultBtn, kCyan);
    stylePresetBtn(m_loadDefaultBtn, kTextPrim);

    m_stdMidiBtn.onClick = [this]()
    {
        m_midiCCManager.applyStandardMidiCC();
        if (m_content) m_content->updateAllRows();
    };

    m_clearAllBtn.onClick = [this]()
    {
        m_midiCCManager.clearAll();
        if (m_content) m_content->updateAllRows();
    };

    m_saveDefaultBtn.onClick = [this]()
    {
        m_midiCCManager.saveAsGlobalDefault();
    };

    m_loadDefaultBtn.onClick = [this]()
    {
        m_midiCCManager.loadGlobalDefault();
        if (m_content) m_content->updateAllRows();
    };

    m_content = std::make_unique<ListContentComponent>(*this, m_paramItems);
    m_viewport.setViewedComponent(m_content.get(), false);
    m_viewport.setScrollBarsShown(true, false);
    m_viewport.setScrollBarThickness(6);
    addAndMakeVisible(m_viewport);

    m_midiCCManager.addListener(this);
}

MidiMappingDialog::~MidiMappingDialog()
{
    m_midiCCManager.removeListener(this);
}

//==============================================================================
void MidiMappingDialog::buildParameterList()
{
    m_paramItems = {
        // Macros
        { "MACRO",   "macroAir",        "Air Macro" },
        { "MACRO",   "macroEmbouchure", "Embouchure Macro" },
        { "MACRO",   "macroResonance",  "Resonance Macro" },

        // Physical Model
        { "PHYSICS", "reedStiffness",   "Reed Stiffness" },
        { "PHYSICS", "boreDiameter",    "Bore Diameter" },
        { "PHYSICS", "blowPressure",    "Blow Pressure" },
        { "PHYSICS", "lipDamping",      "Lip Damping" },
        { "PHYSICS", "boreReflection",  "Bore Reflection" },
        { "PHYSICS", "breathSpeed",     "Breath Speed" },
        { "PHYSICS", "suctionBite",     "Suction Bite" },
        { "PHYSICS", "tongueContact",   "Tongue Contact" },
        { "PHYSICS", "vocalTract",      "Vocal Tract" },

        // Tape Delay
        { "DELAY",   "delayTime",       "Delay Time" },
        { "DELAY",   "delayFeedback",   "Delay Feedback" },
        { "DELAY",   "delayMix",        "Delay Mix" },
        { "DELAY",   "delayWow",        "Delay Wow" },
        { "DELAY",   "delayFlutter",    "Delay Flutter" },

        // Harmonizer
        { "HARMON",  "harmonShift",     "Harmonizer Shift" },
        { "HARMON",  "harmonMix",       "Harmonizer Mix" },
        { "HARMON",  "harmonVoices",    "Harmonizer Spread" },
        { "HARMON",  "harmonDetune",    "Harmonizer Detune" },

        // Granular Crystallizer
        { "CRYSTAL", "crystalGrainMs",  "Crystal Grain Size" },
        { "CRYSTAL", "crystalDelayMs",  "Crystal Delay" },
        { "CRYSTAL", "crystalFeedback", "Crystal Feedback" },
        { "CRYSTAL", "crystalMix",      "Crystal Mix" },
        { "CRYSTAL", "crystalPitch",    "Crystal Pitch" },

        // Reverb Engine
        { "REVERB",  "reverbSize",      "Reverb Size" },
        { "REVERB",  "reverbDecay",     "Reverb Decay" },
        { "REVERB",  "reverbDamp",      "Reverb Damping" },
        { "REVERB",  "reverbWidth",     "Reverb Width" },
        { "REVERB",  "reverbMix",       "Reverb Mix" }
    };
}

//==============================================================================
void MidiMappingDialog::midiMappingsChanged()
{
    if (m_content)
        m_content->updateAllRows();
}

void MidiMappingDialog::midiLearnStateChanged (const juce::String& /*paramID*/, bool /*isLearning*/)
{
    if (m_content)
        m_content->updateAllRows();
}

//==============================================================================
void MidiMappingDialog::paint (juce::Graphics& g)
{
    // Dark semi-transparent background overlay
    g.setColour(juce::Colours::black.withAlpha(0.70f));
    g.fillAll();

    // Centered, compact dialog card (width ~460, height ~500)
    int cardW = juce::jmin(460, getWidth() - 24);
    int cardH = juce::jmin(500, getHeight() - 24);
    auto card = getLocalBounds().withSizeKeepingCentre(cardW, cardH).toFloat();

    // Shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(card.translated(0.0f, 4.0f), 10.0f);

    // Dialog card body
    g.setColour(kBg);
    g.fillRoundedRectangle(card, 10.0f);

    // Copper border
    g.setColour(kCopper.withAlpha(0.6f));
    g.drawRoundedRectangle(card, 10.0f, 1.5f);

    // Header divider lines
    g.setColour(kPanel.brighter(0.12f));
    g.drawLine(card.getX(), card.getY() + 38.0f, card.getRight(), card.getY() + 38.0f, 1.0f);
    g.drawLine(card.getX(), card.getY() + 72.0f, card.getRight(), card.getY() + 72.0f, 1.0f);
}

void MidiMappingDialog::resized()
{
    int cardW = juce::jmin(460, getWidth() - 24);
    int cardH = juce::jmin(500, getHeight() - 24);
    auto card = getLocalBounds().withSizeKeepingCentre(cardW, cardH);

    // Header top row (38px)
    auto headerRow = card.removeFromTop(38).reduced(12, 5);
    m_closeBtn.setBounds(headerRow.removeFromRight(24).reduced(0, 1));
    m_titleLabel.setBounds(headerRow);

    // Presets Toolbar (34px)
    auto toolRow = card.removeFromTop(34).reduced(10, 4);
    m_stdMidiBtn.setBounds(toolRow.removeFromLeft(98).reduced(2));
    m_clearAllBtn.setBounds(toolRow.removeFromLeft(68).reduced(2));
    toolRow.removeFromLeft(6);
    m_saveDefaultBtn.setBounds(toolRow.removeFromLeft(86).reduced(2));
    m_loadDefaultBtn.setBounds(toolRow.removeFromLeft(86).reduced(2));

    // Viewport for rows
    m_viewport.setBounds(card.reduced(6, 6));
    if (m_content)
    {
        int totalH = static_cast<int>(m_paramItems.size()) * 28;
        m_content->setBounds(0, 0, m_viewport.getWidth() - 8, totalH);
    }
}
