cmake_minimum_required(VERSION 3.10 FATAL_ERROR) # because of c++17

project(amuse)

if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()

if (NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(NOT MSVC)
    add_compile_options(-Wno-narrowing)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/boo AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/athena)
  message(STATUS "Preparing standalone build")
  add_subdirectory(boo)
  add_subdirectory(athena)
endif()

option(AMUSE_BUILD_EDITOR "Build Amuse with editor enabled (includes VST)" ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/bintocHelpers.cmake)

add_library(amuse
  lib/AudioGroup.cpp
  lib/AudioGroupData.cpp
  lib/AudioGroupPool.cpp
  lib/AudioGroupProject.cpp
  lib/AudioGroupSampleDirectory.cpp
  lib/Common.cpp
  lib/ContainerRegistry.cpp
  lib/DirectoryEnumerator.cpp
  lib/DSPCodec.cpp
  lib/EffectChorus.cpp
  lib/EffectDelay.cpp
  lib/EffectReverb.cpp
  lib/Emitter.cpp
  lib/Engine.cpp
  lib/Envelope.cpp
  lib/Listener.cpp
  lib/N64MusyXCodec.cpp
  lib/Sequencer.cpp
  lib/SongConverter.cpp
  lib/SongState.cpp
  lib/SoundMacroState.cpp
  lib/Studio.cpp
  lib/Submix.cpp
  lib/Voice.cpp
  lib/VolumeTable.cpp

  include/amuse/amuse.hpp
  include/amuse/AudioGroup.hpp
  include/amuse/AudioGroupData.hpp
  include/amuse/AudioGroupPool.hpp
  include/amuse/AudioGroupProject.hpp
  include/amuse/AudioGroupSampleDirectory.hpp
  include/amuse/Common.hpp
  include/amuse/ContainerRegistry.hpp
  include/amuse/DirectoryEnumerator.hpp
  include/amuse/DSPCodec.hpp
  include/amuse/EffectBase.hpp
  include/amuse/EffectChorus.hpp
  include/amuse/EffectDelay.hpp
  include/amuse/EffectReverb.hpp
  include/amuse/Emitter.hpp
  include/amuse/Engine.hpp
  include/amuse/Entity.hpp
  include/amuse/Envelope.hpp
  include/amuse/IBackendSubmix.hpp
  include/amuse/IBackendVoice.hpp
  include/amuse/IBackendVoiceAllocator.hpp
  include/amuse/Listener.hpp
  include/amuse/N64MusyXCodec.hpp
  include/amuse/Sequencer.hpp
  include/amuse/SongConverter.hpp
  include/amuse/SoundMacroState.hpp
  include/amuse/SongState.hpp
  include/amuse/Submix.hpp
  include/amuse/Studio.hpp
  include/amuse/Voice.hpp
  include/amuse/VolumeTable.hpp
)

target_include_directories(amuse PUBLIC include)
target_link_libraries(amuse
  athena-core
  lzokay
  logvisor
  fmt
  ${ZLIB_LIBRARIES}
)

target_atdna(amuse atdna_AudioGroupPool.cpp include/amuse/AudioGroupPool.hpp)
target_atdna(amuse atdna_AudioGroupProject.cpp include/amuse/AudioGroupProject.hpp)
target_atdna(amuse atdna_AudioGroupSampleDirectory.cpp include/amuse/AudioGroupSampleDirectory.hpp)

if(NX)
  target_sources(amuse PRIVATE include/switch_math.hpp)
endif()

if(TARGET boo)
  target_sources(amuse PRIVATE lib/BooBackend.cpp include/amuse/BooBackend.hpp)
  target_link_libraries(amuse boo)
endif()

if (MSVC)
  target_compile_options(amuse PRIVATE
    # Enforce various standards compliant behavior.
    $<$<COMPILE_LANGUAGE:CXX>:/permissive->

    # Enable standard volatile semantics.
    $<$<COMPILE_LANGUAGE:CXX>:/volatile:iso>

    # Reports the proper value for the __cplusplus preprocessor macro.
    $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>

    # Use latest C++ standard.
    $<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>
  )
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # Flags for MSVC (not clang-cl)
    target_compile_options(amuse PRIVATE
      # Allow constexpr variables to have explicit external linkage.
      $<$<COMPILE_LANGUAGE:CXX>:/Zc:externConstexpr>

      # Assume that new throws exceptions, allowing better code generation.
      $<$<COMPILE_LANGUAGE:CXX>:/Zc:throwingNew>
    )
  endif()
else()
  target_compile_options(amuse PRIVATE -Wno-unknown-pragmas)
endif()

if(COMMAND add_sanitizers)
  add_sanitizers(amuse)
endif()

if(TARGET boo AND NOT WINDOWS_STORE AND NOT NX)
  # AudioUnit Target (OS X only)
  add_subdirectory(AudioUnit)

  # VST Target
  add_subdirectory(VST)

  # Multi-platform CLI tools

  # Player
  set(CMAKE_CXX_STANDARD_LIBRARIES "-lksuser -lole32")
  add_executable(amuseplay WIN32 driver/amuseplay.cpp)
  target_link_libraries(amuseplay amuse logvisor)
  install(TARGETS amuseplay)

  # Converter
  add_executable(amuseconv driver/amuseconv.cpp)
  target_link_libraries(amuseconv amuse logvisor)
  install(TARGETS amuseconv)

  # Renderer
  add_executable(amuserender driver/amuserender.cpp)
  target_link_libraries(amuserender amuse logvisor)
  install(TARGETS amuserender)

  if(COMMAND add_sanitizers)
    add_sanitizers(amuseplay)
    add_sanitizers(amuseconv)
    add_sanitizers(amuserender)
  endif()

  # Editor
  find_package(Qt6 COMPONENTS Widgets PATHS /usr/local/opt/qt)
  if (Qt6Widgets_FOUND AND AMUSE_BUILD_EDITOR)
    message(STATUS "Qt6 found, amuse-gui will be built")
    add_subdirectory(Editor)
  else()
    if (NOT AMUSE_BUILD_EDITOR)
      message(STATUS "amuse-gui has been disabled and will not be built, to enable pass `-DAMUSE_BUILD_EDITOR=ON` to cmake")
    else()
      message(STATUS "Qt6 not found, amuse-gui will not be built")
    endif()
  endif()
endif()
