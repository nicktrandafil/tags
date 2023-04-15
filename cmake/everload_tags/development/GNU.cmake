set(everload_tags_GNU_base_flags -Wall -Wextra -Wpedantic -Werror -ftemplate-backtrace-limit=0)
set(everload_tags_GNU_debug_flags -g -O0)
set(everload_tags_GNU_release_flags -O2 -DNDEBUG -Wno-unused-parameter)

string(CONCAT generator
    "${everload_tags_GNU_base_flags};"
    "$<$<CONFIG:DEBUG>:${everload_tags_GNU_debug_flags};>"
    "$<$<CONFIG:RELEASE>:${everload_tags_GNU_release_flags};>"
    "$<$<CONFIG:RELWITHDEBINFO>:${everload_tags_GNU_release_flags};-g;>"
    "$<$<CONFIG:MINSIZEREL>:${everload_tags_GNU_release_flags};>"
)

target_compile_options(${PROJECT_NAME}_development INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:${generator}>
)

target_link_libraries(${PROJECT_NAME}_development INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:${generator}>
)
