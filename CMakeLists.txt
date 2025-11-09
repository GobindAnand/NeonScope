cmake_minimum_required(VERSION 3.15)

project(NeonScope VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE (expects JUCE in ./JUCE)
add_subdirectory(JUCE)

juce_add_plugin(NeonScope
    COMPANY_NAME "GobindAnand"
    BUNDLE_ID com.gobindanand.neonscope
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Gobd
    PLUGIN_CODE Nscp
    FORMATS VST3
    PRODUCT_NAME "NeonScope"
)

# Generate JuceHeader.h for this target
juce_generate_juce_header(NeonScope)

target_sources(NeonScope
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_link_libraries(NeonScope
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core
)

# Optional, but nice:
target_compile_definitions(NeonScope
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

