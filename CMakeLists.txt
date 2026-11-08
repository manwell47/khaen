cmake_minimum_required(VERSION 3.22)
project(Khaen VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/FS)
endif()

# ─── Fetch JUCE 8 ──────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ─── Plugin Definition ─────────────────────────────────────────────────────────
juce_add_plugin(Khaen
    COMPANY_NAME                "KhaenAudio"
    COMPANY_WEBSITE             "https://khaenaudio.com"
    COMPANY_EMAIL               "dev@khaenaudio.com"
    BUNDLE_ID                   "com.khaenaudio.khaen"
    PLUGIN_MANUFACTURER_CODE    "KhAu"
    PLUGIN_CODE                 "Khen"
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Khaen"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Instrument" "Synth"
    AU_MAIN_TYPE                "kAudioUnitType_MusicDevice"
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CAN_REPLACE_VST2       FALSE
    DESCRIPTION                 "Khaen — Free Reed Physical Model Synthesizer"
    HARDENED_RUNTIME_ENABLED    TRUE
)

# Generate JuceHeader.h for legacy includes
juce_generate_juce_header(Khaen)

# ─── Source Files ──────────────────────────────────────────────────────────────
target_sources(Khaen PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    
    # UI
    Source/UI/CustomKnob.cpp
    Source/UI/CustomKnob.h
    Source/UI/WaveVisualizer.cpp
    Source/UI/WaveVisualizer.h
    Source/UI/MidiLed.h
    Source/UI/MidiMappingDialog.cpp
    Source/UI/MidiMappingDialog.h
    Source/UI/HelpDialog.cpp
    Source/UI/HelpDialog.h

    # MIDI & Hardware Mapping
    Source/MIDI/MidiCCManager.cpp
    Source/MIDI/MidiCCManager.h

    # DSP Engine
    Source/DSP/FreeReedModel.cpp
    Source/DSP/FreeReedModel.h
    Source/DSP/KhaenVoice.cpp
    Source/DSP/KhaenVoice.h
    Source/DSP/KhaenSynthesiser.cpp
    Source/DSP/KhaenSynthesiser.h

    # FX Chain
    Source/FX/TapeDelay.cpp
    Source/FX/TapeDelay.h
    Source/FX/Harmonizer.cpp
    Source/FX/Harmonizer.h
    Source/FX/GranularCrystallizer.cpp
    Source/FX/GranularCrystallizer.h
    Source/FX/ReverbEngine.cpp
    Source/FX/ReverbEngine.h

    # Presets
    Source/Presets/PresetManager.cpp
    Source/Presets/PresetManager.h
)

# ─── Compile Definitions ───────────────────────────────────────────────────────
target_compile_definitions(Khaen
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

# ─── Compiler Optimizations ────────────────────────────────────────────────────
#
# IMPORTANT: distributed binaries must target the x86-64 baseline (SSE2).
# Emitting AVX / AVX2 (or -march=native) makes the plugin trap with
# STATUS_ILLEGAL_INSTRUCTION (0xC000001D) on CPUs that lack those extensions
# (Intel pre-Haswell, AMD pre-Excavator, many Atom / Celeron / Pentium parts).
# The host then dies the first time the DSP code runs, i.e. on the first note,
# while the editor still opens fine.
option(KHAEN_NATIVE_OPT
    "Generate host-CPU-specific code (AVX2 / -march=native). Local builds only, NEVER for release." OFF)

if(MSVC)
    target_compile_options(Khaen PRIVATE /O2 /Oi /Ot /fp:fast)
    if(KHAEN_NATIVE_OPT)
        target_compile_options(Khaen PRIVATE /arch:AVX2)
        message(STATUS "   WARNING: KHAEN_NATIVE_OPT=ON -> /arch:AVX2 (runs only on AVX2 CPUs)")
    endif()
else()
    target_compile_options(Khaen PRIVATE -O3 -ffast-math -funroll-loops)
    if(KHAEN_NATIVE_OPT)
        target_compile_options(Khaen PRIVATE -march=native)
        message(STATUS "   WARNING: KHAEN_NATIVE_OPT=ON -> -march=native (runs only on this CPU)")
    endif()
endif()

# ─── Include Directories ───────────────────────────────────────────────────────
target_include_directories(Khaen PRIVATE
    Source
    Source/DSP
    Source/FX
    Source/MIDI
    Source/Presets
    Source/UI
)

# ─── Binary Assets ─────────────────────────────────────────────────────────────
juce_add_binary_data(KhaenBinaryData
    SOURCES
        "Breath of Laos.png"
)

# ─── Link JUCE Modules ─────────────────────────────────────────────────────────
target_link_libraries(Khaen
    PRIVATE
        KhaenBinaryData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ─── macOS specific ────────────────────────────────────────────────────────────
if(APPLE)
    set_target_properties(Khaen PROPERTIES
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "12.0"
    )
endif()

# ─── DSP Test Executable ───────────────────────────────────────────────────────
juce_add_console_app(KhaenDspTest
    PRODUCT_NAME "KhaenDspTest"
)
juce_generate_juce_header(KhaenDspTest)
target_sources(KhaenDspTest PRIVATE
    Tests/DspTest.cpp
)
if(MSVC)
    target_compile_options(KhaenDspTest PRIVATE /FS)
endif()
target_include_directories(KhaenDspTest PRIVATE
    Source
    Source/DSP
    Source/FX
    Source/MIDI
    Source/Presets
    Source/UI
)
target_link_libraries(KhaenDspTest PRIVATE
    Khaen
    KhaenBinaryData
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# ─── Build summary ────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "🎋 Khaen VST3 ${PROJECT_VERSION} — Build Configuration")
message(STATUS "   Build Type : ${CMAKE_BUILD_TYPE}")
message(STATUS "   C++ Std    : ${CMAKE_CXX_STANDARD}")
message(STATUS "   Formats    : VST3, AU (macOS), Standalone")
message(STATUS "")
