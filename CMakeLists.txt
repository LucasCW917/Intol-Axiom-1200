cmake_minimum_required(VERSION 3.16)
project(axiom1200 VERSION 1.0.0 LANGUAGES C)

# ── Standard ───────────────────────────────────────────────────
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Sources ────────────────────────────────────────────────────
set(SOURCES
        src/main.c
        src/vm.c
        src/exec.c
        src/parser.c
        src/registers.c
        src/mem_manager.c
        src/namespace.c
        src/ops_math.c
        src/ops_logic.c
        src/ops_branch.c
        src/ops_io.c
        src/ops_collections.c
        src/ops_bitwise.c
        src/ops_exceptions.c
        src/ops_time.c
        src/ops_concurrency.c
        src/ops_functions.c
        src/ops_debug.c
)

# ── Executable ─────────────────────────────────────────────────
add_executable(axiom1200 ${SOURCES})

target_include_directories(axiom1200 PRIVATE include)

# ── Threads ────────────────────────────────────────────────────
find_package(Threads REQUIRED)
target_link_libraries(axiom1200 PRIVATE Threads::Threads)

# ── Compiler warnings ──────────────────────────────────────────
target_compile_options(axiom1200 PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wdouble-promotion
        -Wformat=2
        -pedantic
)

# ── Build type defaults ────────────────────────────────────────
# Use: cmake -DCMAKE_BUILD_TYPE=Release ..
#  or: cmake -DCMAKE_BUILD_TYPE=Debug ..
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

target_compile_options(axiom1200 PRIVATE
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
        $<$<CONFIG:Debug>:-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer>
)

target_link_options(axiom1200 PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# ── Install ────────────────────────────────────────────────────
install(TARGETS axiom1200 RUNTIME DESTINATION bin)

# ── Example test targets ───────────────────────────────────────
enable_testing()

foreach(EXAMPLE hello_world fibonacci counter collections bitwise)
    add_test(
            NAME example_${EXAMPLE}
            COMMAND axiom1200 ${CMAKE_SOURCE_DIR}/examples/${EXAMPLE}.apo
    )
endforeach()