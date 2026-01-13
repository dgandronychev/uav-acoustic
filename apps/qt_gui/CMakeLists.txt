cmake_minimum_required(VERSION 3.24)

# ===== Qt path (fallback) =====
if (WIN32 AND NOT DEFINED CMAKE_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/msvc2022_64")
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup()

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
# core_audio (libsndfile via vcpkg)
# =================================================
find_package(SndFile CONFIG QUIET)
if (NOT SndFile_FOUND)
  find_package(sndfile CONFIG REQUIRED)
endif()

add_library(core_audio STATIC
  ${CMAKE_SOURCE_DIR}/core/audio/src/sndfile_replay_source.cc
)

target_include_directories(core_audio PUBLIC
  ${CMAKE_SOURCE_DIR}/core/audio/include
)

if (TARGET SndFile::sndfile)
  target_link_libraries(core_audio PUBLIC SndFile::sndfile)
elseif (TARGET sndfile::sndfile)
  target_link_libraries(core_audio PUBLIC sndfile::sndfile)
else()
  message(FATAL_ERROR "libsndfile найден, но импорт-таргет не обнаружен (ожидали SndFile::sndfile или sndfile::sndfile)")
endif()

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
# core_detect (Step 4A mock detector)
# =================================================
add_library(core_detect STATIC
  ${CMAKE_SOURCE_DIR}/core/detect/src/mock_detector.cc
)

target_include_directories(core_detect PUBLIC
  ${CMAKE_SOURCE_DIR}/core/detect/include
)

target_compile_features(core_detect PUBLIC cxx_std_20)

# =================================================
# core_logic (FSM)  
# =================================================
add_library(core_logic STATIC
  ${CMAKE_SOURCE_DIR}/core/logic/src/fsm.cc
)

target_include_directories(core_logic PUBLIC
  ${CMAKE_SOURCE_DIR}/core/logic/include
  ${CMAKE_SOURCE_DIR}/core/telemetry/include
)

target_compile_features(core_logic PUBLIC cxx_std_20)
# =================================================
# core_fsm 
# =================================================
add_library(core_fsm STATIC
  ${CMAKE_SOURCE_DIR}/core/fsm/src/event_fsm.cc
)
target_include_directories(core_fsm PUBLIC
  ${CMAKE_SOURCE_DIR}/core/fsm/include
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
  AUTOUIC ON
  AUTORCC ON
)

target_include_directories(qt_bridge PUBLIC
  ${CMAKE_SOURCE_DIR}/qt_bridge/include
  ${CMAKE_SOURCE_DIR}/core/telemetry/include
  ${CMAKE_SOURCE_DIR}/core/audio/include
  ${CMAKE_SOURCE_DIR}/core/dsp/include
  ${CMAKE_SOURCE_DIR}/core/detect/include
)

target_link_libraries(qt_bridge PUBLIC
  Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
  core_telemetry
  core_audio
  core_dsp
  core_detect
  core_logic
  core_fsm
  core_segment
)

target_compile_features(qt_bridge PUBLIC cxx_std_20)

# =================================================
# App
# =================================================
qt_add_executable(uav_acoustic_gui
  src/main.cpp
)

target_link_libraries(uav_acoustic_gui PRIVATE
  Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
  qt_bridge
)

# =================================================
# QML resources
# =================================================
set(QML_MAIN ${CMAKE_SOURCE_DIR}/qml/Main.qml)
set_source_files_properties(${QML_MAIN} PROPERTIES QT_RESOURCE_ALIAS "Main.qml")

qt_add_resources(uav_acoustic_gui qml_resources
  PREFIX "/"
  FILES ${QML_MAIN}
)

set_target_properties(uav_acoustic_gui PROPERTIES WIN32_EXECUTABLE TRUE)
