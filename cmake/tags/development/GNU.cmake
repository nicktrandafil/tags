set(tags_GNU_base_flags -Wall -Wextra -Wpedantic -Werror -ftemplate-backtrace-limit=0)
set(tags_GNU_debug_flags -g -O0)
set(tags_GNU_release_flags -O2 -DNDEBUG -Wno-unused-parameter)

string(CONCAT generator
  "${tags_GNU_base_flags};"
  "$<$<OR:$<CONFIG:DEBUG>,"
         "$<CONFIG:RELWITHDEBINFO>>:${tags_GNU_debug_flags};>"
  "$<$<OR:$<CONFIG:RELEASE>,"
         "$<CONFIG:RELWITHDEBINFO>,"
         "$<CONFIG:MINSIZEREL>>:${tags_GNU_release_flags};>")

target_compile_options(${PROJECT_NAME}_development INTERFACE
  $<$<CXX_COMPILER_ID:GNU>:${generator}>)

target_link_libraries(${PROJECT_NAME}_development INTERFACE
$<$<CXX_COMPILER_ID:GNU>:${generator}>)
