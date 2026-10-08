cmake_minimum_required(VERSION 3.22)

project(VitaPlugGainer80 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE is deliberately vendored as source rather than obtained during CMake
# configuration. This keeps builds offline and reproducible once dependencies
# have been checked out with the exact commit documented in README.md.
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE 9.0.3 is missing. Run the documented clone command in README.md "
        "to populate external/JUCE at commit be29c81492b6151c8ea8d14c840e1311963b3a83.")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)

# Approved G80 material assets are compiled into the plug-in. Runtime code
# therefore never depends on Resources/ existing on the end user's Mac.
juce_add_binary_data(VitaPlugGainer80Assets
    SOURCES
        Resources/G80/v1/export/chassis-panel-typography-v2@2x.png
        Resources/G80/v1/export/input-vu-housing@2x.png
        Resources/G80/v1/export/output-vu-housing@2x.png
        Resources/G80/v1/export/gain-knob-metal@2x.png
        Resources/G80/v1/export/fatty-knob-metal@2x.png
        Resources/G80/v1/source/switch/switch-base.png
        Resources/G80/v1/source/switch/switch-lever-upper.png
        Resources/G80/v1/source/switch/switch-lever-lower.png
)

juce_add_plugin(VitaPlugGainer80
    COMPANY_NAME "VITAPLUG"
    COMPANY_WEBSITE "https://vitaplug.audio"
    COMPANY_EMAIL "hello@vitaplug.audio"
    BUNDLE_ID "com.vitaplug.gainer80"
    PLUGIN_MANUFACTURER_CODE Vtpl
    PLUGIN_CODE Gn80
    FORMATS AU VST3
    PRODUCT_NAME "VitaPlug Gainer80"
    PLUGIN_NAME "VitaPlug Gainer80"
    DESCRIPTION "Gain and 80 Hz bass enhancement"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(VitaPlugGainer80
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(VitaPlugGainer80 PRIVATE Source)

target_compile_definitions(VitaPlugGainer80
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        # This is the first release and has no VST2 predecessor to replace.
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VitaPlugGainer80
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        VitaPlugGainer80Assets
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
add_executable(VitaPlugGainer80DSPTests Tests/DSPTests.cpp)
target_include_directories(VitaPlugGainer80DSPTests PRIVATE Source)
target_compile_features(VitaPlugGainer80DSPTests PRIVATE cxx_std_17)
target_link_libraries(VitaPlugGainer80DSPTests PRIVATE juce::juce_data_structures)
add_test(NAME VitaPlugGainer80DSPTests COMMAND VitaPlugGainer80DSPTests)
