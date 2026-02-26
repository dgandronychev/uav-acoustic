cmake_minimum_required(VERSION 3.24)

# ===== Qt path (fallback) =====
if (WIN32 AND NOT DEFINED CMAKE_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/msvc2022_64")
endif()

# NOTE:
# Keep this as a subdirectory CMake file (no nested project()) so IDE auto-setup
# hooks are not re-run twice and to simplify cross-platform configuration.

# Prefer Qt6, but allow Qt5 fallback for embedded Linux kits (e.g. RK3588).
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Qml Quick)

if (QT_VERSION_MAJOR EQUAL 6)
  if (COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
  endif()
endif()

# =================================================
# Build accelerators
# =================================================
option(UAV_UNITY "Enable unity builds for core libs" ON)

# =================================================
# core_telemetry
# =================================================
add_library(core_telemetry STATIC
  ${CMAKE_SOURCE_DIR}/core/telemetry/src/telemetry_bus.cc
)
target_include_directories(core_telemetry PUBLIC
  ${CMAKE_SOURCE_DIR}/core/telemetry/include
)
target_compile_features(core_telemetry PUBLIC cxx_std_20)

# =================================================
# core_audio (libsndfile)
# - сначала ищем config-пакет
# - если он недоступен/битый, используем локальный fallback из vcpkg_installed
# =================================================
if (WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET x64-windows)
endif()

set(_UAV_LOCAL_VCPKG_ROOT "")
if (WIN32 AND DEFINED VCPKG_TARGET_TRIPLET)
  set(_UAV_LOCAL_VCPKG_ROOT "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
endif()

if (_UAV_LOCAL_VCPKG_ROOT AND EXISTS "${_UAV_LOCAL_VCPKG_ROOT}/share/SndFile/SndFileConfig.cmake")
  list(PREPEND CMAKE_PREFIX_PATH "${_UAV_LOCAL_VCPKG_ROOT}")
endif()

set(_UAV_SNDFILE_TARGET "")
if (WIN32)
  find_package(SndFile CONFIG QUIET)
endif()

if (TARGET SndFile::sndfile)
  set(_UAV_SNDFILE_TARGET SndFile::sndfile)
elseif (TARGET sndfile::sndfile)
  set(_UAV_SNDFILE_TARGET sndfile::sndfile)
else()
  set(_UAV_SNDFILE_RELEASE_LIB "")
  set(_UAV_SNDFILE_INCLUDE_DIR "")

  # 1) Local vcpkg fallback (mostly for Windows dev env)
  if (_UAV_LOCAL_VCPKG_ROOT)
    find_library(_UAV_SNDFILE_RELEASE_LIB
      NAMES sndfile libsndfile
      PATHS "${_UAV_LOCAL_VCPKG_ROOT}/lib"
      NO_DEFAULT_PATH
    )
    find_path(_UAV_SNDFILE_INCLUDE_DIR
      NAMES sndfile.h
      PATHS "${_UAV_LOCAL_VCPKG_ROOT}/include"
      NO_DEFAULT_PATH
    )
  endif()

  # 2) System fallback (Linux, including RK3588 kits)
  if (NOT _UAV_SNDFILE_RELEASE_LIB OR NOT _UAV_SNDFILE_INCLUDE_DIR)
    find_library(_UAV_SNDFILE_RELEASE_LIB NAMES sndfile libsndfile)
    find_path(_UAV_SNDFILE_INCLUDE_DIR NAMES sndfile.h)
  endif()

  if (_UAV_SNDFILE_RELEASE_LIB AND _UAV_SNDFILE_INCLUDE_DIR)
    add_library(UAV::sndfile UNKNOWN IMPORTED)
    set_target_properties(UAV::sndfile PROPERTIES
      IMPORTED_LOCATION "${_UAV_SNDFILE_RELEASE_LIB}"
      INTERFACE_INCLUDE_DIRECTORIES "${_UAV_SNDFILE_INCLUDE_DIR}"
      MAP_IMPORTED_CONFIG_DEBUG Release
      MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
      MAP_IMPORTED_CONFIG_MINSIZEREL Release
    )
    set(_UAV_SNDFILE_TARGET UAV::sndfile)
    message(WARNING
      "SndFile config package is unavailable or broken; using fallback library: ${_UAV_SNDFILE_RELEASE_LIB}")
  else()
    message(FATAL_ERROR
      "libsndfile не найден. Проверены config package, локальный vcpkg fallback и системные пути. "
      "Установите пакет dev-заголовков (например, libsndfile1-dev)."
    )
  endif()
endif()

function(_uav_fix_missing_sndfile_debug_import target_name)
  if (NOT TARGET ${target_name})
    return()
  endif()

  # Some local vcpkg installs provide only release artifacts for libsndfile.
  # In that case CMake may fail during configure because IMPORTED_*_DEBUG points
  # to a non-existent file. Reuse release binary for Debug-like configs.
  get_target_property(_snd_dbg_implib ${target_name} IMPORTED_IMPLIB_DEBUG)
  get_target_property(_snd_rel_implib ${target_name} IMPORTED_IMPLIB_RELEASE)
  get_target_property(_snd_dbg_location ${target_name} IMPORTED_LOCATION_DEBUG)
  get_target_property(_snd_rel_location ${target_name} IMPORTED_LOCATION_RELEASE)

  set(_missing_debug_artifact OFF)
  if (_snd_dbg_implib AND NOT EXISTS "${_snd_dbg_implib}")
    set(_missing_debug_artifact ON)
  endif()
  if (_snd_dbg_location AND NOT EXISTS "${_snd_dbg_location}")
    set(_missing_debug_artifact ON)
  endif()

  if (_missing_debug_artifact)
    if ((_snd_rel_implib AND EXISTS "${_snd_rel_implib}") OR (_snd_rel_location AND EXISTS "${_snd_rel_location}"))
      set_property(TARGET ${target_name} PROPERTY MAP_IMPORTED_CONFIG_DEBUG Release)
      set_property(TARGET ${target_name} PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
      set_property(TARGET ${target_name} PROPERTY MAP_IMPORTED_CONFIG_MINSIZEREL Release)
      message(WARNING
        "${target_name}: Debug artifact is missing, falling back to Release import library from SndFile package")
    endif()
  endif()
endfunction()

add_library(core_audio STATIC
  ${CMAKE_SOURCE_DIR}/core/audio/src/sndfile_replay_source.cc
)
target_include_directories(core_audio PUBLIC
  ${CMAKE_SOURCE_DIR}/core/audio/include
)

target_link_libraries(core_audio PUBLIC ${_UAV_SNDFILE_TARGET})
target_compile_features(core_audio PUBLIC cxx_std_20)

# =================================================
# core_dsp (PCEN-mel)
# =================================================
add_library(core_dsp STATIC
  ${CMAKE_SOURCE_DIR}/core/dsp/src/mel_filterbank.cc
  ${CMAKE_SOURCE_DIR}/core/dsp/src/pcen_extractor.cc
  ${CMAKE_SOURCE_DIR}/core/dsp/src/pcen_ring_buffer.cc
)
target_include_directories(core_dsp PUBLIC
  ${CMAKE_SOURCE_DIR}/core/dsp/include
)
target_compile_features(core_dsp PUBLIC cxx_std_20)

# =================================================
# core_detect (mock detector)
# =================================================
add_library(core_detect STATIC
  ${CMAKE_SOURCE_DIR}/core/detect/src/mock_detector.cc
)
target_include_directories(core_detect PUBLIC
  ${CMAKE_SOURCE_DIR}/core/detect/include
)
target_compile_features(core_detect PUBLIC cxx_std_20)

# =================================================
# core_fsm (Event FSM)
# =================================================
add_library(core_fsm STATIC
  ${CMAKE_SOURCE_DIR}/core/fsm/src/event_fsm.cc
)
target_include_directories(core_fsm PUBLIC
  ${CMAKE_SOURCE_DIR}/core/fsm/include
  ${CMAKE_SOURCE_DIR}/core/telemetry/include
)
target_link_libraries(core_fsm PUBLIC core_telemetry)
target_compile_features(core_fsm PUBLIC cxx_std_20)

# =================================================
# core_segment (Segment Builder)
# =================================================
add_library(core_segment STATIC
  ${CMAKE_SOURCE_DIR}/core/segment/src/segment_builder.cc
)
target_include_directories(core_segment PUBLIC
  ${CMAKE_SOURCE_DIR}/core/segment/include
  ${CMAKE_SOURCE_DIR}/core/dsp/include
)
target_compile_features(core_segment PUBLIC cxx_std_20)

# =================================================
# TensorFlow Lite (optional)
# - If provided by Conan: expects target tensorflow::tensorflowlite
# - If not found: we build without core_tflite
# =================================================
option(UAV_ENABLE_TFLITE "Enable TensorFlow Lite integration" OFF)

set(UAV_HAVE_TFLITE OFF)

if (UAV_ENABLE_TFLITE)
  find_package(tensorflowlite CONFIG QUIET)

  if (TARGET tensorflow::tensorflowlite)
    set(UAV_HAVE_TFLITE ON)
  else()
    message(WARNING
      "TensorFlow Lite not found (tensorflowliteConfig.cmake). "
      "core_tflite будет отключён. "
      "Если нужен TFLite — подключите Conan toolchain/пакет, который экспортирует tensorflow::tensorflowlite."
    )
  endif()
endif()

# =================================================
# core_tflite (runner + TCN detector) [optional]
# =================================================
if (UAV_HAVE_TFLITE)
  set(TCN_DETECTOR_SRC ${CMAKE_SOURCE_DIR}/core/tflite/src/tcn_detector.cc)

  set(CORE_TFLITE_SOURCES
    ${CMAKE_SOURCE_DIR}/core/tflite/src/tflite_runner.cc
  )

  if (EXISTS ${TCN_DETECTOR_SRC})
    list(APPEND CORE_TFLITE_SOURCES ${TCN_DETECTOR_SRC})
  endif()

  add_library(core_tflite STATIC
    ${CORE_TFLITE_SOURCES}
  )

  target_include_directories(core_tflite PUBLIC
    ${CMAKE_SOURCE_DIR}/core/tflite/include
  )

  target_link_libraries(core_tflite PUBLIC
    tensorflow::tensorflowlite
  )

  target_compile_features(core_tflite PUBLIC cxx_std_20)
endif()

# =================================================
# qt_bridge
# =================================================
add_library(qt_bridge STATIC
  ${CMAKE_SOURCE_DIR}/qt_bridge/src/pcen_image_provider.cc
  ${CMAKE_SOURCE_DIR}/qt_bridge/src/telemetry_provider.cc
  ${CMAKE_SOURCE_DIR}/qt_bridge/include/qt_bridge/pcen_image_provider.h
  ${CMAKE_SOURCE_DIR}/qt_bridge/include/qt_bridge/telemetry_provider.h
)

set_target_properties(qt_bridge PROPERTIES
  AUTOMOC ON
  AUTOUIC OFF
  AUTORCC OFF
)

target_include_directories(qt_bridge PUBLIC
  ${CMAKE_SOURCE_DIR}/qt_bridge/include
  ${CMAKE_SOURCE_DIR}/core/telemetry/include
  ${CMAKE_SOURCE_DIR}/core/audio/include
  ${CMAKE_SOURCE_DIR}/core/dsp/include
  ${CMAKE_SOURCE_DIR}/core/detect/include
  ${CMAKE_SOURCE_DIR}/core/fsm/include
  ${CMAKE_SOURCE_DIR}/core/segment/include
)

if (UAV_HAVE_TFLITE)
  target_include_directories(qt_bridge PUBLIC
    ${CMAKE_SOURCE_DIR}/core/tflite/include
  )
endif()

target_link_libraries(qt_bridge PUBLIC
  Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
  core_telemetry
  core_audio
  core_dsp
  core_detect
  core_fsm
  core_segment
)

if (UAV_HAVE_TFLITE)
  target_link_libraries(qt_bridge PUBLIC core_tflite)
endif()

target_compile_features(qt_bridge PUBLIC cxx_std_20)

# =================================================
# Unity build (optional)
# =================================================
if (UAV_UNITY)
  set_target_properties(core_telemetry core_audio core_dsp core_detect core_fsm core_segment
    PROPERTIES UNITY_BUILD ON
  )
  if (TARGET core_tflite)
    set_target_properties(core_tflite PROPERTIES UNITY_BUILD ON)
  endif()
endif()

# =================================================
# App
# =================================================
if (QT_VERSION_MAJOR EQUAL 6)
  qt_add_executable(uav_acoustic_gui
    src/main.cpp
  )
else()
  add_executable(uav_acoustic_gui
    src/main.cpp
  )
endif()

#target_include_directories(uav_acoustic_gui PRIVATE
#  ${CMAKE_SOURCE_DIR}/core/tflite/include
#)

target_link_libraries(uav_acoustic_gui PRIVATE
  Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
  qt_bridge
)

#target_include_directories(uav_acoustic_gui PRIVATE
#  ${CMAKE_SOURCE_DIR}/core/tflite/include
#)


# =================================================
# QML resources
# =================================================
set(QML_MAIN ${CMAKE_SOURCE_DIR}/qml/Main.qml)
set_source_files_properties(${QML_MAIN} PROPERTIES QT_RESOURCE_ALIAS "Main.qml")

if (QT_VERSION_MAJOR EQUAL 6)
  qt_add_resources(uav_acoustic_gui qml_resources
    PREFIX "/"
    FILES ${QML_MAIN}
  )
else()
  qt5_add_resources(UAV_QML_RCC ${QML_MAIN})
  target_sources(uav_acoustic_gui PRIVATE ${UAV_QML_RCC})
endif()

set_target_properties(uav_acoustic_gui PROPERTIES WIN32_EXECUTABLE TRUE)

# =================================================
# Windows runtime deployment
# =================================================
if (WIN32)
  # 1) Базовый деплой runtime DLL, которые CMake умеет вывести по графу линковки.
  add_custom_command(TARGET uav_acoustic_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:uav_acoustic_gui>
      $<TARGET_FILE_DIR:uav_acoustic_gui>
    COMMAND_EXPAND_LISTS
  )

  # 2) Деплой Qt-плагинов (platforms/qwindows[d].dll и т.п.), иначе Qt GUI не стартует.
    get_target_property(_UAV_QMAKE_EXE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
  if (_UAV_QMAKE_EXE)
    get_filename_component(_UAV_QT_BIN_DIR "${_UAV_QMAKE_EXE}" DIRECTORY)
    set(_UAV_WINDEPLOYQT "${_UAV_QT_BIN_DIR}/windeployqt.exe")

    if (EXISTS "${_UAV_WINDEPLOYQT}")
      add_custom_command(TARGET uav_acoustic_gui POST_BUILD
        COMMAND "${_UAV_WINDEPLOYQT}"
          --qmldir "${CMAKE_SOURCE_DIR}/qml"
          "$<TARGET_FILE:uav_acoustic_gui>"
      )
    else()
      message(WARNING "windeployqt.exe не найден рядом с qmake: ${_UAV_WINDEPLOYQT}")
    endif()
  endif()

  # 3) Подтягиваем полное vcpkg runtime-окружение (bin/debug/bin),
  #    чтобы не ловить каскад отсутствующих транзитивных DLL.
  if (NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET x64-windows)
  endif()

  set(_UAV_LOCAL_VCPKG_BIN "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin")
  set(_UAV_LOCAL_VCPKG_DEBUG_BIN "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/bin")

  if (EXISTS "${_UAV_LOCAL_VCPKG_BIN}")
    add_custom_command(TARGET uav_acoustic_gui POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${_UAV_LOCAL_VCPKG_BIN}"
        "$<TARGET_FILE_DIR:uav_acoustic_gui>"
      VERBATIM
      COMMENT "Copy full release runtime DLL set from local vcpkg"
    )
  endif()

  if (EXISTS "${_UAV_LOCAL_VCPKG_DEBUG_BIN}")
    add_custom_command(TARGET uav_acoustic_gui POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${_UAV_LOCAL_VCPKG_DEBUG_BIN}"
        "$<TARGET_FILE_DIR:uav_acoustic_gui>"
      VERBATIM
      COMMENT "Copy full debug runtime DLL set from local vcpkg"
    )
  endif()
endif()