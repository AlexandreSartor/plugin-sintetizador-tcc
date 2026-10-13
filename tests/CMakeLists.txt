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

# Por enquanto so o modulo juce_core (sem GUI, sem DAW).
# Nos proximos passos, o "core" do plugin entra aqui.
target_link_libraries(tapsynth_tests
    PRIVATE
        juce::juce_core
        GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(tapsynth_tests)
