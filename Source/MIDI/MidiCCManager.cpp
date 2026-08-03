#include "MidiCCManager.h"

//==============================================================================
MidiCCManager::MidiCCManager()
{
    for (auto& slot : m_ccLookup)
        slot.store(nullptr, std::memory_order_relaxed);
}

MidiCCManager::~MidiCCManager()
{
    cancelPendingUpdate();
}

void MidiCCManager::setAPVTS (juce::AudioProcessorValueTreeState* apvts)
{
    m_apvts = apvts;
    rebuildFastLookupTable();
}

//==============================================================================
bool MidiCCManager::handleMidiCC (int channel, int ccNumber, float raw0To127Value) noexcept
{
    if (ccNumber < 0 || ccNumber > 127)
        return false;

    const float normVal = juce::jlimit(0.0f, 1.0f, raw0To127Value / 127.0f);

    // ── Check if currently learning a parameter ──────────────────────────────
    if (m_isLearning.load(std::memory_order_acquire))
    {
        auto* param = m_learningParam.load(std::memory_order_relaxed);
        if (param != nullptr)
        {
            // End learning mode atomically
            m_isLearning.store(false, std::memory_order_release);
            m_learningParam.store(nullptr, std::memory_order_relaxed);

            // Immediately set parameter value
            param->setValueNotifyingHost(normVal);

            // Try to immediately update mappings without blocking audio thread
            std::unique_lock<std::mutex> lock(m_mapMutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                juce::String paramID;
                {
                    std::lock_guard<std::mutex> learnLock(m_learnMutex);
                    paramID = m_learningParamID;
                    m_learningParamID.clear();
                }

                if (paramID.isNotEmpty())
                {
                    for (auto it = m_mappings.begin(); it != m_mappings.end(); )
                    {
                        if (it->second.ccNumber == ccNumber)
                            it = m_mappings.erase(it);
                        else
                            ++it;
                    }
                    m_mappings[paramID] = { paramID, ccNumber, channel };
                }

                // Update lookup table
                std::array<juce::RangedAudioParameter*, 128> tempTable {};
                tempTable.fill(nullptr);
                if (m_apvts != nullptr)
                {
                    for (const auto& kv : m_mappings)
                    {
                        const auto& m = kv.second;
                        if (m.ccNumber >= 0 && m.ccNumber < 128)
                        {
                            if (auto* p = m_apvts->getParameter(m.paramID))
                                tempTable[m.ccNumber] = p;
                        }
                    }
                }
                for (int i = 0; i < 128; ++i)
                    m_ccLookup[i].store(tempTable[i], std::memory_order_release);
            }
            else
            {
                // Fallback: defer to message thread
                m_learnedCC.store(ccNumber, std::memory_order_relaxed);
                m_learnedChannel.store(channel, std::memory_order_relaxed);
            }

            // Always trigger async update for UI listener notifications
            triggerAsyncUpdate();
            return true;
        }
    }

    // ── Regular CC modulation lookup ─────────────────────────────────────────
    auto* targetParam = m_ccLookup[ccNumber].load(std::memory_order_relaxed);
    if (targetParam != nullptr)
    {
        targetParam->setValueNotifyingHost(normVal);
        return true;
    }

    return false;
}

//==============================================================================
void MidiCCManager::startLearning (const juce::String& paramID)
{
    if (m_apvts == nullptr) return;

    auto* param = m_apvts->getParameter(paramID);
    if (param == nullptr) return;

    {
        std::lock_guard<std::mutex> lock(m_learnMutex);
        m_learningParamID = paramID;
    }

    m_learningParam.store(param, std::memory_order_release);
    m_isLearning.store(true, std::memory_order_release);

    m_listeners.call([&](Listener& l) {
        l.midiLearnStateChanged(paramID, true);
    });
}

void MidiCCManager::stopLearning()
{
    juce::String prevParamID;
    {
        std::lock_guard<std::mutex> lock(m_learnMutex);
        prevParamID = m_learningParamID;
        m_learningParamID.clear();
    }

    m_learningParam.store(nullptr, std::memory_order_release);
    m_isLearning.store(false, std::memory_order_release);

    if (prevParamID.isNotEmpty())
    {
        m_listeners.call([&](Listener& l) {
            l.midiLearnStateChanged(prevParamID, false);
        });
    }
}

bool MidiCCManager::isLearningParam (const juce::String& paramID) const
{
    if (!isLearning()) return false;
    std::lock_guard<std::mutex> lock(m_learnMutex);
    return m_learningParamID == paramID;
}

juce::String MidiCCManager::getLearningParamID() const
{
    if (!isLearning()) return {};
    std::lock_guard<std::mutex> lock(m_learnMutex);
    return m_learningParamID;
}

//==============================================================================
void MidiCCManager::setMapping (const juce::String& paramID, int ccNumber, int channel)
{
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        if (ccNumber >= 0 && ccNumber <= 127)
        {
            // Remove existing mapping for this CC if any
            for (auto it = m_mappings.begin(); it != m_mappings.end(); )
            {
                if (it->second.ccNumber == ccNumber && it->first != paramID)
                    it = m_mappings.erase(it);
                else
                    ++it;
            }
            m_mappings[paramID] = { paramID, ccNumber, channel };
        }
        else
        {
            m_mappings.erase(paramID);
        }
    }

    rebuildFastLookupTable();
    triggerAsyncUpdate();
}

void MidiCCManager::clearMapping (const juce::String& paramID)
{
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        m_mappings.erase(paramID);
    }

    rebuildFastLookupTable();
    triggerAsyncUpdate();
}

void MidiCCManager::clearAll()
{
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        m_mappings.clear();
    }

    rebuildFastLookupTable();
    triggerAsyncUpdate();
}

int MidiCCManager::getCCForParam (const juce::String& paramID) const
{
    std::lock_guard<std::mutex> lock(m_mapMutex);
    auto it = m_mappings.find(paramID);
    if (it != m_mappings.end())
        return it->second.ccNumber;
    return -1;
}

int MidiCCManager::getChannelForParam (const juce::String& paramID) const
{
    std::lock_guard<std::mutex> lock(m_mapMutex);
    auto it = m_mappings.find(paramID);
    if (it != m_mappings.end())
        return it->second.channel;
    return 0;
}

juce::String MidiCCManager::getParamForCC (int ccNumber, int /*channel*/) const
{
    std::lock_guard<std::mutex> lock(m_mapMutex);
    for (const auto& kv : m_mappings)
    {
        if (kv.second.ccNumber == ccNumber)
            return kv.first;
    }
    return {};
}

std::vector<MidiCCManager::Mapping> MidiCCManager::getAllMappings() const
{
    std::lock_guard<std::mutex> lock(m_mapMutex);
    std::vector<Mapping> result;
    result.reserve(m_mappings.size());
    for (const auto& kv : m_mappings)
        result.push_back(kv.second);
    return result;
}

//==============================================================================
void MidiCCManager::applyStandardMidiCC()
{
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        m_mappings.clear();
        // Standard MIDI Controller defaults
        m_mappings["macroAir"]        = { "macroAir",        11, 0 }; // CC 11 Expression
        m_mappings["macroEmbouchure"] = { "macroEmbouchure",  1, 0 }; // CC 1 Mod Wheel
        m_mappings["blowPressure"]    = { "blowPressure",     2, 0 }; // CC 2 Breath Controller
        m_mappings["macroResonance"]  = { "macroResonance",  74, 0 }; // CC 74 Brightness / Cutoff
        m_mappings["reedStiffness"]   = { "reedStiffness",   71, 0 }; // CC 71 Timbre / Resonance
        m_mappings["delayMix"]        = { "delayMix",        93, 0 }; // CC 93 Chorus / Delay Send
        m_mappings["reverbMix"]       = { "reverbMix",       91, 0 }; // CC 91 Reverb Send
        m_mappings["boreDiameter"]    = { "boreDiameter",    73, 0 }; // CC 73 Sound Attack
    }

    rebuildFastLookupTable();
    triggerAsyncUpdate();
}

//==============================================================================
void MidiCCManager::rebuildFastLookupTable()
{
    std::array<juce::RangedAudioParameter*, 128> tempTable {};
    tempTable.fill(nullptr);

    if (m_apvts != nullptr)
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        for (const auto& kv : m_mappings)
        {
            const auto& m = kv.second;
            if (m.ccNumber >= 0 && m.ccNumber < 128)
            {
                if (auto* p = m_apvts->getParameter(m.paramID))
                    tempTable[m.ccNumber] = p;
            }
        }
    }

    for (int i = 0; i < 128; ++i)
        m_ccLookup[i].store(tempTable[i], std::memory_order_release);
}

//==============================================================================
void MidiCCManager::saveToXml (juce::XmlElement& xml) const
{
    auto* elem = xml.createNewChildElement("MidiCCMappings");
    std::lock_guard<std::mutex> lock(m_mapMutex);

    for (const auto& kv : m_mappings)
    {
        auto* mapElem = elem->createNewChildElement("Map");
        mapElem->setAttribute("paramID", kv.second.paramID);
        mapElem->setAttribute("cc", kv.second.ccNumber);
        mapElem->setAttribute("channel", kv.second.channel);
    }
}

void MidiCCManager::loadFromXml (const juce::XmlElement& xml)
{
    auto* elem = xml.getChildByName("MidiCCMappings");
    if (elem == nullptr) return;

    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        m_mappings.clear();

        for (auto* mapElem : elem->getChildIterator())
        {
            if (mapElem->hasTagName("Map"))
            {
                juce::String paramID = mapElem->getStringAttribute("paramID");
                int cc = mapElem->getIntAttribute("cc", -1);
                int ch = mapElem->getIntAttribute("channel", 0);

                if (paramID.isNotEmpty() && cc >= 0 && cc <= 127)
                    m_mappings[paramID] = { paramID, cc, ch };
            }
        }
    }

    rebuildFastLookupTable();
    triggerAsyncUpdate();
}

//==============================================================================
void MidiCCManager::saveAsGlobalDefault()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("KhaenAudio");
    dir.createDirectory();

    auto file = dir.getChildFile("default_midi_map.xml");
    juce::XmlElement root("KhaenMidiConfig");
    saveToXml(root);
    root.writeTo(file);
}

bool MidiCCManager::loadGlobalDefault()
{
    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("KhaenAudio")
                    .getChildFile("default_midi_map.xml");

    if (!file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr)
    {
        loadFromXml(*xml);
        return true;
    }
    return false;
}

//==============================================================================
void MidiCCManager::addListener (Listener* listener)
{
    m_listeners.add(listener);
}

void MidiCCManager::removeListener (Listener* listener)
{
    m_listeners.remove(listener);
}

void MidiCCManager::handleAsyncUpdate()
{
    int learnedCC = m_learnedCC.exchange(-1, std::memory_order_relaxed);
    if (learnedCC >= 0 && learnedCC <= 127)
    {
        juce::String paramID;
        {
            std::lock_guard<std::mutex> lock(m_learnMutex);
            paramID = m_learningParamID;
            m_learningParamID.clear();
        }

        if (paramID.isNotEmpty())
        {
            int ch = m_learnedChannel.load(std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(m_mapMutex);
            for (auto it = m_mappings.begin(); it != m_mappings.end(); )
            {
                if (it->second.ccNumber == learnedCC)
                    it = m_mappings.erase(it);
                else
                    ++it;
            }
            m_mappings[paramID] = { paramID, learnedCC, ch };
        }

        rebuildFastLookupTable();

        if (paramID.isNotEmpty())
        {
            m_listeners.call([&](Listener& l) {
                l.midiLearnStateChanged(paramID, false);
            });
        }
    }

    m_listeners.call([](Listener& l) {
        l.midiMappingsChanged();
    });
}
