cmake_minimum_required(VERSION 3.22)
project(HolterdipolterComp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokal installierte Kopie verwenden, falls vorhanden (Standard C:/JUCE-9.0.1,
# mit -DJUCE_DIR=... ueberschreibbar). Sonst - z. B. auf GitHub - wird genau diese
# Version automatisch von GitHub geholt.
set(JUCE_VERSION "9.0.1")
set(JUCE_DIR "C:/JUCE-${JUCE_VERSION}" CACHE PATH "Pfad zum JUCE-Ordner")

if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "JUCE: lokale Kopie ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    message(STATUS "JUCE: nicht unter ${JUCE_DIR} gefunden - lade Version ${JUCE_VERSION} von GitHub")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${JUCE_VERSION}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(HolterdipolterComp
    COMPANY_NAME "Ben"
    PLUGIN_MANUFACTURER_CODE Benb
    PLUGIN_CODE Hdpc
    FORMATS VST3
    PRODUCT_NAME "Holterdipolter Comp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(HolterdipolterComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(HolterdipolterComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(HolterdipolterComp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
