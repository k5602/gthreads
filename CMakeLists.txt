cmake_minimum_required(VERSION 3.20)

project(gthreads C ASM)

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

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
	set(GTHREADS_ASM_SOURCE src/context/ctx_x86_64.S)
else()
	message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

add_library(gthreads STATIC
				src/runtime/runtime.c
				src/context/context.c
				${GTHREADS_ASM_SOURCE}
				src/context/stack.c
				src/sched/scheduler.c
				src/thread/thread.c
				src/sync/mutex.c
				src/sync/sem.c
				src/sync/cond.c
				src/trace/trace.c
				src/trace/trace_buffer.c
				src/trace/replay.c
				src/trace/fuzz.c
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
				tests/test_context.c
				tests/test_sync_mutex.c
				tests/test_sync_sem.c
				tests/test_sync_cond.c
				tests/test_trace_replay.c
)

target_include_directories(gthreads_tests
				PRIVATE
								${CMOCKA_INCLUDE_DIR}
								${CMAKE_CURRENT_SOURCE_DIR}/src/internal
)

target_link_libraries(gthreads_tests PRIVATE gthreads ${CMOCKA_LIBRARY})

add_test(NAME gthreads_tests COMMAND gthreads_tests)

# Examples
add_executable(basic_threads examples/basic_threads.c)
target_link_libraries(basic_threads gthreads)

add_executable(sync_demo examples/sync_demo.c)
target_link_libraries(sync_demo gthreads)

add_executable(trace_replay examples/trace_replay.c)
target_link_libraries(trace_replay gthreads)

# Benchmarks
add_executable(bench_context_switch benchmarks/bench_context_switch.c)
target_link_libraries(bench_context_switch gthreads)

add_executable(bench_thread_create benchmarks/bench_thread_create.c)
target_link_libraries(bench_thread_create gthreads)

add_executable(bench_mutex benchmarks/bench_mutex.c)
target_link_libraries(bench_mutex gthreads)

add_executable(bench_semaphore benchmarks/bench_semaphore.c)
target_link_libraries(bench_semaphore gthreads)

if(CLANG_FORMAT_BIN)
	set(GTHREADS_FORMAT_FILES
								include/gthreads/gthreads.h
								src/internal/runtime_state.h
								src/sync/wait_queue.h
								src/trace/trace_format.h
								src/runtime/runtime.c
								src/context/context.c
								src/context/stack.c
								src/sched/scheduler.c
								src/thread/thread.c
								src/sync/mutex.c
								src/sync/sem.c
								src/sync/cond.c
								src/trace/trace.c
								src/trace/trace_buffer.c
								src/trace/replay.c
								src/trace/fuzz.c
								tests/test_main.c
								tests/test_api_validation.c
								tests/test_runtime.c
								tests/test_context.c
								tests/test_sync_mutex.c
								tests/test_sync_sem.c
								tests/test_sync_cond.c
								examples/basic_threads.c
								examples/sync_demo.c
								examples/trace_replay.c
								benchmarks/bench_context_switch.c
								benchmarks/bench_thread_create.c
								benchmarks/bench_mutex.c
								benchmarks/bench_semaphore.c
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
