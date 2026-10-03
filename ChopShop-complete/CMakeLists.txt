cmake_minimum_required(VERSION 3.22)
project(ChopShop VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Pass -DAAX_SDK_PATH=/path/to/aax-sdk to also build the AAX (Pro Tools) version.
set(AAX_SDK_PATH "" CACHE PATH "Path to the Avid AAX SDK")
set(PLUGIN_FORMATS VST3)
if(AAX_SDK_PATH)
  juce_set_aax_sdk_path(${AAX_SDK_PATH})
  list(APPEND PLUGIN_FORMATS AAX)
endif()

juce_add_plugin(ChopShop
  COMPANY_NAME "ChopShop Audio"
  PLUGIN_MANUFACTURER_CODE Chpa
  PLUGIN_CODE Chsh
  FORMATS ${PLUGIN_FORMATS}
  PRODUCT_NAME "ChopShop"
  IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx
  AAX_CATEGORY AAX_ePlugInCategory_Effect
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(ChopShop PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(ChopShop PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ChopShop PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
