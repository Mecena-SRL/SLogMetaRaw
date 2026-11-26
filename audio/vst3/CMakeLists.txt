# SPDX-License-Identifier: GPL-3.0-or-later
# Log MetaRaw Audio, VST3 for DaVinci Resolve Fairlight (macOS, Windows; Linux builds for the tests only) and Audio
# Units for Logic Pro (macOS, target LogMetaRawAU).
#   cmake -S audio/vst3 -B audio/vst3/build -DCMAKE_BUILD_TYPE=Release && cmake --build audio/vst3/build --config Release
# The Steinberg VST3 SDK (MIT) is fetched at a pinned tag unless VST3_SDK_DIR points at a local copy.
cmake_minimum_required(VERSION 3.25)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
file(READ "${REPO_ROOT}/slogmetaraw/__init__.py" _init)
string(REGEX MATCH "__version__ = '([0-9]+\\.[0-9]+\\.[0-9]+)'" _m "${_init}")
if(NOT CMAKE_MATCH_1)
  message(FATAL_ERROR "versione non trovata in slogmetaraw/__init__.py")
endif()
set(AU_VERSION "${CMAKE_MATCH_1}")
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()
project(LogMetaRawAudio VERSION ${AU_VERSION} LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC OBJCXX)   # VSTGUI's Cocoa code: enabled here, at the top, so every target sees it
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)   # populate only: the SDKs are added below, after their options
endif()
set(VST3_SDK_DIR "" CACHE PATH "Steinberg VST3 SDK root; fetched from GitHub (pinned tag) when empty")
if(NOT VST3_SDK_DIR)
  FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.8.1_build_84
    GIT_SHALLOW TRUE
    GIT_SUBMODULES base cmake pluginterfaces public.sdk vstgui4)
  FetchContent_GetProperties(vst3sdk)
  if(NOT vst3sdk_POPULATED)
    FetchContent_Populate(vst3sdk)
  endif()
  set(VST3_SDK_DIR "${vst3sdk_SOURCE_DIR}")
endif()

set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "")
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_AUV2_BUILDS OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)   # the SDK's validator runs on every build
set(vstgui_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(VSTGUI_STANDALONE OFF CACHE BOOL "" FORCE)
set(VSTGUI_TOOLS OFF CACHE BOOL "" FORCE)
set(vst3sdk_SOURCE_DIR "${VST3_SDK_DIR}")
add_subdirectory("${VST3_SDK_DIR}" "${PROJECT_BINARY_DIR}/vst3sdk" EXCLUDE_FROM_ALL)
smtg_enable_vst3_sdk()

set(AU_SOURCES
  AuPlugin.cpp
  AuParams.h
  AuLooks.h
  "${REPO_ROOT}/audio/src/AudioEngine.cpp"
  "${REPO_ROOT}/audio/src/AudioEngine.h"
  "${REPO_ROOT}/audio/src/AudioPresets.h")

# One VST3 module. only: 0 = both plugins (for Fairlight), 1 or 2 = one of them, to sit inside an Audio Unit.
function(lmr_add_vst3 target package only bundle_id)
  smtg_add_vst3plugin(${target} PACKAGE_NAME "${package}" SOURCES_LIST ${AU_SOURCES})
  target_include_directories(${target} PRIVATE "${REPO_ROOT}/audio/src")
  target_compile_definitions(${target} PRIVATE AU_VERSION="${AU_VERSION}" AU_ONLY=${only})
  target_link_libraries(${target} PRIVATE sdk vstgui_support)
  if(MSVC)
    target_compile_options(${target} PRIVATE /utf-8)
  endif()
  if(APPLE)
    smtg_target_set_bundle(${target} BUNDLE_IDENTIFIER ${bundle_id} COMPANY_NAME "Mecena SRL")
  endif()
endfunction()

lmr_add_vst3(LogMetaRawAudio "Log MetaRaw Audio" 0 com.slogmetaraw.audio)

# Audio Units (v2) for Logic Pro and the other Mac hosts: Steinberg's wrapper (public.sdk/source/vst/auwrapper) around
# a one-plugin VST3 copied into the component's Resources as plugin.vst3. Apple's AudioUnitSDK (Apache-2.0) is
# compiled in, so any CMake generator builds them (the SDK's own helper wants Xcode). Built by the target LogMetaRawAU.
if(APPLE)
  FetchContent_Declare(ausdk
    GIT_REPOSITORY https://github.com/apple/AudioUnitSDK.git
    GIT_TAG AudioUnitSDK-1.1.0   # C++17, like the wrapper; later versions need C++23
    GIT_SHALLOW TRUE)
  FetchContent_GetProperties(ausdk)
  if(NOT ausdk_POPULATED)
    FetchContent_Populate(ausdk)
  endif()
  file(GLOB _ausdk_sources "${ausdk_SOURCE_DIR}/src/AudioUnitSDK/*.cpp")
  add_library(ausdk STATIC EXCLUDE_FROM_ALL ${_ausdk_sources})
  target_include_directories(ausdk PUBLIC "${ausdk_SOURCE_DIR}/include")

  set(_wrapper "${VST3_SDK_DIR}/public.sdk/source/vst/auwrapper")
  add_custom_target(LogMetaRawAU)
  # name: bundle name; only: which plugin; subtype: its four-letter Audio Unit code (maker code Mcna)
  function(lmr_add_au target name only subtype)
    lmr_add_vst3(${target}Vst3 ${target}Vst3 ${only} com.slogmetaraw.audio.au${only}.vst3)   # no spaces: the SDK quotes poorly
    set(AU_NAME "${name}")
    set(AU_SUBTYPE "${subtype}")
    set(AU_BUNDLE_ID "com.slogmetaraw.audio.au${only}")
    math(EXPR AU_VERSION_INT "(${PROJECT_VERSION_MAJOR} << 16) | (${PROJECT_VERSION_MINOR} << 8) | ${PROJECT_VERSION_PATCH}")
    configure_file(au-info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist" @ONLY)
    add_library(${target} MODULE EXCLUDE_FROM_ALL
      "${_wrapper}/auwrapper.mm" "${_wrapper}/aucocoaview.mm" "${_wrapper}/NSDataIBStream.mm")
    target_compile_definitions(${target} PRIVATE SMTG_AUWRAPPER_USES_AUSDK CA_USE_AUDIO_PLUGIN_ONLY=0
                                                 SMTG_AUCocoaUIBase_CLASS_NAME=LogMetaRawAUView${only})
    target_link_libraries(${target} PRIVATE sdk_hosting ausdk "-framework AudioUnit" "-framework AudioToolbox"
                          "-framework CoreMIDI" "-framework CoreFoundation" "-framework CoreAudio"
                          "-framework Carbon" "-framework Cocoa")
    set_target_properties(${target} PROPERTIES
      BUNDLE TRUE
      BUNDLE_EXTENSION component
      OUTPUT_NAME "${name}"
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/AU")
    add_dependencies(${target} ${target}Vst3)
    add_custom_command(TARGET ${target} POST_BUILD   # a real copy, not the SDK's link: the component travels alone
      COMMAND rm -rf "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources/plugin.vst3"
      COMMAND mkdir -p "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources"
      COMMAND cp -R "$<TARGET_BUNDLE_DIR:${target}Vst3>" "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources/plugin.vst3"
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:${target}>")
    add_dependencies(LogMetaRawAU ${target})
  endfunction()
  lmr_add_au(LogMetaRawRipresaAU "Log MetaRaw Ripresa" 1 LMrp)
  lmr_add_au(LogMetaRawMixAU "Log MetaRaw Mix e supporto" 2 LMmx)
endif()
