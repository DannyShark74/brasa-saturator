cmake_minimum_required(VERSION 3.22)
project(Brasa VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BRASA_BUILD_TOOLS "Build the offline test / render tool" OFF)
option(BRASA_BUILD_PLUGIN "Build the plugin" ON)
set(BRASA_JUCE_DIR "" CACHE PATH "Local JUCE checkout. If empty, JUCE is downloaded.")

if(BRASA_JUCE_DIR)
    add_subdirectory(${BRASA_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(BRASA_FORMATS VST3 AU)
else()
    set(BRASA_FORMATS VST3)
endif()

set(BRASA_DSP_SOURCES
    source/Styles.cpp
    source/Engine.cpp
    source/ParamTable.cpp)

if(BRASA_BUILD_PLUGIN)
    juce_add_plugin(Brasa
        PRODUCT_NAME "Brasa"
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Danny Shark"
        BUNDLE_ID com.dannyshark.brasa
        PLUGIN_MANUFACTURER_CODE Dnsh
        PLUGIN_CODE Bras
        PLUGIN_NAME "Brasa"
        DESCRIPTION "Analog saturation: five circuits, from smooth to scorched"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Distortion
        AU_MAIN_TYPE kAudioUnitType_Effect
        FORMATS ${BRASA_FORMATS})

    target_sources(Brasa PRIVATE
        ${BRASA_DSP_SOURCES}
        source/Params.cpp
        source/PluginProcessor.cpp
        source/PluginEditor.cpp)
    target_include_directories(Brasa PRIVATE source)
    target_compile_definitions(Brasa PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(Brasa
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endif()

if(BRASA_BUILD_TOOLS)
    juce_add_console_app(BrasaRender PRODUCT_NAME "BrasaRender")
    target_sources(BrasaRender PRIVATE tools/render.cpp ${BRASA_DSP_SOURCES})
    target_include_directories(BrasaRender PRIVATE source)
    target_compile_definitions(BrasaRender PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(BrasaRender PRIVATE juce::juce_audio_formats juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()

if(BRASA_BUILD_TOOLS AND BRASA_BUILD_PLUGIN)
    # renders the editor to a PNG (layout check without a DAW)
    juce_add_console_app(BrasaShot PRODUCT_NAME "BrasaShot")
    target_sources(BrasaShot PRIVATE tools/shot.cpp ${BRASA_DSP_SOURCES}
        source/Params.cpp source/PluginProcessor.cpp source/PluginEditor.cpp)
    target_include_directories(BrasaShot PRIVATE source)
    target_compile_definitions(BrasaShot PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JucePlugin_Name="Brasa")
    target_link_libraries(BrasaShot PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
