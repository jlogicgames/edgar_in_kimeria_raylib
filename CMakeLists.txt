cmake_minimum_required(VERSION 3.24)

project(edgard_in_kimeria VERSION 0.1.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# This project intentionally remains on raylib 5.5: later phases use APIs introduced in
# raylib 5.x, and a fixed tag keeps desktop and web builds reproducible.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_library(flecs STATIC third_party/flecs/flecs.c)
target_compile_definitions(flecs PUBLIC FLECS_CUSTOM_BUILD FLECS_SYSTEM FLECS_PIPELINE FLECS_LOG)
target_compile_definitions(flecs PRIVATE FLECS_OS_API_IMPL)
target_include_directories(flecs SYSTEM PUBLIC third_party/flecs)

add_library(yxml STATIC third_party/yxml/yxml.c)
target_include_directories(yxml SYSTEM PUBLIC third_party/yxml)

add_executable(edgard_in_kimeria src/main.c)
target_link_libraries(edgard_in_kimeria PRIVATE raylib flecs yxml)
target_include_directories(edgard_in_kimeria PRIVATE src)
set_target_properties(edgard_in_kimeria PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# The desktop executable resolves assets relative to its own directory. Keep a
# copy beside it for local runs and CTest while the web build preloads the same
# source directory directly (below).
add_custom_command(TARGET edgard_in_kimeria POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:edgard_in_kimeria>/assets
)

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(edgard_in_kimeria PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if (EMSCRIPTEN)
    set_target_properties(edgard_in_kimeria PROPERTIES SUFFIX ".html")
    target_link_options(edgard_in_kimeria PRIVATE
        "SHELL:-sUSE_GLFW=3"
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
    )
endif()

enable_testing()
add_test(NAME scaffold_starts COMMAND edgard_in_kimeria --check-scaffold)
