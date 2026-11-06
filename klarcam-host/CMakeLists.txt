# KlarCam filter host for OBS Studio: the OBS module klarcam.dll. It hands the
# frames to the KlarCam engine (engine/klarcam-engine.exe beside it) and knows
# nothing of the image processing.
#
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Lars von Thienen
#
# Builds on its own against libobs (or as part of the KlarCam build):
#   tools/make-libobs-sdk.ps1    (once: libobs headers and obs.lib for the installed OBS)
#   cmake -S . -B build
#   cmake --build build --config RelWithDebInfo
# The engine protocol (protocol/, MIT) is all it shares with the engine.
cmake_minimum_required(VERSION 3.28)
if(NOT DEFINED PROJECT_NAME)
  project(klarcam-host VERSION 0.4.0 LANGUAGES CXX RC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  set(KLARCAM_HOST_STANDALONE ON)
endif()

# libobs: headers + import library, as tools/make-libobs-sdk.ps1 makes them.
set(LIBOBS_SDK_DIR "${CMAKE_SOURCE_DIR}/.deps/libobs-sdk" CACHE PATH "libobs SDK: include/obs.h and lib/obs.lib")
set(OBS_INSTALL_DIR "C:/Program Files/obs-studio" CACHE PATH "OBS Studio installation (for obs.dll)")
if(NOT EXISTS "${LIBOBS_SDK_DIR}/lib/obs.lib" OR NOT EXISTS "${LIBOBS_SDK_DIR}/include/obs.h")
  message(FATAL_ERROR "libobs SDK not found in ${LIBOBS_SDK_DIR}.")
endif()
add_library(klarcam-libobs SHARED IMPORTED GLOBAL)
set_target_properties(klarcam-libobs PROPERTIES
  IMPORTED_IMPLIB "${LIBOBS_SDK_DIR}/lib/obs.lib"
  IMPORTED_LOCATION "${OBS_INSTALL_DIR}/bin/64bit/obs.dll"
  INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_SDK_DIR}/include;${LIBOBS_SDK_DIR}/include/w32-pthreads"
)

configure_file(res/klarcam.rc.in "${CMAKE_CURRENT_BINARY_DIR}/klarcam.rc" @ONLY)
add_library(klarcam-host MODULE
  src/plugin-main.cpp
  src/klarcam-filter.cpp
  src/klarcam-filter.h
  src/engine-link.cpp
  src/engine-link.h
  src/filter-defaults.h
  protocol/klarcam-engine-protocol.h
  protocol/klarcam-engine-text.h
  "${CMAKE_CURRENT_BINARY_DIR}/klarcam.rc"
)
target_include_directories(klarcam-host PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/protocol")
target_link_libraries(klarcam-host PRIVATE klarcam-libobs bcrypt)
target_compile_definitions(klarcam-host PRIVATE PLUGIN_VERSION="${PROJECT_VERSION}" NOMINMAX WIN32_LEAN_AND_MEAN)
# /wd4201: libobs vec2/vec3/vec4 headers use nameless struct/union
target_compile_options(klarcam-host PRIVATE /W4 /wd4201 /utf-8 /permissive- /Zc:__cplusplus /EHsc)
set_target_properties(klarcam-host PROPERTIES PREFIX "" OUTPUT_NAME "klarcam")

if(KLARCAM_HOST_STANDALONE)
  # The OBS 33 layout: <plugins>/klarcam/klarcam.dll + data/. The engine and its
  # data come from the KlarCam setup.
  install(TARGETS klarcam-host LIBRARY DESTINATION "klarcam" RUNTIME DESTINATION "klarcam")
  install(DIRECTORY data/ DESTINATION "klarcam/data")
else()
  # Part of the KlarCam build: staged into the plugin folder with the engine.
  add_custom_command(TARGET klarcam-host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${KLARCAM_STAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:klarcam-host>" "${KLARCAM_STAGE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:klarcam-host>" "${KLARCAM_STAGE_DIR}/"
    COMMENT "Staging the filter host to ${KLARCAM_STAGE_DIR}"
  )
endif()
