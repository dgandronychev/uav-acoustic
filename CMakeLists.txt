cmake_minimum_required(VERSION 3.24)

project(uav_acoustic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Удобно для MSVC
if (MSVC)
  add_compile_options(/W4 /permissive- /EHsc)
endif()

add_subdirectory(apps/qt_gui)
