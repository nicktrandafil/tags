cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0022 NEW)

project(everload_tags VERSION 0.1)

include(common.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# if inside subdirectory
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(${PROJECT_NAME}_sub OFF)
else()
  set(${PROJECT_NAME}_sub ON)
endif()

# 3rd party

if(NOT ${PROJECT_NAME}_sub)
  find_package(
    QT NAMES Qt6 Qt5
    COMPONENTS Widgets
    QUIET)
  message(STATUS "Using Qt${QT_VERSION_MAJOR}")
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED QUIET COMPONENTS Widgets)
endif()

if(NOT QT_VERSION_MAJOR)
  message(FATAL_ERROR "QT_VERSION_MAJOR should be either 5 or 6")
endif()

if(NOT TARGET Qt${QT_VERSION_MAJOR}::Widgets)
  message(FATAL_ERROR "Qt${QT_VERSION_MAJOR}::Widgets is required dependency")
endif()

if(Qt${QT_VERSION_MAJOR}Widgets_VERSION VERSION_LESS 5.12.0)
  message(FATAL_ERROR "Minimum supported Qt5 version is 5.12.0")
endif()

# Target

add_library(
  ${PROJECT_NAME}
  include/${PROJECT_NAME}/config.hpp
  include/${PROJECT_NAME}/tags_line_edit.hpp
  include/${PROJECT_NAME}/tags_edit.hpp
  src/${PROJECT_NAME}/tags_edit.cpp
  src/${PROJECT_NAME}/tags_line_edit.cpp
  src/${PROJECT_NAME}/scope_exit.hpp
  src/${PROJECT_NAME}/common.hpp
  src/${PROJECT_NAME}/util.hpp)

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(${PROJECT_NAME} SYSTEM
                           PUBLIC $<INSTALL_INTERFACE:include>)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)
set_target_build_settings(${PROJECT_NAME})

# Testing app

option(everload_tags_BUILD_TESTING_APP "Build testing app" OFF)
if(everload_tags_BUILD_TESTING_APP)
  add_executable(app test/app/main.cpp test/app/form.h test/app/form.cpp
                     test/app/form.ui)
  set_target_build_settings(app)
  target_link_libraries(app PRIVATE ${PROJECT_NAME})
endif()

# Unit tests
option(everload_tags_TEST "Build unit tests" OFF)
if(everload_tags_TEST)
  find_package(Catch2 REQUIRED)
  add_executable(test_everload_tags test/util.cpp)
  target_include_directories(test_everload_tags PRIVATE src)
  target_link_libraries(test_everload_tags PRIVATE Catch2::Catch2WithMain
                                                   Qt${QT_VERSION_MAJOR}::Core)
  set_target_build_settings(test_everload_tags)
endif()

# Setup package config

install(DIRECTORY include/${PROJECT_NAME} DESTINATION include)

if(NOT ${PROJECT_NAME}_sub)
  include(CMakePackageConfigHelpers)
  set(CONFIG_PACKAGE_INSTALL_DIR lib/cmake/${PROJECT_NAME})

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
    COMPATIBILITY SameMajorVersion)

  install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    DESTINATION lib)

  install(EXPORT ${PROJECT_NAME}-targets
          DESTINATION ${CONFIG_PACKAGE_INSTALL_DIR})

  configure_file(${PROJECT_NAME}-config.cmake.in
                 ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake @ONLY)

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
          DESTINATION ${CONFIG_PACKAGE_INSTALL_DIR})
else()
  install(TARGETS ${PROJECT_NAME} DESTINATION lib)
endif()
