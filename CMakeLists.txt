cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0022 NEW)

project(tags VERSION 0.1)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(tags)


# if inside subdirectory
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(${PROJECT_NAME}_sub OFF)
else()
    set(${PROJECT_NAME}_sub ON)
endif()


# 3rd party

if(NOT ${PROJECT_NAME}_sub)
    find_package(Qt5Widgets 5.9.5 QUIET REQUIRED)
endif()

if(NOT TARGET Qt5::Widgets)
    message(FATAL_ERROR "Qt5::Widgets is required dependency")
endif()
if(NOT TARGET Qt5::GuiPrivate)
    message(FATAL_ERROR "Qt5::GuiPrivate is required dependency (consider installing qtbase5-private-dev on Ubuntu)")
endif()
if(Qt5Widgets_VERSION VERSION_LESS 5.9.5)
    message(FATAL_ERROR "Minimum supported Qt5 version is 5.9.5")
endif()

# Target

add_library(
    ${PROJECT_NAME}

    include/${PROJECT_NAME}/tags_line_edit.hpp
    include/${PROJECT_NAME}/tags_edit.hpp
    src/tags_line_edit.cpp
    src/tags_edit.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 AUTOMOC ON)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_include_directories(${PROJECT_NAME}
    SYSTEM
    PUBLIC
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Widgets Qt5::GuiPrivate)

# compile options/definitions
if(NOT ${PROJECT_NAME}_sub)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_development)
endif()


# Setup package config

install(DIRECTORY include/${PROJECT_NAME} DESTINATION include)

if(NOT ${PROJECT_NAME}_sub)
    include(CMakePackageConfigHelpers)
    set(CONFIG_PACKAGE_INSTALL_DIR lib/cmake/${PROJECT_NAME})

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        COMPATIBILITY SameMajorVersion
    )

    install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_development
        EXPORT ${PROJECT_NAME}-targets
        DESTINATION lib
    )

    install(EXPORT ${PROJECT_NAME}-targets
        DESTINATION ${CONFIG_PACKAGE_INSTALL_DIR}
    )

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        @ONLY
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        DESTINATION ${CONFIG_PACKAGE_INSTALL_DIR}
    )
else()
    install(TARGETS ${PROJECT_NAME} DESTINATION lib)
endif()
