cmake_minimum_required(VERSION 3.22)

project(tapSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TAPSYNTH_BUILD_TESTS "Compila a suite de testes" ON)

# Deixe vazio para baixar o JUCE automaticamente (bom para o CI).
# Para usar o JUCE que ja esta na sua maquina:
#   cmake -B build -DTAPSYNTH_JUCE_DIR="C:/caminho/para/JUCE"
# (a pasta que contem "modules" e o CMakeLists.txt do JUCE)
set(TAPSYNTH_JUCE_DIR "" CACHE PATH "Pasta raiz do JUCE (opcional)")

include(FetchContent)

if(TAPSYNTH_JUCE_DIR)
    add_subdirectory("${TAPSYNTH_JUCE_DIR}" "${CMAKE_BINARY_DIR}/_juce")
else()
    # Use a MESMA versao do JUCE que voce usa hoje no Projucer (Projucer > Help > About)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.10
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin (equivalente ao tapSynth.jucer: synth com entrada MIDI, VST3 + Standalone)
# ---------------------------------------------------------------------------
juce_add_plugin(tapSynth
    PRODUCT_NAME                "tapSynth"
    COMPANY_NAME                "tapSynth"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Tsyn
    PLUGIN_CODE                 Tap1
    FORMATS                     VST3 Standalone)

target_sources(tapSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SynthVoice.cpp
    Source/Data/AdsrData.cpp
    Source/Data/FilterData.cpp
    Source/Data/MeterData.cpp
    Source/Data/OscData.cpp
    Source/UI/AdsrComponent.cpp
    Source/UI/Assets.cpp
    Source/UI/CustomComponent.cpp
    Source/UI/FilterComponent.cpp
    Source/UI/LfoComponent.cpp
    Source/UI/OscComponent.cpp
    Source/UI/ReverbComponent.cpp
    Source/UI/MeterComponent.cpp)

target_include_directories(tapSynth PRIVATE Source)

# O codigo atual usa #include <JuceHeader.h> (herdado do Projucer)
juce_generate_juce_header(tapSynth)

target_compile_definitions(tapSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(tapSynth
    PRIVATE
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
    PUBLIC
        juce::juce_recommended_config_flags)

# ---------------------------------------------------------------------------
# Testes
# ---------------------------------------------------------------------------
if(TAPSYNTH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
