cmake_minimum_required(VERSION 3.22)
project(ReverseReverb VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(RR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RR_FORMATS AU)
endif()

juce_add_plugin(ReverseReverb
    COMPANY_NAME "YourName"                 # <- change me
    PLUGIN_MANUFACTURER_CODE Ynme           # 4 chars, at least one capital
    PLUGIN_CODE Rvrs                        # 4 chars, exactly one capital
    FORMATS ${RR_FORMATS}
    PRODUCT_NAME "Reverse Reverb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE
    BUNDLE_ID com.yourname.reversereverb)

target_sources(ReverseReverb PRIVATE
    Source/ReverseReverbEngine.cpp
    Source/LiveSwell.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ReverseReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(ReverseReverb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional DSP test harness:  cmake -DRR_BUILD_TESTS=ON ..
option(RR_BUILD_TESTS "Build the console DSP tests" OFF)
if(RR_BUILD_TESTS)
    juce_add_console_app(EngineTest PRODUCT_NAME "EngineTest")
    target_sources(EngineTest PRIVATE
        Tests/EngineTest.cpp
        Source/ReverseReverbEngine.cpp
        Source/LiveSwell.cpp)
    target_include_directories(EngineTest PRIVATE Source)
    target_link_libraries(EngineTest PRIVATE juce::juce_dsp juce::juce_audio_basics
        juce::juce_recommended_config_flags)
endif()
