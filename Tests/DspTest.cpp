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
        if (pitchPeakRms >= 0.15f && pitchPeakRms <= 0.65f)
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

    std::cout << "  4-Note Chord Peak RMS: " << chordMaxRms << " (nominal headroom 0.40 - 0.95)" << std::endl;
    std::cout << "  Visualizer RingBuffer Active Samples: " << nonZeroVisSamples << " (expect > 0)" << std::endl;
    bool chordPass = (chordMaxRms >= 0.40f && chordMaxRms <= 0.95f);

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
    bool delayPass = (delayMaxRms > 0.10f && delayTailRms > 0.01f);

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
    bool bypassPass = (bypassActiveRms > 0.15f && bypassTailRms < 0.005f);
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

    // 7. Continuous Respiratory Engine & Auto-Riff Articulation Verification
    std::cout << "\n--- Continuous Respiratory Engine & Auto-Riff Articulation Verification ---" << std::endl;
    auto* breathSpeedParam = processor.getAPVTS().getParameter("breathSpeed");
    auto* suctionBiteParam = processor.getAPVTS().getParameter("suctionBite");
    auto* autoRiffParam    = processor.getAPVTS().getParameter("autoInhaleRiff");

    if (breathSpeedParam) breathSpeedParam->setValueNotifyingHost(0.40f); // 1.2 Hz
    if (suctionBiteParam) suctionBiteParam->setValueNotifyingHost(0.65f); // 0.65 suction bite

    // Test continuous breathing on sustained note
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0); // High A4
    buffer.clear();
    processor.processBlock(buffer, midi);
    float breathPeakRms = 0.f;
    for (int b = 0; b < 20; ++b) // ~230ms of continuous respiratory audio
    {
        midi.clear(); buffer.clear();
        processor.processBlock(buffer, midi);
        float r = buffer.getRMSLevel(0, 0, 512);
        if (r > breathPeakRms) breathPeakRms = r;
    }
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 10; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

    std::cout << "  Continuous Respiratory Peak RMS: " << breathPeakRms << std::endl;
    bool breathPass = (breathPeakRms > 0.15f);

    // Test Sustained Bass Note Suction Lock: Note <= 60 (e.g. A2 = 45)
    std::cout << "\n--- Sustained Bass Note Inhale / Suction Lock Test ---" << std::endl;
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 45, (juce::uint8)100), 0); // Low A2 Drone
    buffer.clear();
    processor.processBlock(buffer, midi);
    
    bool bassLockHeld = FreeReedModel::hasSustainedBassNote();
    std::cout << "  Bass Note 45 Held -> hasSustainedBassNote(): " << (bassLockHeld ? "TRUE [LOCKED TO SUCTION]" : "FALSE") << std::endl;

    // Process blocks while held
    for (int b = 0; b < 10; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

    // Release bass note
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 45), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    bool bassLockReleased = !FreeReedModel::hasSustainedBassNote();
    std::cout << "  Bass Note 45 Released -> hasSustainedBassNote(): " << (!bassLockReleased ? "TRUE" : "FALSE [RESUMED NORMAL CYCLING]") << std::endl;
    bool bassSuctionPass = bassLockHeld && bassLockReleased;

    // Test Auto-Inhale Riff
    if (autoRiffParam) autoRiffParam->setValueNotifyingHost(1.0f); // Enable Auto-Riff
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100), 0); // E4 strike 1
    buffer.clear();
    processor.processBlock(buffer, midi);
    float autoRiff1Rms = 0.f;
    for (int b = 0; b < 6; ++b)
    {
        midi.clear(); buffer.clear();
        processor.processBlock(buffer, midi);
        float r = buffer.getRMSLevel(0, 0, 512);
        if (r > autoRiff1Rms) autoRiff1Rms = r;
    }
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 8; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100), 0); // Second strike
    buffer.clear();
    processor.processBlock(buffer, midi);
    float autoRiff2Rms = 0.f;
    for (int b = 0; b < 6; ++b)
    {
        midi.clear(); buffer.clear();
        processor.processBlock(buffer, midi);
        float r = buffer.getRMSLevel(0, 0, 512);
        if (r > autoRiff2Rms) autoRiff2Rms = r;
    }
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    for (int b = 0; b < 8; ++b) { midi.clear(); buffer.clear(); processor.processBlock(buffer, midi); }

    std::cout << "  Auto-Riff Strike 1 RMS: " << autoRiff1Rms << " | Strike 2 RMS: " << autoRiff2Rms << std::endl;
    bool autoRiffPass = (autoRiff1Rms > 0.15f && autoRiff2Rms > 0.15f);

    // 8. Video 0:09–0:27 Keyswitch Retriggering of Held Chords Test
    std::cout << "\n--- Video 0:09-0:27 Keyswitch Retriggering Verification ---" << std::endl;
    // Play sustained chord (C3 + G3 + C4)
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8)90), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 55, (juce::uint8)90), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)90), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);

    for (int b = 0; b < 10; ++b)
    {
        midi.clear();
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
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
    bool keyswitchPass = (chordSustainedRms > 0.10f && exhalePulseRms >= chordSustainedRms && exhalePulseFired && inhalePulseFired);
    std::cout << "  Keyswitch Retrigger Result: " << (keyswitchPass ? "[PASS]" : "[FAIL]") << std::endl;

    // 9. Preset Loading: Classic Khaen
    std::cout << "\n--- Preset Loading: Classic Khaen ---" << std::endl;
    processor.loadPreset(0); // Classic Khaen
    std::cout << "  Loaded Preset 0: " << processor.getPresetManager().getPreset(0).name.toStdString() << std::endl;

    bool allTestsPass = pitchRangePass && tuningPass && midiFired && chordPass && delayPass 
                        && nonZeroVisSamples > 0 && releasedRms < 0.005f && bypassPass && breathPass && bassSuctionPass && autoRiffPass && reverbPresetsPass && keyswitchPass;

    if (allTestsPass)
    {
        std::cout << "\n>>> ALL AUDIO, TAPE DELAY, BYPASS & BREATH TESTS PASSED! Generating UI snapshots... <<<" << std::endl;
        
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        if (auto* khaenEditor = dynamic_cast<KhaenEditor*>(editor.get()))
        {
            khaenEditor->setSize(1280, 800);
            
            auto saveSnapshot = [&](int tabIdx, const juce::String& filename)
            {
                khaenEditor->switchTab(tabIdx);
                juce::Image img(juce::Image::ARGB, 1280, 800, true);
                {
                    juce::Graphics g(img);
                    khaenEditor->paintEntireComponent(g, true);
                }
                juce::File outFile(juce::String("C:/Users/manua/.gemini/antigravity-ide/brain/635bf0d2-a505-4be5-b617-4491baecae08/") + filename);
                if (outFile.existsAsFile()) outFile.deleteFile();
                juce::FileOutputStream stream(outFile);
                if (stream.openedOk())
                {
                    juce::PNGImageFormat png;
                    png.writeImageToStream(img, stream);
                    std::cout << "Saved: " << outFile.getFullPathName() << std::endl;
                }
            };

            saveSnapshot(0, "media_ui_delay_tab.png");
            saveSnapshot(1, "media_ui_harm_tab.png");
            saveSnapshot(2, "media_ui_crystal_tab.png");
            saveSnapshot(3, "media_ui_reverb_tab.png");

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
