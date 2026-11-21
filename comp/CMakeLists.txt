cmake_minimum_required(VERSION 3.22)

project(TwoStageComp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCE は monoral-to-steleo と共用する
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../monoral-to-steleo/external/JUCE
                 ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

juce_add_plugin(TwoStageComp
    COMPANY_NAME            "Private"
    PLUGIN_MANUFACTURER_CODE Prvt
    PLUGIN_CODE             Tscp
    PRODUCT_NAME            "TwoStageComp"
    FORMATS                 VST3 Standalone
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TwoStageComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(TwoStageComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TwoStageComp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# 静的カーブ検証用の小さなテスト実行ファイル(JUCE 非依存)
add_executable(CurveCheck Tests/CurveCheck.cpp)
target_include_directories(CurveCheck PRIVATE Source)
