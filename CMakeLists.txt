cmake_minimum_required(VERSION 3.22)

project(J60Chorus VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# Installing into the system plugin folder is handy locally but unwanted on CI.
option(J60_COPY_AFTER_BUILD "Copy plugin to the system plugin folder after build" ON)

set(J60_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND J60_FORMATS AU)
endif()

juce_add_plugin(J60Chorus
    COMPANY_NAME "YourName"
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE J60c
    FORMATS ${J60_FORMATS}
    PRODUCT_NAME "J60 Chorus"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD ${J60_COPY_AFTER_BUILD})

# The editor artwork (made by tools/make_sprites.py) is compiled into the plugin.
juce_add_binary_data(J60Assets SOURCES
    assets/bg.png
    assets/slider_stereo.png
    assets/led1_on.png
    assets/led2_on.png
    assets/btn_1.png
    assets/btn_2.png
    assets/btn_12.png)
set_target_properties(J60Assets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(J60Chorus PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    EditorLayout.h
    JuneChorus.h)

target_compile_definitions(J60Chorus PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(J60Chorus
    PRIVATE
        J60Assets
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
