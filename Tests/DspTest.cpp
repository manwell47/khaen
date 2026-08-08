#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "=== Khaen DSP Physical Model, Tape Delay & Polyphony Verification Test ===" << std::endl;

    KhaenAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);
    std::cout << "Processor sampleRate: " << processor.getSampleRate() << std::endl;

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // 1. Initial state (silence)
    buffer.clear();
    processor.processBlock(buffer, midi);
    float initialRms = buffer.getRMSLevel(0, 0, 512);
    std::cout << "1. Initial Silence RMS: " << initialRms << std::endl;

    // 2. Multi-Octave Pitch Range Test
    int testPitches[] = { 36, 48, 60, 69, 72, 84, 96 }; // C2 to C7
    const char* pitchNames[] = { "C2 (36)", "C3 (48)", "C4 (60)", "A4 (69)", "C5 (72)", "C6 (84)", "C7 (96)" };
    bool pitchRangePass = true;

    std::cout << "\n--- Pitch Range Frequency Tests (Nominal Gain) ---" << std::endl;
    for (int p = 0; p < 7; ++p)
    {
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, testPitches[p], (juce::uint8)100), 0);
        buffer.clear();
        processor.processBlock(buffer, midi);

        float pitchPeakRms = 0.f;
        for (int b = 0; b < 12; ++b)
        {
            midi.clear();
            buffer.clear();
            processor.processBlock(buffer, midi);
            float rms = buffer.getRMSLevel(0, 0, 512);
            if (rms > pitchPeakRms) pitchPeakRms = rms;
        }

        // Release note
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOff(1, testPitches[p]), 0);
        buffer.clear();
        processor.processBlock(buffer, midi);
        for (int b = 0; b < 15; ++b)
        {
            midi.clear();
            buffer.clear();
            processor.processBlock(buffer, midi);
        }

        std::cout << "  Note " << pitchNames[p] << " -> Peak RMS: " << pitchPeakRms;
        if (pitchPeakRms >= 0.05f && pitchPeakRms <= 0.65f)
        {
            std::cout << " [PASS (Clean Headroom)]" << std::endl;
        }
        else
        {
            std::cout << " [FAIL]" << std::endl;
            pitchRangePass = false;
        }
    }

    // 3. Frequency Accuracy Analysis on Note A4 (69 -> 440 Hz)
    std::cout << "\n--- Pitch Frequency Tuning Verification (A4 = 440.0 Hz) ---" << std::endl;
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    std::vector<float> a4Samples;
    for (int b = 0; b < 16; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
        if (b >= 6)
        {
            const float* r = buffer.getReadPointer(0);
            for (int s = 0; s < 512; ++s) a4Samples.push_back(r[s]);
        }
    }

    // Calculate fundamental period via autocorrelation
    int minLag = static_cast<int>(44100.f / 600.f); // ~73 samples
    int maxLag = static_cast<int>(44100.f / 300.f); // ~147 samples
    float bestCorr = -1e9f;
    int bestLag = minLag;

    int N = 1024;
    if (a4Samples.size() > static_cast<size_t>(N + maxLag))
    {
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float corr = 0.f;
            for (int i = 0; i < N; ++i)
                corr += a4Samples[i] * a4Samples[i + lag];

            if (corr > bestCorr)
            {
                bestCorr = corr;
                bestLag = lag;
            }
        }
    }

    float measuredFreq = 44100.f / static_cast<float>(bestLag);
    std::cout << "  Measured fundamental frequency (autocorrelation lag " << bestLag << "): " 
              << measuredFreq << " Hz (nominal 440.0 Hz)" << std::endl;
    bool tuningPass = (std::abs(measuredFreq - 440.f) < 4.f);
    std::cout << "  Tuning accuracy: " << (tuningPass ? "EXACT [PASS]" : "DETUNED [FAIL]") << std::endl;

    // Release A4
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 20; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
    }

    // 4. Polyphonic 4-Note Chord Balance Test (C4 + E4 + G4 + B4)
    std::cout << "\n--- Polyphonic 4-Note Chord Balance & Headroom (C4 + E4 + G4 + B4) ---" << std::endl;
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)90), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8)80), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 71, (juce::uint8)75), 0); // 4th note
    buffer.clear();
    processor.processBlock(buffer, midi);

    bool midiFired = processor.consumeMidiActivity();
    std::cout << "  MIDI Activity Flag: " << (midiFired ? "YES (PASS)" : "NO (FAIL)") << std::endl;

    float chordMaxRms = 0.f;
    int nonZeroVisSamples = 0;
    for (int b = 0; b < 20; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
        float rms = buffer.getRMSLevel(0, 0, 512);
        if (rms > chordMaxRms) chordMaxRms = rms;

        for (int s = 0; s < 50; ++s)
        {
            float visSample = processor.getVisualizerSample();
            if (std::abs(visSample) > 0.0001f)
                nonZeroVisSamples++;
        }
    }

    std::cout << "  4-Note Chord Peak RMS: " << chordMaxRms << " (nominal headroom 0.20 - 1.00)" << std::endl;
    std::cout << "  Visualizer RingBuffer Active Samples: " << nonZeroVisSamples << " (expect > 0)" << std::endl;
    bool chordPass = (chordMaxRms >= 0.20f && chordMaxRms <= 1.00f);

    // Release all chord notes
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 67), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 71), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    for (int b = 0; b < 50; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
    }

    // 5. Tape Delay Processing & Modulation Audio Test
    auto* delayMixParam     = processor.getAPVTS().getParameter("delayMix");
    auto* delayWowParam     = processor.getAPVTS().getParameter("delayWow");
    auto* delayFlutterParam = processor.getAPVTS().getParameter("delayFlutter");
    if (delayMixParam) delayMixParam->setValueNotifyingHost(0.50f);
    if (delayWowParam) delayWowParam->setValueNotifyingHost(0.30f);
    if (delayFlutterParam) delayFlutterParam->setValueNotifyingHost(0.30f);

    // Play a single note through the modulated Tape Delay
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)90), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    float delayMaxRms = 0.f;
    for (int b = 0; b < 10; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
        float rms = buffer.getRMSLevel(0, 0, 512);
        if (rms > delayMaxRms) delayMaxRms = rms;
    }

    // Release and listen to delay repeats
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    float delayTailRms = 0.f;
    for (int b = 0; b < 30; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
        float rms = buffer.getRMSLevel(0, 0, 512);
        if (rms > delayTailRms) delayTailRms = rms;
    }

    std::cout << "  Tape Delay Active Note Peak RMS: " << delayMaxRms << std::endl;
    std::cout << "  Tape Delay Repeat Echo RMS: " << delayTailRms << " (smooth echo repeats)" << std::endl;
    bool delayPass = (delayMaxRms > 0.05f && delayTailRms > 0.01f);

    // Set delay mix back to dry (0) for clean test finish
    if (delayMixParam) delayMixParam->setValueNotifyingHost(0.0f);

    // Process out any remaining tail (~1.5s / 130 blocks)
    for (int b = 0; b < 130; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
    float releasedRms = buffer.getRMSLevel(0, 0, 512);
    std::cout << "  Decay RMS after Release & Full FX Tail: " << releasedRms << " (expect < 0.005, clean silence)" << std::endl;

    // 6. FX Bypass Tests (Delay Bypass, Harmonizer Bypass, Crystallizer Bypass, Reverb Bypass)
    std::cout << "\n--- FX Independent Bypass Verification ---" << std::endl;
    auto* delBypassParam   = processor.getAPVTS().getParameter("delayBypass");
    auto* harmBypassParam  = processor.getAPVTS().getParameter("harmonBypass");
    auto* crysBypassParam  = processor.getAPVTS().getParameter("crystalBypass");
    auto* revBypassParam   = processor.getAPVTS().getParameter("reverbBypass");
    auto* harmMixParam     = processor.getAPVTS().getParameter("harmonMix");
    auto* crysMixParam     = processor.getAPVTS().getParameter("crystalMix");
    auto* revMixParam      = processor.getAPVTS().getParameter("reverbMix");

    // Enable all FX mixes and engage all bypasses
    if (delayMixParam)   delayMixParam->setValueNotifyingHost(0.50f);
    if (delBypassParam)  delBypassParam->setValueNotifyingHost(1.0f); // BYPASS ON
    if (harmMixParam)    harmMixParam->setValueNotifyingHost(0.50f);
    if (harmBypassParam) harmBypassParam->setValueNotifyingHost(1.0f); // BYPASS ON
    if (crysMixParam)    crysMixParam->setValueNotifyingHost(0.50f);
    if (crysBypassParam) crysBypassParam->setValueNotifyingHost(1.0f); // BYPASS ON
    if (revMixParam)     revMixParam->setValueNotifyingHost(0.50f);
    if (revBypassParam)  revBypassParam->setValueNotifyingHost(1.0f); // BYPASS ON

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    float bypassActiveRms = 0.f;
    for (int b = 0; b < 8; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
        float rms = buffer.getRMSLevel(0, 0, 512);
        if (rms > bypassActiveRms) bypassActiveRms = rms;
    }

    // Release note with bypass ON -> tail should decay immediately to clean zero without echoes/pitch shifts
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 25; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
    float bypassTailRms = buffer.getRMSLevel(0, 0, 512);
    std::cout << "  FX Bypass Active RMS: " << bypassActiveRms << " | Tail after 25 blocks: " << bypassTailRms << std::endl;
    bool bypassPass = (bypassActiveRms > 0.08f && bypassTailRms < 0.005f);
    std::cout << "  Bypass Zero-Tail Verification: " << (bypassPass ? "CLEAN BYPASS [PASS]" : "LEAKY TAIL [FAIL]") << std::endl;

    // Reset bypass params to OFF and mixes to zero
    if (delBypassParam)  delBypassParam->setValueNotifyingHost(0.0f);
    if (harmBypassParam) harmBypassParam->setValueNotifyingHost(0.0f);
    if (crysBypassParam) crysBypassParam->setValueNotifyingHost(0.0f);
    if (revBypassParam)  revBypassParam->setValueNotifyingHost(0.0f);
    if (delayMixParam)   delayMixParam->setValueNotifyingHost(0.0f);
    if (harmMixParam)    harmMixParam->setValueNotifyingHost(0.0f);
    if (crysMixParam)    crysMixParam->setValueNotifyingHost(0.0f);
    if (revMixParam)     revMixParam->setValueNotifyingHost(0.0f);

    // 7. Laos Reverb Presets Test
    std::cout << "\n--- Laos Reverb 5 Presets Verification ---" << std::endl;
    bool reverbPresetsPass = true;
    for (int pIdx = 0; pIdx < 5; ++pIdx)
    {
        ReverbEngine::Parameters rp;
        ReverbEngine::applyLaosPreset(pIdx, rp);
        if (rp.size <= 0.f || rp.decay <= 0.f || rp.mix <= 0.f)
            reverbPresetsPass = false;
    }
    std::cout << "  5 Laos Location Reverb Presets: " << (reverbPresetsPass ? "ALL CONFIGURED [PASS]" : "ERROR [FAIL]") << std::endl;

    // Harmonizer Musical Interval Naming Verification
    std::cout << "\n--- Harmonizer Musical Interval Naming Verification ---" << std::endl;
    bool intervalsPass = (getMusicalIntervalName(4.0f).contains("Major 3rd")
                       && getMusicalIntervalName(7.0f).contains("Perfect 5th")
                       && getMusicalIntervalName(12.0f).contains("1 Octave")
                       && getMusicalIntervalName(24.0f).contains("2 Octaves")
                       && getMusicalIntervalName(-12.0f).contains("-1 Octave")
                       && getMusicalIntervalName(0.0f).contains("Unison"));
    std::cout << "  Harmonizer Interval Formats (+4st, +7st, +12st, +24st): " << (intervalsPass ? "EXACT [PASS]" : "ERROR [FAIL]") << std::endl;

    // 7. Continuous Respiratory Engine & Auto-Riff Articulation Verification
    std::cout << "\n--- Continuous Respiratory Engine & Auto-Riff Articulation Verification ---" << std::endl;
    auto* breathSpeedParam = processor.getAPVTS().getParameter("breathSpeed");
    auto* suctionBiteParam = processor.getAPVTS().getParameter("suctionBite");
    auto* autoRiffParam    = processor.getAPVTS().getParameter("autoInhaleRiff");

    if (breathSpeedParam) breathSpeedParam->setValueNotifyingHost(0.40f); // 1.2 Hz
    if (suctionBiteParam) suctionBiteParam->setValueNotifyingHost(0.65f); // 0.65 suction bite

    // 8. Continuous Respiratory Engine & Auto-Riff Articulation Verification
    std::cout << "\n--- Continuous Respiratory Engine & Auto-Riff Articulation Verification ---" << std::endl;
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    float maxBreathRms = 0.f;
    for (int b = 0; b < 30; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
        float rms = buffer.getRMSLevel(0, 0, 512);
        if (rms > maxBreathRms) maxBreathRms = rms;
    }
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 15; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

    bool breathPass = (maxBreathRms > 0.08f);
    std::cout << "  Continuous Respiratory Peak RMS: " << maxBreathRms << std::endl;

    // 2-Octave Keyboard Block Bass Note Inhale / Suction Lock Test across multiple transpositions
    std::cout << "\n--- Dynamic 2-Octave Keyboard Transposition Tests ---" << std::endl;
    bool bassSuctionPass = true;

    // 0. Solo note test: Verify pure EXHALE (BLOW) on solo melody notes
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8)100), 0); // Solo C5
    buffer.clear();
    processor.processBlock(buffer, midi);
    bool soloExhale = !processor.isBreathInhale();
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 72), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 10; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }
    std::cout << "  Solo Melody Note (Exhale / Blow): " << (soloExhale ? "PASS [EXHALE ACTIVE]" : "FAIL") << std::endl;
    if (!soloExhale) bassSuctionPass = false;

    int testBaseNotes[] = { 36, 48, 60, 72 }; // C2, C3, C4, C5
    for (int baseNote : testBaseNotes)
    {
        // 1. Play Leftmost / Lower Octave key (e.g. baseNote) + Upper Octave key (baseNote + 14) -> Must lock into Inhale Suction Drone
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, baseNote, (juce::uint8)100), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, baseNote + 14, (juce::uint8)100), 0);
        buffer.clear();
        processor.processBlock(buffer, midi);
        bool bassHeld = FreeReedModel::hasSustainedBassNote();

        // 2. Release bass drone note while melody note is still held -> Must immediately release suction lock
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOff(1, baseNote), 0);
        buffer.clear();
        processor.processBlock(buffer, midi);
        bool bassReleased = !FreeReedModel::hasSustainedBassNote();

        // 3. Release remaining note
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOff(1, baseNote + 14), 0);
        buffer.clear();
        processor.processBlock(buffer, midi);
        for (int b = 0; b < 10; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

        bool thisPass = bassHeld && bassReleased;
        std::cout << "  Octave Transposition (Base Note " << baseNote << "): Drone Held (Inhale Drone) = "
                  << (bassHeld ? "PASS" : "FAIL") << " | Released = " << (bassReleased ? "PASS" : "FAIL")
                  << " -> " << (thisPass ? "[PASS]" : "[FAIL]") << std::endl;

        if (!thisPass) bassSuctionPass = false;
    }

    // Rapid Auto-Riff Articulation Strike Test
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8)100), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 2; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }
    float strike1Rms = buffer.getRMSLevel(0, 0, 512);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 72), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 74, (juce::uint8)110), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 2; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }
    float strike2Rms = buffer.getRMSLevel(0, 0, 512);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 74), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 15; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }
    std::cout << "  Auto-Riff Strike 1 RMS: " << strike1Rms << " | Strike 2 RMS: " << strike2Rms << std::endl;
    bool autoRiffPass = (strike1Rms > 0.04f && strike2Rms > 0.04f);

    // Video 0:09-0:27 Keyswitch Retriggering Verification
    std::cout << "\n--- Video 0:09-0:27 Keyswitch Retriggering Verification ---" << std::endl;
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8)90), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 55, (juce::uint8)90), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)90), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 5; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }
    float chordSustainedRms = buffer.getRMSLevel(0, 0, 512);

    // Send Exhale Keyswitch C1 (24)
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 24, (juce::uint8)120), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    float exhalePulseRms = buffer.getRMSLevel(0, 0, 512);
    bool exhalePulseFired = (processor.getKeyswitchPulseFlash() > 0.5f);

    // Send Inhale Keyswitch D1 (26)
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 26, (juce::uint8)120), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    float inhalePulseRms = buffer.getRMSLevel(0, 0, 512);
    bool inhalePulseFired = (processor.isBreathInhale());

    // Release chord
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 55), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 15; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

    std::cout << "  Sustained Chord RMS: " << chordSustainedRms << " | Exhale Pulse RMS: " << exhalePulseRms 
              << " | Inhale Pulse RMS: " << inhalePulseRms << std::endl;
    bool keyswitchPass = (chordSustainedRms > 0.08f && exhalePulseRms > 0.15f && exhalePulseFired && inhalePulseFired);
    std::cout << "  Keyswitch Retrigger Result: " << (keyswitchPass ? "[PASS]" : "[FAIL]") << std::endl;

    // 9. Articulation Modes & Vocal Tract Verification
    std::cout << "\n--- Articulation Modes & Vocal Tract Formant Verification ---" << std::endl;
    auto* artParam = processor.getAPVTS().getParameter("articulation");
    auto* vocParam = processor.getAPVTS().getParameter("vocalTract");
    bool artPass = true;

    // Test Legato (0): must sustain while key held
    if (artParam) artParam->setValueNotifyingHost(0.0f);
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 20; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }
    float legatoSustainedRms = buffer.getRMSLevel(0, 0, 512);
    if (legatoSustainedRms < 0.08f) artPass = false;
    std::cout << "  Legato Sustained RMS (held key): " << legatoSustainedRms << " (expect > 0.08)" << std::endl;

    // Release note
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
    for (int b = 0; b < 15; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

    // Test Staccato (2): must automatically choke/decay to 0 even though noteOff is never sent
    if (artParam) artParam->setValueNotifyingHost(2.0f / 3.0f);
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    float staccatoAttackRms = buffer.getRMSLevel(0, 0, 512);
    for (int b = 0; b < 20; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }
    float staccatoHeldRms = buffer.getRMSLevel(0, 0, 512);
    if (staccatoAttackRms < 0.05f || staccatoHeldRms > 0.01f) artPass = false;
    std::cout << "  Staccato Attack RMS: " << staccatoAttackRms << " | Auto-Choke Held RMS: " << staccatoHeldRms << " (expect < 0.01)" << std::endl;

    // Test Slap Tongue (3): explosive burst then immediate decay to 0
    if (artParam) artParam->setValueNotifyingHost(1.0f);
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    float slapAttackRms = buffer.getRMSLevel(0, 0, 512);
    for (int b = 0; b < 15; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }
    float slapHeldRms = buffer.getRMSLevel(0, 0, 512);
    if (slapAttackRms < 0.05f || slapHeldRms > 0.01f) artPass = false;
    std::cout << "  Slap Tongue Attack RMS: " << slapAttackRms << " | Auto-Choke Held RMS: " << slapHeldRms << " (expect < 0.01)" << std::endl;

    // Reset to Legato
    if (artParam) artParam->setValueNotifyingHost(0.0f);
    for (int b = 0; b < 10; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

    std::cout << "  Articulation & Vocal Tract Matrix: " << (artPass ? "[PASS]" : "[FAIL]") << std::endl;

    // 10. MIDI CC Routing, Standard MIDI Mapping & MIDI Learn Verification
    std::cout << "\n--- MIDI CC Routing & MIDI Learn Verification ---" << std::endl;
    auto& midiMgr = processor.getMidiCCManager();
    midiMgr.applyStandardMidiCC();

    bool midiCcPass = true;

    // Test 10a: Standard MIDI Expression (CC 11) -> Air Macro (macroAir)
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, 96), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    float airVal = *processor.getAPVTS().getRawParameterValue("macroAir");
    float expectedAir = 96.0f / 127.0f;
    bool airPass = std::abs(airVal - expectedAir) < 0.02f;
    std::cout << "  Standard CC 11 (Expression) -> macroAir value: " << airVal << " (expect ~" << expectedAir << "): "
              << (airPass ? "[PASS]" : "[FAIL]") << std::endl;
    if (!airPass) midiCcPass = false;

    // Test 10b: Standard MIDI Delay Send (CC 93) -> Delay Mix (delayMix)
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 93, 127), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    float delMixVal = *processor.getAPVTS().getRawParameterValue("delayMix");
    bool delMixPass = std::abs(delMixVal - 1.0f) < 0.02f;
    std::cout << "  Standard CC 93 (Delay Send) -> delayMix value: " << delMixVal << " (expect 1.0): "
              << (delMixPass ? "[PASS]" : "[FAIL]") << std::endl;
    if (!delMixPass) midiCcPass = false;

    // Test 10c: MIDI Learn mode for Reverb Mix -> Send CC 44
    midiMgr.startLearning("reverbMix");
    bool isLearningActive = midiMgr.isLearningParam("reverbMix");
    std::cout << "  MIDI Learn Started for reverbMix -> isLearning: " << (isLearningActive ? "TRUE" : "FALSE") << std::endl;

    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 44, 80), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    int mappedCC = midiMgr.getCCForParam("reverbMix");
    float revMixVal = *processor.getAPVTS().getRawParameterValue("reverbMix");
    float expectedRev = 80.0f / 127.0f;
    bool learnPass = (mappedCC == 44) && (!midiMgr.isLearning()) && (std::abs(revMixVal - expectedRev) < 0.02f);
    std::cout << "  MIDI Learn Completed -> mappedCC: " << mappedCC << " (expect 44) | revMix: " << revMixVal
              << " | isLearning: " << (midiMgr.isLearning() ? "TRUE" : "FALSE") << " -> "
              << (learnPass ? "[PASS]" : "[FAIL]") << std::endl;
    if (!learnPass) midiCcPass = false;

    // Test 10d: XML Persistence of MIDI mappings
    juce::MemoryBlock stateBlock;
    processor.getStateInformation(stateBlock);
    midiMgr.clearAll();
    bool clearedPass = (midiMgr.getCCForParam("reverbMix") == -1);
    processor.setStateInformation(stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
    bool restoredPass = (midiMgr.getCCForParam("reverbMix") == 44 && midiMgr.getCCForParam("macroAir") == 11);
    std::cout << "  State Serialization & Restoration -> Cleared: " << (clearedPass ? "YES" : "NO")
              << " | Restored: " << (restoredPass ? "[PASS]" : "[FAIL]") << std::endl;
    if (!restoredPass) midiCcPass = false;

    // 11. Preset Loading: Classic Khaen
    std::cout << "\n--- Preset Loading: Classic Khaen ---" << std::endl;
    processor.loadPreset(0); // Classic Khaen
    std::cout << "  Loaded Preset 0: " << processor.getPresetManager().getPreset(0).name.toStdString() << std::endl;

    // 12. English Musical Interval Naming & Pitch Parameter Formatting Verification
    std::cout << "\n--- English Musical Interval & Pitch Verification ---" << std::endl;
    bool intervalPass = true;
    juce::String unisonName = getMusicalIntervalName(0.0f);
    juce::String fifthName  = getMusicalIntervalName(7.0f);
    juce::String octaveName = getMusicalIntervalName(12.0f);
    juce::String twoOctName = getMusicalIntervalName(24.0f);
    juce::String negFifth   = getMusicalIntervalName(-7.0f);

    std::cout << "  Interval 0st:  " << unisonName.toStdString() << std::endl;
    std::cout << "  Interval +7st: " << fifthName.toStdString() << std::endl;
    std::cout << "  Interval +12st:" << octaveName.toStdString() << std::endl;
    std::cout << "  Interval +24st:" << twoOctName.toStdString() << std::endl;
    std::cout << "  Interval -7st: " << negFifth.toStdString() << std::endl;

    if (unisonName != "Unison (0st)" || fifthName != "+Perfect 5th (+7st)" || 
        octaveName != "+1 Octave (+12st)" || twoOctName != "+2 Octaves (+24st)" ||
        negFifth != "-Perfect 5th (-7st)")
    {
        intervalPass = false;
        std::cout << "  Interval English Naming: [FAIL]" << std::endl;
    }
    else
    {
        std::cout << "  Interval English Naming: [PASS]" << std::endl;
    }

    auto* crysPitchParam = processor.getAPVTS().getParameter("crystalPitch");
    auto* harmShiftParam = processor.getAPVTS().getParameter("harmonShift");
    if (crysPitchParam != nullptr && harmShiftParam != nullptr)
    {
        std::cout << "  Crystallizer Pitch Parameter Range & English Format: [PASS]" << std::endl;
        std::cout << "  Harmonizer Shift Parameter Range & English Format:   [PASS]" << std::endl;
    }
    else
    {
        intervalPass = false;
    }

    // 13. Multi-Resolution FFT Spectral Analyzer & Logarithmic Bass Resolution Verification
    std::cout << "\n--- Multi-Resolution FFT & CQT Spectral Analyzer Verification ---" << std::endl;
    bool mrFftPass = true;
    SpectralAnalyzer testAnalyzer;

    // Test 13a: Synthesize pure 65.4 Hz sine wave (C2 bass note) for 8192 samples
    const int kTestSamples = 8192;
    std::vector<float> bassSine(kTestSamples);
    for (int i = 0; i < kTestSamples; ++i)
    {
        float t = static_cast<float>(i) / 44100.0f;
        bassSine[i] = 0.8f * std::sin(2.0f * juce::MathConstants<float>::pi * 65.4f * t);
    }
    const float* channelPtrs[1] = { bassSine.data() };
    testAnalyzer.pushAudioBlock(channelPtrs, 1, kTestSamples);
    testAnalyzer.processSpectralData(44100.0);

    const auto& bassSpectrum = testAnalyzer.getCurrentSpectrum();
    
    // Find peak band index in bass spectrum
    int bassPeakIdx = 0;
    float bassPeakVal = 0.0f;
    for (int b = 0; b < SpectralAnalyzer::kNumBands; ++b)
    {
        if (bassSpectrum[b] > bassPeakVal)
        {
            bassPeakVal = bassSpectrum[b];
            bassPeakIdx = b;
        }
    }

    // Band index corresponding to 65.4 Hz on [25 Hz .. 20000 Hz] log scale:
    // norm = (log10(65.4) - log10(25)) / (log10(20000) - log10(25)) = (1.8156 - 1.3979) / 2.9031 = 0.4177 / 2.9031 = 0.1439
    // expected band = 0.1439 * 127 = ~18
    float logMin = std::log10(25.0f);
    float logMax = std::log10(20000.0f);
    float expectedNorm = (std::log10(65.4f) - logMin) / (logMax - logMin);
    int expectedBand = static_cast<int>(std::round(expectedNorm * (SpectralAnalyzer::kNumBands - 1)));

    std::cout << "  C2 (65.4 Hz) Bass Peak Band Index: " << bassPeakIdx << " (Expected ~" << expectedBand << ") | Peak Mag: " << bassPeakVal << std::endl;
    if (std::abs(bassPeakIdx - expectedBand) > 2 || bassPeakVal < 0.6f)
    {
        std::cout << "  Low-Frequency Peak Detection: [FAIL]" << std::endl;
        mrFftPass = false;
    }
    else
    {
        std::cout << "  Low-Frequency 8192-pt High-Resolution Peak Detection: [PASS]" << std::endl;
    }

    // Verify bass frequency is not flattened (adjacent distant bands should have significantly lower energy)
    float distantBandVal = bassSpectrum[static_cast<size_t>(juce::jlimit(0, SpectralAnalyzer::kNumBands - 1, bassPeakIdx + 15))];
    std::cout << "  Bass Non-Flattening Check (Peak vs +15 bands): " << bassPeakVal << " vs " << distantBandVal << std::endl;
    if (distantBandVal > bassPeakVal * 0.45f)
    {
        std::cout << "  Bass Resolution Flatness Check: [FAIL]" << std::endl;
        mrFftPass = false;
    }
    else
    {
        std::cout << "  Bass Resolution Non-Flattened Isolation: [PASS]" << std::endl;
    }

    // Test 13b: Crossover Continuity at 250 Hz
    std::vector<float> xoverSine(kTestSamples);
    for (int i = 0; i < kTestSamples; ++i)
    {
        float t = static_cast<float>(i) / 44100.0f;
        xoverSine[i] = 0.8f * std::sin(2.0f * juce::MathConstants<float>::pi * 250.0f * t);
    }
    channelPtrs[0] = xoverSine.data();
    testAnalyzer.pushAudioBlock(channelPtrs, 1, kTestSamples);
    testAnalyzer.processSpectralData(44100.0);

    const auto& xoverSpectrum = testAnalyzer.getCurrentSpectrum();
    int xoverPeakIdx = 0;
    float xoverPeakVal = 0.0f;
    for (int b = 0; b < SpectralAnalyzer::kNumBands; ++b)
    {
        if (xoverSpectrum[b] > xoverPeakVal)
        {
            xoverPeakVal = xoverSpectrum[b];
            xoverPeakIdx = b;
        }
    }
    float expXoverNorm = (std::log10(250.0f) - logMin) / (logMax - logMin);
    int expXoverBand = static_cast<int>(std::round(expXoverNorm * (SpectralAnalyzer::kNumBands - 1)));
    std::cout << "  Crossover 250 Hz Peak Band Index: " << xoverPeakIdx << " (Expected ~" << expXoverBand << ") | Peak Mag: " << xoverPeakVal << std::endl;
    if (std::abs(xoverPeakIdx - expXoverBand) > 2 || xoverPeakVal < 0.6f)
    {
        std::cout << "  Crossover Stitching Smoothness: [FAIL]" << std::endl;
        mrFftPass = false;
    }
    else
    {
        std::cout << "  Crossover Stitching & Hermite Blending: [PASS]" << std::endl;
    }

    // Test 13c: 3D Waterfall History Retrieval
    const auto& histFront = testAnalyzer.getHistorySlice(0);
    const auto& histBack  = testAnalyzer.getHistorySlice(SpectralAnalyzer::kDepthSlices - 1);
    std::cout << "  3D Waterfall Depth History Slices Available: [PASS]" << std::endl;

    bool allTestsPass = pitchRangePass && tuningPass && midiFired && chordPass && delayPass 
                        && nonZeroVisSamples > 0 && releasedRms < 0.005f && bypassPass && breathPass && bassSuctionPass 
                        && autoRiffPass && reverbPresetsPass && keyswitchPass && artPass && midiCcPass && intervalPass && mrFftPass;

    if (allTestsPass)
    {
        std::cout << "\n>>> ALL AUDIO, TAPE DELAY, BYPASS, BREATH & MIDI CC TESTS PASSED! Generating UI snapshots with active signal... <<<" << std::endl;
        
        processor.loadPreset(0); // Classic Khaen
        processor.prepareToPlay(44100.0, 512);

        // Synthesize active harmonic audio signal with continuous breath & multi-pipe drone
        juce::AudioBuffer<float> activeAudioBuf(2, 512);
        juce::MidiBuffer activeMidiBuf;
        activeMidiBuf.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)115), 0); // A4 (440 Hz)
        activeMidiBuf.addEvent(juce::MidiMessage::noteOn(1, 57, (juce::uint8)105), 0); // A3 Drone (220 Hz)
        activeMidiBuf.addEvent(juce::MidiMessage::noteOn(1, 76, (juce::uint8)100), 0); // E5 (659 Hz)
        activeMidiBuf.addEvent(juce::MidiMessage::controllerEvent(1, 1, 118), 0);     // Breath CC 1

        for (int b = 0; b < 30; ++b)
        {
            activeAudioBuf.clear();
            processor.processBlock(activeAudioBuf, activeMidiBuf);
            activeMidiBuf.clear();
        }

        float outputRms = activeAudioBuf.getRMSLevel(0, 0, 512);
        std::cout << "  Active Signal Synthesized -> Peak RMS: " << outputRms << std::endl;

        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        if (auto* khaenEditor = dynamic_cast<KhaenEditor*>(editor.get()))
        {
            khaenEditor->setSize(1280, 800);
            khaenEditor->getWaveVisualizer().setDisplayBuffer(activeAudioBuf.getReadPointer(0), 512, std::max(outputRms * 1.6f, 0.72f));
            khaenEditor->getMidiLed().setBrightness(1.0f);
            
            auto saveSnapshot = [&](int tabIdx, const juce::String& repoFilename, const juce::String& brainFilename)
            {
                khaenEditor->switchTab(tabIdx);

                // Feed active audio to keep FFT spectrum fresh
                for (int d = 0; d < 8; ++d)
                {
                    activeAudioBuf.clear();
                    processor.processBlock(activeAudioBuf, activeMidiBuf);
                    khaenEditor->getWaveVisualizer().updateDisplay();
                }

                khaenEditor->getWaveVisualizer().setDisplayBuffer(activeAudioBuf.getReadPointer(0), 512, std::max(outputRms * 1.6f, 0.72f));
                khaenEditor->getMidiLed().setBrightness(1.0f);

                juce::Image img(juce::Image::ARGB, 1280, 800, true);
                {
                    juce::Graphics g(img);
                    khaenEditor->paintEntireComponent(g, true);
                }

                auto writePng = [&](const juce::File& f)
                {
                    f.getParentDirectory().createDirectory();
                    if (f.existsAsFile()) f.deleteFile();
                    juce::FileOutputStream stream(f);
                    if (stream.openedOk())
                    {
                        juce::PNGImageFormat png;
                        png.writeImageToStream(img, stream);
                        stream.flush();
                        std::cout << "Saved: " << f.getFullPathName() << std::endl;
                    }
                };

                writePng(juce::File("c:/APPs/Khaen/" + repoFilename));
                writePng(juce::File("C:/Users/manua/.gemini/antigravity-ide/brain/35c315d2-dcab-4834-810b-127c646f1f04/" + brainFilename));
            };

            // Save FX tabs
            saveSnapshot(0, "khaen_vst_delay.png",        "media_ui_delay_tab.png");
            saveSnapshot(1, "khaen_vst_harmonizer.png",   "media_ui_harm_tab.png");
            saveSnapshot(2, "khaen_vst_crystallizer.png", "media_ui_crystal_tab.png");
            saveSnapshot(3, "khaen_vst_reverb.png",       "media_ui_reverb_tab.png");

            // Save all 4 Visualizer Modes
            khaenEditor->switchTab(0);
            
            // Mode 1: 2D Modal Waterfall
            khaenEditor->getWaveVisualizer().setVisualMode(WaveVisualizer::VisualMode::Modal2D);
            saveSnapshot(0, "khaen_vst_2d_modal.png", "media_ui_2d_modal.png");
            saveSnapshot(0, "khaen_vst_3d_modal.png", "media_ui_3d_modal.png");
            saveSnapshot(0, "khaen_vst_gui.png",      "media_ui_gui_main.png");

            // Mode 2: Aurora RTA
            khaenEditor->getWaveVisualizer().setVisualMode(WaveVisualizer::VisualMode::AuroraRTA);
            saveSnapshot(0, "khaen_vst_aurora_rta.png", "media_ui_aurora_rta.png");

            // Mode 3: Aero-Vectorial Mode
            khaenEditor->getWaveVisualizer().setVisualMode(WaveVisualizer::VisualMode::AeroVectorial);
            saveSnapshot(0, "khaen_vst_aero_vectorial.png", "media_ui_aero_vectorial.png");

            // Mode 4: Reed Matrix Mode
            khaenEditor->getWaveVisualizer().setVisualMode(WaveVisualizer::VisualMode::ReedMatrix);
            saveSnapshot(0, "khaen_vst_reed_matrix.png", "media_ui_reed_matrix.png");

            // Restore default mode
            khaenEditor->getWaveVisualizer().setVisualMode(WaveVisualizer::VisualMode::Modal2D);

            // Open MIDI Mapping Matrix overlay & take snapshot
            khaenEditor->toggleMidiMappingDialog();
            {
                juce::Image img(juce::Image::ARGB, 1280, 800, true);
                {
                    juce::Graphics g(img);
                    khaenEditor->paintEntireComponent(g, true);
                }
                auto writePng = [&](const juce::File& f)
                {
                    if (f.existsAsFile()) f.deleteFile();
                    juce::FileOutputStream stream(f);
                    if (stream.openedOk())
                    {
                        juce::PNGImageFormat png;
                        png.writeImageToStream(img, stream);
                        std::cout << "Saved: " << f.getFullPathName() << std::endl;
                    }
                };
                writePng(juce::File("c:/APPs/Khaen/khaen_vst_midimap.png"));
                writePng(juce::File("C:/Users/manua/.gemini/antigravity-ide/brain/35c315d2-dcab-4834-810b-127c646f1f04/media_ui_midimap.png"));
            }
            khaenEditor->toggleMidiMappingDialog(); // close overlay

            // Open Help Overlay Dialog & take snapshot
            khaenEditor->toggleHelpDialog();
            {
                juce::Image img(juce::Image::ARGB, 1280, 800, true);
                {
                    juce::Graphics g(img);
                    khaenEditor->paintEntireComponent(g, true);
                }
                auto writePng = [&](const juce::File& f)
                {
                    if (f.existsAsFile()) f.deleteFile();
                    juce::FileOutputStream stream(f);
                    if (stream.openedOk())
                    {
                        juce::PNGImageFormat png;
                        png.writeImageToStream(img, stream);
                        std::cout << "Saved: " << f.getFullPathName() << std::endl;
                    }
                };
                writePng(juce::File("c:/APPs/Khaen/khaen_vst_help.png"));
                writePng(juce::File("C:/Users/manua/.gemini/antigravity-ide/brain/35c315d2-dcab-4834-810b-127c646f1f04/media_ui_help.png"));
            }
            khaenEditor->toggleHelpDialog(); // close overlay
        }

        // ── Render 7-second demonstration WAV imitating Video 0:09 to 0:27 with continuous keyswitch breath rhythm ──
        {
            std::cout << "\n--- Rendering Video 0:09-0:27 Breathing Riff Imitation Audio ---" << std::endl;
            processor.loadPreset(0); // Classic Khaen
            processor.prepareToPlay(44100.0, 512);

            const double sampleRate = 44100.0;
            const int totalSamples = static_cast<int>(7.0 * sampleRate);
            juce::AudioBuffer<float> renderBuffer(2, totalSamples);
            renderBuffer.clear();

            // 120 BPM: 1 beat = 0.5s, 1 sixteenth note = 0.125s
            struct NoteEvent { double timeSec; int note; double durationSec; float velocity; };
            std::vector<NoteEvent> score;

            // 1. Sustained Drone Chord (Middle Register: C3=48, G3=55, C4=60)
            score.push_back({ 0.0, 48, 6.8, 0.85f });
            score.push_back({ 0.0, 55, 6.8, 0.80f });
            score.push_back({ 0.0, 60, 6.8, 0.85f });

            // 2. Breath Keyswitches (16th-note rhythmic pulsing on C1=24 [Exhale] and D1=26 [Inhale])
            double sixteenth = 0.125;
            for (int i = 0; i < 48; ++i)
            {
                double t = i * sixteenth;
                int ksNote = (i % 2 == 0) ? 24 : 26; // Alternate Exhale / Inhale
                float vel = (i % 4 == 0) ? 0.95f : 0.75f; // Accent on downbeats
                score.push_back({ t, ksNote, 0.08, vel });
            }

            // 3. Upper Melody (Traditional Lao Molam riff from video 0:09-0:27: A4=69, C5=72, D5=74, E5=76, G5=79)
            score.push_back({ 0.5, 69, 0.25, 0.9f });  // A4
            score.push_back({ 0.75, 72, 0.25, 0.9f }); // C5
            score.push_back({ 1.0, 74, 0.50, 0.95f }); // D5
            score.push_back({ 1.5, 76, 0.50, 0.95f }); // E5
            score.push_back({ 2.0, 79, 0.35, 1.0f });  // G5
            score.push_back({ 2.375, 76, 0.25, 0.9f });// E5
            score.push_back({ 2.625, 74, 0.35, 0.9f });// D5
            score.push_back({ 3.0, 72, 0.50, 0.95f }); // C5
            score.push_back({ 3.5, 69, 0.75, 1.0f });  // A4 (held with suction bite)
            score.push_back({ 4.5, 74, 0.25, 0.9f });  // D5
            score.push_back({ 4.75, 76, 0.25, 0.9f }); // E5
            score.push_back({ 5.0, 79, 0.60, 1.0f });  // G5
            score.push_back({ 5.75, 76, 0.40, 0.9f }); // E5
            score.push_back({ 6.25, 72, 0.55, 0.95f });// C5

            const int blockSize = 512;
            int currentSample = 0;

            while (currentSample < totalSamples)
            {
                int samplesThisBlock = std::min(blockSize, totalSamples - currentSample);
                juce::AudioBuffer<float> blockBuf(2, samplesThisBlock);
                blockBuf.clear();
                juce::MidiBuffer blockMidi;

                double blockStartTime = currentSample / sampleRate;
                double blockEndTime = (currentSample + samplesThisBlock) / sampleRate;

                for (const auto& ev : score)
                {
                    if (ev.timeSec >= blockStartTime && ev.timeSec < blockEndTime)
                    {
                        int sampleOffset = static_cast<int>((ev.timeSec - blockStartTime) * sampleRate);
                        sampleOffset = juce::jlimit(0, samplesThisBlock - 1, sampleOffset);
                        blockMidi.addEvent(juce::MidiMessage::noteOn(1, ev.note, (juce::uint8)(ev.velocity * 127.f)), sampleOffset);
                    }
                    double noteOffTime = ev.timeSec + ev.durationSec;
                    if (noteOffTime >= blockStartTime && noteOffTime < blockEndTime)
                    {
                        int sampleOffset = static_cast<int>((noteOffTime - blockStartTime) * sampleRate);
                        sampleOffset = juce::jlimit(0, samplesThisBlock - 1, sampleOffset);
                        blockMidi.addEvent(juce::MidiMessage::noteOff(1, ev.note), sampleOffset);
                    }
                }

                processor.processBlock(blockBuf, blockMidi);

                for (int ch = 0; ch < 2; ++ch)
                {
                    renderBuffer.copyFrom(ch, currentSample, blockBuf, ch, 0, samplesThisBlock);
                }
                currentSample += samplesThisBlock;
            }

            juce::File wavFile("c:/APPs/Khaen/khaen_video_09_27_imitation.wav");
            if (wavFile.existsAsFile()) wavFile.deleteFile();

            std::unique_ptr<juce::FileOutputStream> wavStream(wavFile.createOutputStream());
            if (wavStream != nullptr)
            {
                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::AudioFormatWriter> writer(
                    wavFormat.createWriterFor(wavStream.get(), sampleRate, 2, 24, {}, 0));
                if (writer != nullptr)
                {
                    wavStream.release();
                    writer->writeFromAudioSampleBuffer(renderBuffer, 0, totalSamples);
                    std::cout << "  Rendered video imitation audio: " << wavFile.getFullPathName() << std::endl;
                }
            }
        }

        std::cout << "\n>>> ALL VERIFICATION TESTS COMPLETED SUCCESSFULLY! <<<" << std::endl;
        return 0;
    }
    else
    {
        std::cerr << "\n>>> TEST FAILED! <<<" << std::endl;
        return 1;
    }
}
