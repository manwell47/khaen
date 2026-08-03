# Khaen VST3 Physical Model Synthesizer

> 🎋 *Free-reed digital waveguide synthesizer. JUCE 8 · MPE · VST3/AU*

---

## Quick Start

### 1. Prerequisites
| Tool | Min Version | Check |
|------|------------|-------|
| CMake | 3.22+ | `cmake --version` |
| Git | any | `git --version` |
| Visual Studio | 2022 (C++ workload) | vswhere |
| Python 3 + pip | 3.8+ | for LiteLLM |

### 2. Build (Windows)
```powershell
# First time (~20–40 min for JUCE download):
.\build.ps1

# Subsequent builds (fast):
.\build.ps1

# Debug build:
.\build.ps1 -Debug

# Clean rebuild:
.\build.ps1 -Clean
```

### 3. Install
```powershell
# Copy VST3 to system folder:
Copy-Item -Recurse "build\Khaen_artefacts\Release\VST3\Khaen.vst3" `
          "C:\Program Files\Common Files\VST3\"
```

### 4. LiteLLM Proxy (optional)
```powershell
# Launch proxy on port 4000:
.\start_proxy.ps1 -Background
```

---

## Architecture

```
MIDI In (MPE)
    │
    ▼
KhaenSynthesiser (16 voices)
    │  └── KhaenVoice × 16
    │       └── FreeReedModel
    │            ├── Reed excitation (Hirschberg)
    │            ├── Digital Waveguide (forward/backward)
    │            ├── Thiran allpass interpolation
    │            └── 1-pole loss filter
    │
    ▼
TapeDelay (wow/flutter/ping-pong)
    │
    ▼
Harmonizer (OLA pitch shift, ±12 semi)
    │
    ▼
GranularCrystallizer
    ├── Mode 0: Reverse Granular Echo
    ├── Mode 1: Pitch-Shifted Feedback
    └── Mode 2: Glitched Freeze
    │
    ▼
Audio Out (Stereo)
```

---

## Presets

| # | Name | Material | Character |
|---|------|----------|-----------|
| 1 | Khaen Clásico | Bambú | Traditional, warm |
| 2 | Breath of Laos | Bambú Añejado | Airy, spacious |
| 3 | Jade Reed | Jade | Bright, crystalline |
| 4 | Copper Tongue | Latón | Metallic, punchy |
| 5 | Ghost Pipe | PVC | Dark, haunting |
| 6 | Deep Resonance | Madera Oscura | Sub-bass, drone |
| 7 | Crystal Air | Bambú | Granular shimmer |
| 8 | Storm Reed | Metal | Aggressive, noisy |
| 9 | Silk Breath | Bambú Pulido | Smooth, legato |
|10 | Void Drone | Sintético | Deep, alien drone |

---

## MPE Expression Mapping

| MPE Axis | Parameter |
|----------|-----------|
| Pressure (Z) | Breath intensity |
| Slide (Y) | Lip damping / timbre |
| Pitch Bend (X) | Micro-tonal glissando ±48 semi |

---

## Project Structure

```
Khaen/
├── CMakeLists.txt          JUCE 8 build (FetchContent)
├── build.ps1               Windows build script
├── start_proxy.ps1         LiteLLM proxy launcher
├── litellm_config.yaml     Model routing config
└── Source/
    ├── PluginProcessor.h/cpp
    ├── PluginEditor.h/cpp
    ├── DSP/
    │   ├── FreeReedModel.h/cpp   Digital Waveguide engine
    │   ├── KhaenVoice.h/cpp      MPE voice
    │   └── KhaenSynthesiser.h/cpp
    ├── FX/
    │   ├── TapeDelay.h/cpp
    │   ├── Harmonizer.h/cpp
    │   └── GranularCrystallizer.h/cpp
    ├── Presets/
    │   └── PresetManager.h/cpp   10 built-in presets
    └── UI/
        ├── CustomKnob.h/cpp      Copper vector knobs
        └── WaveVisualizer.h/cpp  Real-time oscilloscope
```
