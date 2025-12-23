cmake_minimum_required(VERSION 3.22)
project(Counterpoints VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to JUCE folder (adjust if nested differently)
add_subdirectory(${CMAKE_SOURCE_DIR}/JUCE)

add_compile_definitions(JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS
    "Source/*.h"
    "Source/*.cpp"
)

juce_add_gui_app(Counterpoints
    PRODUCT_NAME "Counterpoints"
    COMPANY_NAME "Counterpoints"
    VERSION "0.1.0"
)

target_sources(Counterpoints PRIVATE ${SOURCE_FILES})

target_link_libraries(Counterpoints
    PRIVATE
      juce::juce_gui_basics
      juce::juce_gui_extra
      juce::juce_audio_utils
      juce::juce_audio_devices
      juce::juce_audio_basics
      juce::juce_audio_processors
      juce::juce_graphics
      juce::juce_core
      juce::juce_events
      juce::juce_data_structures
)

set_target_properties(Counterpoints PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.junnuo.Counterpoints"
    MACOSX_BUNDLE_BUNDLE_NAME "Counterpoints"
)
