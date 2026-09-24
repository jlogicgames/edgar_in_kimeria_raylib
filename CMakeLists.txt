cmake_minimum_required(VERSION 3.24)

# CMake's compiler probe would otherwise try to link an unsigned iOS app before
# this project has supplied its bundle settings. A static-library probe is the
# normal cross-compilation-safe check and still verifies the iOS compiler.
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
endif()

project(edgard_in_kimeria VERSION 0.1.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# This project intentionally remains on raylib 5.5: later phases use APIs introduced in
# raylib 5.x, and a fixed tag keeps desktop and web builds reproducible.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Populate raylib without adding its GLFW-oriented CMake project. The iOS
    # target below compiles the pinned source tree with the SDL backend instead.
    set(EIK_RAYLIB_SOURCE_SUBDIR SOURCE_SUBDIR eik-source-only)
endif()
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
    ${EIK_RAYLIB_SOURCE_SUBDIR}
)

# raylib's own CMake target uses GLFW on Apple platforms. The iOS spike instead
# builds its core sources with the upstream SDL backend; keep that experiment
# isolated so the normal desktop and web targets remain on raylib's supported
# CMake path.
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJC)
    FetchContent_MakeAvailable(raylib)

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        sdl2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.11
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(sdl2)

    add_library(eik_raylib_ios STATIC
        ${raylib_SOURCE_DIR}/src/raudio.c
        ${raylib_SOURCE_DIR}/src/rcore.c
        ${raylib_SOURCE_DIR}/src/rmodels.c
        ${raylib_SOURCE_DIR}/src/rshapes.c
        ${raylib_SOURCE_DIR}/src/rtext.c
        ${raylib_SOURCE_DIR}/src/rtextures.c
        ${raylib_SOURCE_DIR}/src/utils.c
    )
    # miniaudio's iOS backend imports AVFoundation declarations. The game and
    # all other raylib core sources remain C99; only this upstream bridge needs
    # Objective-C parsing on Apple platforms.
    set_source_files_properties(${raylib_SOURCE_DIR}/src/raudio.c PROPERTIES
        LANGUAGE OBJC
    )
    target_compile_definitions(eik_raylib_ios PRIVATE
        PLATFORM_DESKTOP_SDL
        GRAPHICS_API_OPENGL_ES3
    )
    target_include_directories(eik_raylib_ios
        PUBLIC ${raylib_SOURCE_DIR}/src
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/ios/include
            ${sdl2_SOURCE_DIR}/include
    )
    find_library(EIK_OPENGL_ES_FRAMEWORK OpenGLES REQUIRED)
    find_library(EIK_AUDIO_TOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
    find_library(EIK_AV_FOUNDATION_FRAMEWORK AVFoundation REQUIRED)
    find_library(EIK_CORE_AUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(EIK_CORE_VIDEO_FRAMEWORK CoreVideo REQUIRED)
    target_link_libraries(eik_raylib_ios PRIVATE
        SDL2::SDL2-static
        ${EIK_OPENGL_ES_FRAMEWORK}
        ${EIK_AUDIO_TOOLBOX_FRAMEWORK}
        ${EIK_AV_FOUNDATION_FRAMEWORK}
        ${EIK_CORE_AUDIO_FRAMEWORK}
        ${EIK_CORE_VIDEO_FRAMEWORK}
    )
    set(EIK_RAYLIB_TARGET eik_raylib_ios)
else()
    FetchContent_MakeAvailable(raylib)
    set(EIK_RAYLIB_TARGET raylib)
endif()

add_library(flecs STATIC third_party/flecs/flecs.c)
target_compile_definitions(flecs PUBLIC FLECS_CUSTOM_BUILD FLECS_SYSTEM FLECS_PIPELINE FLECS_LOG)
target_compile_definitions(flecs PRIVATE FLECS_OS_API_IMPL)
target_include_directories(flecs SYSTEM PUBLIC third_party/flecs)

add_library(yxml STATIC third_party/yxml/yxml.c)
target_include_directories(yxml SYSTEM PUBLIC third_party/yxml)

add_library(eik_collision STATIC src/collision.c)
target_link_libraries(eik_collision PUBLIC ${EIK_RAYLIB_TARGET})
target_include_directories(eik_collision PUBLIC src)

add_library(eik_level STATIC
    src/tmx.c
    src/mod_level.c
    src/render.c
    src/input.c
    src/touch.c
    src/audio.c
    src/l10n.c
    src/ui.c
    src/anim.c
    src/mod_player.c
    src/mod_enemy.c
    src/mod_items.c
)
target_link_libraries(eik_level PUBLIC eik_collision ${EIK_RAYLIB_TARGET} flecs yxml)
target_include_directories(eik_level PUBLIC src)

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(edgard_in_kimeria MACOSX_BUNDLE
        src/main.c
        src/platform_ios.m
    )
else()
    add_executable(edgard_in_kimeria src/main.c)
endif()
target_link_libraries(edgard_in_kimeria PRIVATE eik_level)
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # SDL supplies the UIKit application entry point, which in turn calls the
    # SDL_main symbol produced by including SDL.h in src/main.c.
    target_link_libraries(edgard_in_kimeria PRIVATE SDL2::SDL2main)
    target_include_directories(edgard_in_kimeria PRIVATE
        ${sdl2_BINARY_DIR}/include/SDL2
        ${sdl2_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>/SDL2
    )
endif()
target_include_directories(edgard_in_kimeria PRIVATE src)
set_target_properties(edgard_in_kimeria PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# The desktop executable resolves assets relative to its own directory. Keep a
# copy beside it for local runs and CTest while the web build preloads the same
# source directory directly (below).
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(edgard_in_kimeria PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.jlogicgames.edgard-in-kimeria"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    )
    set(EIK_BUNDLE_ASSETS)
    file(GLOB_RECURSE EIK_ASSET_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/*
    )
    foreach (asset IN LISTS EIK_ASSET_FILES)
        if (NOT IS_DIRECTORY ${asset})
            file(RELATIVE_PATH asset_relative ${CMAKE_CURRENT_SOURCE_DIR}/assets ${asset})
            get_filename_component(asset_directory ${asset_relative} DIRECTORY)
            set_source_files_properties(${asset} PROPERTIES
                MACOSX_PACKAGE_LOCATION Resources/assets/${asset_directory}
            )
            list(APPEND EIK_BUNDLE_ASSETS ${asset})
        endif()
    endforeach()
    set_source_files_properties(ios/LaunchScreen.storyboard PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(edgard_in_kimeria PRIVATE
        ${EIK_BUNDLE_ASSETS}
        ios/LaunchScreen.storyboard
    )
    target_compile_definitions(edgard_in_kimeria PRIVATE EIK_IOS=1)
    target_compile_definitions(eik_level PUBLIC EIK_IOS=1)
    if (CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
        set_target_properties(edgard_in_kimeria PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        )
        target_compile_definitions(edgard_in_kimeria PRIVATE EIK_IOS_SIMULATOR=1)
    endif()
else()
    # The desktop executable resolves assets relative to its own directory. Keep a
    # copy beside it for local runs and CTest while the web build preloads the same
    # source directory directly (below).
    add_custom_command(TARGET edgard_in_kimeria POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:edgard_in_kimeria>/assets
    )
endif()

if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(edgard_in_kimeria PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(eik_collision PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(eik_level PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
if (NOT EMSCRIPTEN AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(test_collision tests/test_collision.c)
    target_link_libraries(test_collision PRIVATE eik_collision)
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_collision PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME collision COMMAND test_collision)
    add_executable(test_tmx tests/test_tmx.c)
    target_link_libraries(test_tmx PRIVATE eik_level)
    target_compile_options(test_tmx PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME tmx COMMAND test_tmx ${CMAKE_CURRENT_SOURCE_DIR}/assets/tiles
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/offset.tmx)
    add_executable(test_player tests/test_player.c)
    target_link_libraries(test_player PRIVATE eik_level)
    target_compile_options(test_player PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME player COMMAND test_player)
    add_executable(test_enemy tests/test_enemy.c)
    target_link_libraries(test_enemy PRIVATE eik_level)
    target_compile_options(test_enemy PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME enemy COMMAND test_enemy)
    add_executable(test_triggers tests/test_triggers.c)
    target_link_libraries(test_triggers PRIVATE eik_level)
    target_compile_options(test_triggers PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME triggers COMMAND test_triggers)
    add_executable(test_l10n tests/test_l10n.c)
    target_link_libraries(test_l10n PRIVATE eik_level)
    target_compile_options(test_l10n PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME l10n COMMAND test_l10n)
endif()
