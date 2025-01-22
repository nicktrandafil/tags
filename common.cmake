# Usage: set_target_build_settings(<target-name> [WARNINGS_ARE_ERRORS ON/OFF])
function(set_target_build_settings target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Not target ${target}")
    endif()

    cmake_parse_arguments(arg "" "WARNINGS_ARE_ERRORS" "" ${ARGN})

    if(NOT DEFINED arg_WARNINGS_ARE_ERRORS)
        set(arg_WARNINGS_ARE_ERRORS ON)
    endif()

    message(STATUS "Setting up build for ${target}")
    message(STATUS "  WARNINGS_ARE_ERRORS: ${arg_WARNINGS_ARE_ERRORS}")

    set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

    set(build_options)
    set(link_options)

    set(sancov_flags --coverage -fsanitize=address -fsanitize=undefined -fsanitize=leak
        -fsanitize-address-use-after-scope
    )

    set(warning_flags -Wall -Wextra -Wpedantic -ftemplate-backtrace-limit=0)

    if(arg_WARNINGS_ARE_ERRORS)
        set(error_flags -Werror)
    else()
        set(error_flags)
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(build_options ${warning_flags})
        if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo|MinSizeRel")
            set(build_options ${build_options} ${error_flags} -fconcepts)
        elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(build_options ${build_options} ${sancov_flags})
            set(link_options ${link_options} ${sancov_flags})
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(build_options ${warning_flags} -Wno-unneeded-internal-declaration)
        if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo|MinSizeRel")
            set(build_options ${build_options} ${error_flags})
        elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(build_options ${build_options} ${sancov_flags})
            set(link_options  ${link_options} ${sancov_flags})
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo|MinSizeRel")
            set(build_options ${build_options} /W4 /WX)
        elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(build_options ${build_options} /W4 /WX)
        endif()
    endif()

    target_compile_options(${target} PRIVATE ${build_options})
    target_link_options(${target} PRIVATE ${link_options})
endfunction()
