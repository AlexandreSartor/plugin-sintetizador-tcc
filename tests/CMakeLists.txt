include(FetchContent)

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# Necessario no Windows/MSVC: usa o mesmo runtime (DLL) do resto do projeto
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(tapsynth_tests
    SmokeTests.cpp)

# tapsynth_core so traz cabecalhos dos modulos JUCE; o codigo deles
# (juce_core, juce_audio_basics, juce_dsp) e compilado aqui. Sem GUI, sem DAW.
target_link_libraries(tapsynth_tests
    PRIVATE
        tapsynth_core
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_recommended_config_flags
        GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(tapsynth_tests)
