cmake_minimum_required(VERSION 3.20)

project(gthreads C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_program(CLANG_FORMAT_BIN clang-format)

option(GTHREADS_ENABLE_SANITIZERS "Enable ASan/UBSan in Debug builds" ON)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(GTHREADS_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

add_library(gthreads STATIC
    src/runtime/runtime.c
    src/context/context.c
    src/sched/scheduler.c
    src/thread/thread.c
    src/sync/mutex.c
    src/sync/sem.c
    src/sync/cond.c
    src/trace/trace.c
)

target_include_directories(gthreads
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
)

enable_testing()

find_path(CMOCKA_INCLUDE_DIR cmocka.h)
find_library(CMOCKA_LIBRARY NAMES cmocka)

if(NOT CMOCKA_INCLUDE_DIR OR NOT CMOCKA_LIBRARY)
    message(FATAL_ERROR "CMocka not found. Install libcmocka-dev (or equivalent) to build tests.")
endif()

add_executable(gthreads_tests
    tests/test_main.c
    tests/test_api_validation.c
    tests/test_runtime.c
)

target_include_directories(gthreads_tests PRIVATE ${CMOCKA_INCLUDE_DIR})

target_link_libraries(gthreads_tests PRIVATE gthreads ${CMOCKA_LIBRARY})

add_test(NAME gthreads_tests COMMAND gthreads_tests)

if(CLANG_FORMAT_BIN)
    set(GTHREADS_FORMAT_FILES
        include/gthreads/gthreads.h
        src/internal/runtime_state.h
        src/runtime/runtime.c
        src/context/context.c
        src/sched/scheduler.c
        src/thread/thread.c
        src/sync/mutex.c
        src/sync/sem.c
        src/sync/cond.c
        src/trace/trace.c
        tests/test_main.c
        tests/test_api_validation.c
        tests/test_runtime.c
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i ${GTHREADS_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting gthreads sources with clang-format"
    )

    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror ${GTHREADS_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking source formatting with clang-format"
    )
else()
    message(WARNING "clang-format not found; format and format-check targets disabled")
endif()
