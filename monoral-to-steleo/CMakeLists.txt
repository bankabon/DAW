cmake_minimum_required(VERSION 3.22)

project(MonoToStereo VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # VS Code の clangd が使う

add_subdirectory(external/JUCE)

juce_add_plugin(MonoToStereo
    COMPANY_NAME            "Private"
    PLUGIN_MANUFACTURER_CODE Prvt
    PLUGIN_CODE             M2st
    PRODUCT_NAME            "MonoToStereo"
    FORMATS                 VST3 Standalone # Cubase は VST3。AU は後で追加する
    VST3_CATEGORIES         Fx EQ
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(MonoToStereo PRIVATE
    Source/EqCurve.cpp
    Source/EqImpulseDesigner.cpp
    Source/Parameters.cpp
    Source/PresetManager.cpp
    Source/SpectrumAnalyzer.cpp
    Source/EqCurveComponent.cpp
    Source/ModernLookAndFeel.cpp
    Source/StereoWidener.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(MonoToStereo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MonoToStereo
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
