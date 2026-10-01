# S-Log MetaRaw OpenFX plugins for DaVinci Resolve: Linux and Windows (CPU render) build.
# macOS keeps using the Makefile (universal binary, Metal); this file builds the same sources elsewhere.
#
#   cmake -S ofx/SLogMetaRaw -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#   cmake --install build --prefix <dir>        # <dir>/SLogMetaRaw.ofx.bundle
#
# The OpenFX SDK (Support library + headers) is fetched at the pinned tag unless OFX_SDK_DIR points to a
# local copy (e.g. the one in Resolve's Developer folder).
cmake_minimum_required(VERSION 3.20)
project(SLogMetaRaw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(APPLE)
  message(FATAL_ERROR "On macOS use the Makefile (universal binary with Metal): make -C ofx/SLogMetaRaw")
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ---- version: single source of truth is slogmetaraw/__init__.py
file(READ "${REPO_ROOT}/slogmetaraw/__init__.py" _init)
string(REGEX MATCH "__version__ = '([0-9]+\\.[0-9]+\\.[0-9]+)'" _m "${_init}")
if(NOT CMAKE_MATCH_1)
  message(FATAL_ERROR "versione non trovata in slogmetaraw/__init__.py")
endif()
set(SLOGMETARAW_VERSION "${CMAKE_MATCH_1}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${REPO_ROOT}/slogmetaraw/__init__.py")

# ---- OpenFX SDK
set(OFX_SDK_DIR "" CACHE PATH "OpenFX SDK root (contains include/ and Support/); fetched from GitHub when empty")
if(NOT OFX_SDK_DIR)
  include(FetchContent)
  FetchContent_Declare(openfx
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
    GIT_TAG OFX_Release_1_4_TAG
    GIT_SHALLOW ON)
  FetchContent_GetProperties(openfx)
  if(NOT openfx_POPULATED)
    FetchContent_Populate(openfx)
  endif()
  set(OFX_SDK_DIR "${openfx_SOURCE_DIR}")
endif()
if(NOT EXISTS "${OFX_SDK_DIR}/Support/Library/ofxsCore.cpp")
  message(FATAL_ERROR "OFX_SDK_DIR=${OFX_SDK_DIR}: Support/Library/ofxsCore.cpp not found")
endif()
set(OFX_SUPPORT_SRCS)
foreach(n ofxsCore ofxsImageEffect ofxsInteract ofxsLog ofxsMultiThread ofxsParams ofxsProperty ofxsPropertyValidation)
  list(APPEND OFX_SUPPORT_SRCS "${OFX_SDK_DIR}/Support/Library/${n}.cpp")
endforeach()

# ---- generated headers (gen/): same generator as the Makefile
file(GLOB MATH_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/math/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/metal/kernels/*.metal")
set(GEN_OUT
  "${CMAKE_CURRENT_SOURCE_DIR}/gen/DevelopMath.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/gen/MetalSource.inc"
  "${CMAKE_CURRENT_SOURCE_DIR}/gen/GamutMatrices.h")
add_custom_command(
  OUTPUT ${GEN_OUT}
  COMMAND ${Python3_EXECUTABLE} build_math.py
  WORKING_DIRECTORY "${REPO_ROOT}/tools"
  DEPENDS ${MATH_INPUTS} "${REPO_ROOT}/tools/build_math.py" "${REPO_ROOT}/tools/gamut_matrices.py"
  COMMENT "Generating gen/*.h")
add_custom_target(slogmetaraw_gen DEPENDS ${GEN_OUT})

# ---- plugin sources (no Metal: the CPU path renders everywhere)
file(GLOB PLUGIN_SRCS CONFIGURE_DEPENDS
  src/*.cpp src/common/*.cpp src/develop/*.cpp src/detail/*.cpp)
if(NOT WIN32)
  list(FILTER PLUGIN_SRCS EXCLUDE REGEX "ChildWin\\.cpp$")
endif()

add_library(SLogMetaRaw MODULE ${PLUGIN_SRCS} ${OFX_SUPPORT_SRCS})
add_dependencies(SLogMetaRaw slogmetaraw_gen)
target_include_directories(SLogMetaRaw PRIVATE
  "${OFX_SDK_DIR}/include" "${OFX_SDK_DIR}/Support/include" "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT EXISTS "${OFX_SDK_DIR}/include/ofxImageEffectExt.h")
  target_include_directories(SLogMetaRaw PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compat")
endif()
set_source_files_properties(src/common/Update.cpp PROPERTIES
  COMPILE_DEFINITIONS "SLOGMETARAW_RELEASE=\"${SLOGMETARAW_VERSION}\"")
set_target_properties(SLogMetaRaw PROPERTIES PREFIX "" SUFFIX ".ofx")

if(MSVC)
  # /fp:precise never contracts a*b+c; /utf-8 because the sources hold UTF-8 text
  target_compile_options(SLogMetaRaw PRIVATE /utf-8 /fp:precise /EHsc /W3)
  target_compile_definitions(SLogMetaRaw PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
  # -ffp-contract=off: bit-exact CPU results across compilers (see the macOS Makefile)
  target_compile_options(SLogMetaRaw PRIVATE -O2 -ffp-contract=off)
  if(WIN32)
    target_compile_definitions(SLogMetaRaw PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_options(SLogMetaRaw PRIVATE -static-libgcc -static-libstdc++ -static)
  else()
    target_link_options(SLogMetaRaw PRIVATE -Wl,--no-undefined -Wl,--exclude-libs,ALL)
  endif()
endif()
if(WIN32)
  target_link_libraries(SLogMetaRaw PRIVATE comdlg32 shell32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(SLogMetaRaw PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

# ---- bundle: <name>.ofx.bundle/Contents/{Linux-x86-64|Win64}/SLogMetaRaw.ofx + Resources
if(WIN32)
  set(OFX_ARCH_DIR "Win64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(OFX_ARCH_DIR "Linux-arm-64")
else()
  set(OFX_ARCH_DIR "Linux-x86-64")
endif()
set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/SLogMetaRaw.ofx.bundle")
set(RESOURCES com.slogmetaraw.SLogMetaRaw.png com.slogmetaraw.SLogMetaRawDetail.png fc_exposure.png fc_temperature.png
              fc_tint.png fc_zones.png view_gain.png view_base.png)
set(_res_copy)
foreach(r ${RESOURCES})
  list(APPEND _res_copy COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${r}"
       "${BUNDLE_DIR}/Contents/Resources/${r}")
endforeach()
add_custom_command(TARGET SLogMetaRaw POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}/Contents/${OFX_ARCH_DIR}" "${BUNDLE_DIR}/Contents/Resources"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:SLogMetaRaw>" "${BUNDLE_DIR}/Contents/${OFX_ARCH_DIR}/SLogMetaRaw.ofx"
  ${_res_copy}
  COMMENT "Assembling SLogMetaRaw.ofx.bundle")

install(DIRECTORY "${BUNDLE_DIR}" DESTINATION . USE_SOURCE_PERMISSIONS)

# ---- test helpers, driven by tests/*.py (python3 -m unittest discover -s tests); not run by ctest
option(SLOGMETARAW_BUILD_TESTS "Build the C++ test binaries" ON)
if(SLOGMETARAW_BUILD_TESTS)
  set(_common ${CMAKE_CURRENT_SOURCE_DIR}/src/common)
  function(slog_test name)
    add_executable(${name} ${ARGN})
    add_dependencies(${name} slogmetaraw_gen)
    target_include_directories(${name} PRIVATE "${OFX_SDK_DIR}/include" "${OFX_SDK_DIR}/Support/include"
                                               "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${OFX_SDK_DIR}/include/ofxImageEffectExt.h")
      target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compat")
    endif()
    if(NOT MSVC)
      target_compile_options(${name} PRIVATE -O2 -ffp-contract=off)
    else()
      target_compile_options(${name} PRIVATE /utf-8 /EHsc)
    endif()
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
                          RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tests"
                          RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/tests")
  endfunction()
  if(NOT WIN32)   # the OFX host simulator loads the bundle with dlopen
    slog_test(host_test "${REPO_ROOT}/tests/host_test.cpp")
    target_link_libraries(host_test PRIVATE ${CMAKE_DL_LIBS})
  endif()
  slog_test(ofx_math_test "${REPO_ROOT}/tests/ofx_math_test.cpp")
  slog_test(detail_test "${REPO_ROOT}/tests/detail_test.cpp" src/detail/DetailPasses.cpp)
  if(WIN32)
    set(_child ${_common}/ChildWin.cpp)
  else()
    set(_child ${_common}/Child.cpp)
  endif()
  slog_test(common_test "${REPO_ROOT}/tests/common_test.cpp" ${_common}/Files.cpp ${_common}/FlatJson.cpp
            ${_common}/Version.cpp ${_child})
  if(WIN32)
    target_link_libraries(common_test PRIVATE shell32)
  endif()
endif()
