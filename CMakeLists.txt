cmake_minimum_required(VERSION 3.25)
project(jdm C)



set(CMAKE_C_STANDARD 11)


add_library(jdm SHARED source/jdm.c)
target_sources(jdm PUBLIC FILE_SET jdm_headers TYPE HEADERS BASE_DIRS include FILES include/jdm.h)
install(TARGETS jdm
        FILE_SET jdm_headers
        DESTINATION "/usr/include"
        LIBRARY
        DESTINATION "/usr/lib")

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(jdm PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
enable_testing()

add_executable(simple_test source/tests/simple_test.c)
target_link_libraries(simple_test PRIVATE jdm)
add_test(NAME simple COMMAND simple_test)

