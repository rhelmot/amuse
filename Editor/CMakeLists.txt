cmake_minimum_required(VERSION 3.10)

# Automatically handle invoking autorcc, moc, and uic.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 COMPONENTS LinguistTools Network Qml SvgWidgets Widgets Xml REQUIRED)

configure_file(resources/translation_res.qrc translation_res.qrc @ONLY)
set(TRANSLATIONS
  resources/lang_de.ts
)
QT6_CREATE_TRANSLATION(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../lib ${TRANSLATIONS})

add_executable(amuse-gui WIN32 MACOSX_BUNDLE
  ADSREditor.cpp
  ADSREditor.hpp
  Common.cpp
  Common.hpp
  CurveEditor.cpp
  CurveEditor.hpp
  EditorWidget.cpp
  EditorWidget.hpp
  KeyboardWidget.cpp
  KeyboardWidget.hpp
  KeymapEditor.cpp
  KeymapEditor.hpp
  LayersEditor.cpp
  LayersEditor.hpp
  MainWindow.cpp
  MainWindow.hpp
  MainWindow.ui
  MIDIReader.cpp
  MIDIReader.hpp
  NewSoundMacroDialog.cpp
  NewSoundMacroDialog.hpp
  ProjectModel.cpp
  ProjectModel.hpp
  SampleEditor.cpp
  SampleEditor.hpp
  SongGroupEditor.cpp
  SongGroupEditor.hpp
  SoundGroupEditor.cpp
  SoundGroupEditor.hpp
  SoundMacroEditor.cpp
  SoundMacroEditor.hpp
  StatusBarWidget.cpp
  StatusBarWidget.hpp
  StudioSetupWidget.cpp
  StudioSetupWidget.hpp

  main.cpp

  resources/resources.qrc
  ${CMAKE_CURRENT_BINARY_DIR}/translation_res.qrc

  ${QM_FILES}
)
install(TARGETS amuse-gui)

target_compile_definitions(amuse-gui PRIVATE
  # Disable implicit conversions from ASCII to QString.
  -DQT_NO_CAST_FROM_ASCII
  -DQT_NO_CAST_TO_ASCII

  # Disable implicit conversions of QByteArray to const char* or const void*
  -DQT_NO_CAST_FROM_BYTEARRAY

  # Disable narrowing conversions in signal/slot connect() calls.
  -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT

  # Disable unsafe overloads of QProcess' start() function.
  -DQT_NO_PROCESS_COMBINED_ARGUMENT_START

  # Disable implicit QString->QUrl conversions to enforce use of proper resolving functions.
  -DQT_NO_URL_CAST_FROM_STRING

  # Allows for more efficient string concatenation, resulting in less temporaries.
  -DQT_USE_QSTRINGBUILDER 
)

if (MSVC)
  target_compile_options(amuse-gui PRIVATE
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
    target_compile_options(amuse-gui PRIVATE
      # Allow constexpr variables to have explicit external linkage.
      $<$<COMPILE_LANGUAGE:CXX>:/Zc:externConstexpr>

      # Assume that new throws exceptions, allowing better code generation.
      $<$<COMPILE_LANGUAGE:CXX>:/Zc:throwingNew>
    )
  endif()
endif()

if(WIN32)
  target_sources(amuse-gui PRIVATE
    platforms/win/amuse-gui.rc
    platforms/win/amuse-gui.manifest
  )
elseif(APPLE)
  target_sources(amuse-gui PRIVATE
    MacOSExtras.mm
    platforms/mac/mainicon.icns
  )
  set_source_files_properties(platforms/mac/mainicon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
  )
endif()

add_subdirectory(platforms/freedesktop)
declare_qticon_target()
target_sources(amuse-gui PRIVATE mainicon_qt.cpp)

if(COMMAND add_sanitizers)
  add_sanitizers(amuse-gui)
endif()

set_target_properties(amuse-gui PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platforms/mac/Info.plist"
)

target_link_libraries(amuse-gui
  ${PLAT_LIBS}

  Qt6::Network
  Qt6::Qml
  Qt6::SvgWidgets
  Qt6::Widgets
  Qt6::Xml

  amuse
  athena-core
  boo
  ${BOO_SYS_LIBS}
  logvisor
  lzokay
  xxhash
  ${ZLIB_LIBRARIES}
)
