cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(Iso VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 9 - the shared ~/JUCE checkout every plug-in in this workspace uses; CI
# checks it out into ./JUCE instead.
if(DEFINED ENV{HOME} AND EXISTS "$ENV{HOME}/JUCE/CMakeLists.txt")
    set(ISO_JUCE_DIR "$ENV{HOME}/JUCE")
elseif(DEFINED ENV{USERPROFILE} AND EXISTS "$ENV{USERPROFILE}/JUCE/CMakeLists.txt")
    set(ISO_JUCE_DIR "$ENV{USERPROFILE}/JUCE")
else()
    set(ISO_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()
add_subdirectory(${ISO_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

# --- build identity ------------------------------------------------------------
set(ISO_VERSION "${PROJECT_VERSION}")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE ISO_GIT_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT ISO_GIT_DESCRIBE)
    set(ISO_GIT_DESCRIBE "no-git")
endif()
string(TIMESTAMP ISO_BUILD_DATE "%Y-%m-%d" UTC)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/IsoVersion.h.in"
               "${CMAKE_BINARY_DIR}/generated/IsoVersion.h" @ONLY)
message(STATUS "ISO ${ISO_VERSION} (${ISO_GIT_DESCRIBE}, ${ISO_BUILD_DATE})")

if(APPLE)
    set(ISO_FORMATS VST3 AU Standalone)
else()
    set(ISO_FORMATS VST3 Standalone)
endif()

option(ISO_COPY_AFTER_BUILD "Copy built plug-ins into the user plug-in folders" ON)
option(ISO_WERROR "Treat warnings in ISO's own sources as errors" OFF)
option(ISO_SANITIZERS "Add IsoTestsSan / IsoHostTestsSan targets built with ASan/UBSan" OFF)

if(MSVC)
    # /utf-8: without it MSVC reads the source in the machine's ANSI codepage
    # and the first non-ASCII character in a comment is a Windows-only error.
    set(ISO_WARNING_FLAGS /W4 /permissive- /utf-8)
    if(ISO_WERROR)
        list(APPEND ISO_WARNING_FLAGS /WX)
    endif()
else()
    set(ISO_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wcast-align -Wunused
        -Woverloaded-virtual -Wnon-virtual-dtor -Wimplicit-fallthrough -Wfloat-conversion)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND ISO_WARNING_FLAGS -Wshorten-64-to-32)
    endif()
    if(ISO_WERROR)
        list(APPEND ISO_WARNING_FLAGS -Werror)
    endif()
endif()

set(ISO_DSP_SOURCES
    Source/Dsp/IsoEngine.cpp)

set(ISO_PLUGIN_SOURCES
    ${ISO_DSP_SOURCES}
    Source/Core/Parameters.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/Ui/CurveView.cpp
    Source/Ui/Theme.cpp)

set(ISO_OWN_SOURCES ${ISO_PLUGIN_SOURCES} Source/Tools/test_dsp.cpp Source/Tools/test_host.cpp Source/Tools/screenshot_tool.cpp)
set_source_files_properties(${ISO_OWN_SOURCES} PROPERTIES COMPILE_OPTIONS "${ISO_WARNING_FLAGS}")

set(ISO_COMMON_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# The bundle icon is rendered BY the plug-in's own code (IsoShot --icon) and
# committed; on a fresh tree it may not exist yet, and a missing ICON_BIG is a
# configure error rather than a missing icon.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png")
    set(ISO_ICON_ARG ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png")
endif()

# --- the plug-in ---------------------------------------------------------------
juce_add_plugin(Iso
    COMPANY_NAME "Naaman"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${ISO_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Naam
    PLUGIN_CODE Iso1
    FORMATS ${ISO_FORMATS}
    VST3_CATEGORIES "Fx" "EQ" "Filter"
    AU_MAIN_TYPE kAudioUnitType_Effect
    VERSION "${ISO_VERSION}"
    ${ISO_ICON_ARG}
    PRODUCT_NAME "ISO")

target_sources(Iso PRIVATE ${ISO_PLUGIN_SOURCES})
target_include_directories(Iso PUBLIC "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(Iso PUBLIC ${ISO_COMMON_DEFS})
target_link_libraries(Iso
    PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

# --- DSP measurement suite -----------------------------------------------------
#     build/IsoTests_artefacts/<config>/IsoTests
juce_add_console_app(IsoTests PRODUCT_NAME "IsoTests")
juce_generate_juce_header(IsoTests)
target_sources(IsoTests PRIVATE Source/Tools/test_dsp.cpp ${ISO_DSP_SOURCES})
target_include_directories(IsoTests PUBLIC "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(IsoTests PUBLIC ${ISO_COMMON_DEFS} JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(IsoTests
    PRIVATE juce::juce_audio_basics juce::juce_core
    PUBLIC  juce::juce_recommended_config_flags)

# --- deterministic UI renderer -------------------------------------------------
#     build/IsoShot_artefacts/<config>/IsoShot ui-shots
juce_add_console_app(IsoShot PRODUCT_NAME "IsoShot")
juce_generate_juce_header(IsoShot)
target_sources(IsoShot PRIVATE Source/Tools/screenshot_tool.cpp ${ISO_PLUGIN_SOURCES})
target_include_directories(IsoShot PUBLIC "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(IsoShot PUBLIC ${ISO_COMMON_DEFS} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(IsoShot
    PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
    PUBLIC  juce::juce_recommended_config_flags)

# --- host-contract suite -------------------------------------------------------
#     build/IsoHostTests_artefacts/<config>/IsoHostTests
# Drives the whole AudioProcessor the way a host does, editor included.
juce_add_console_app(IsoHostTests PRODUCT_NAME "IsoHostTests")
juce_generate_juce_header(IsoHostTests)
target_sources(IsoHostTests PRIVATE Source/Tools/test_host.cpp ${ISO_PLUGIN_SOURCES})
target_include_directories(IsoHostTests PUBLIC "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(IsoHostTests PUBLIC ${ISO_COMMON_DEFS} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(IsoHostTests
    PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
    PUBLIC  juce::juce_recommended_config_flags)

# --- sanitiser builds of both suites (opt-in) ----------------------------------
#     cmake -B build-san -DISO_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
if(ISO_SANITIZERS)
    juce_add_console_app(IsoTestsSan PRODUCT_NAME "IsoTestsSan")
    juce_generate_juce_header(IsoTestsSan)
    target_sources(IsoTestsSan PRIVATE Source/Tools/test_dsp.cpp ${ISO_DSP_SOURCES})
    target_include_directories(IsoTestsSan PUBLIC "${CMAKE_BINARY_DIR}/generated")
    target_compile_definitions(IsoTestsSan PUBLIC ${ISO_COMMON_DEFS} JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(IsoTestsSan PRIVATE juce::juce_audio_basics juce::juce_core PUBLIC juce::juce_recommended_config_flags)

    juce_add_console_app(IsoHostTestsSan PRODUCT_NAME "IsoHostTestsSan")
    juce_generate_juce_header(IsoHostTestsSan)
    target_sources(IsoHostTestsSan PRIVATE Source/Tools/test_host.cpp ${ISO_PLUGIN_SOURCES})
    target_include_directories(IsoHostTestsSan PUBLIC "${CMAKE_BINARY_DIR}/generated")
    target_compile_definitions(IsoHostTestsSan PUBLIC ${ISO_COMMON_DEFS} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(IsoHostTestsSan PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra PUBLIC juce::juce_recommended_config_flags)

    foreach(t IsoTestsSan IsoHostTestsSan)
        if(MSVC)
            target_compile_options(${t} PRIVATE /fsanitize=address)
        else()
            target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
            target_link_options(${t} PRIVATE -fsanitize=address,undefined)
        endif()
    endforeach()
endif()
