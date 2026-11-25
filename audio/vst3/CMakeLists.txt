# SPDX-License-Identifier: GPL-3.0-or-later
# Log MetaRaw Audio, VST3 for DaVinci Resolve Fairlight (macOS, Windows; Linux builds for the tests only).
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

set(VST3_SDK_DIR "" CACHE PATH "Steinberg VST3 SDK root; fetched from GitHub (pinned tag) when empty")
if(NOT VST3_SDK_DIR)
  include(FetchContent)
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)   # populate only: the SDK is added below, after its options
  endif()
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

smtg_add_vst3plugin(LogMetaRawAudio
  PACKAGE_NAME "Log MetaRaw Audio"
  SOURCES_LIST
    AuPlugin.cpp
    AuParams.h
    "${REPO_ROOT}/audio/src/AudioEngine.cpp"
    "${REPO_ROOT}/audio/src/AudioEngine.h"
    "${REPO_ROOT}/audio/src/AudioPresets.h")
target_include_directories(LogMetaRawAudio PRIVATE "${REPO_ROOT}/audio/src")
target_compile_definitions(LogMetaRawAudio PRIVATE AU_VERSION="${AU_VERSION}")
target_link_libraries(LogMetaRawAudio PRIVATE sdk vstgui_support)
if(MSVC)
  target_compile_options(LogMetaRawAudio PRIVATE /utf-8)
endif()
if(APPLE)
  smtg_target_set_bundle(LogMetaRawAudio
    BUNDLE_IDENTIFIER com.slogmetaraw.audio
    COMPANY_NAME "Mecena SRL")
endif()
