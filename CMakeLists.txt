cmake_minimum_required(VERSION 3.22)
project(FreeBass VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")

# JUCE: use ./JUCE if present, otherwise fetch it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FB_FORMATS VST3)
if(APPLE)
    list(APPEND FB_FORMATS AU)
endif()

juce_add_plugin(FreeBass
    COMPANY_NAME "Chris"
    BUNDLE_ID "com.chris.freebass"
    PLUGIN_MANUFACTURER_CODE Chrs
    PLUGIN_CODE FBas
    FORMATS ${FB_FORMATS}
    PRODUCT_NAME "FreeBass"
    DESCRIPTION "Psychoacoustic bass enhancer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FreeBass PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(FreeBass PRIVATE Ecosystem)

target_compile_definitions(FreeBass PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FreeBass
    PRIVATE juce::juce_audio_utils juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline engine test (no JUCE): run_engine in.wav out.wav freq int gain orig
add_executable(run_engine tests/run_engine.cpp)
