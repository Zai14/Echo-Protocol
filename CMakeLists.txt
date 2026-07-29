cmake_minimum_required(VERSION 3.16)
project(EchoProtocol C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Build type default
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# -----------------------------------------------------------------------------
# Size optimisation — target executable < 1.44 MB (floppy-disk friendly)
# -----------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os -flto -s -ffunction-sections -fdata-sections")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s -flto")
elseif(MSVC)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O1 /GL /Gy /Gw")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
endif()

# -----------------------------------------------------------------------------
# Raylib dependency
#
# Always fetch raylib 5.5 from GitHub to ensure the API functions we use
# (GenWaveSine, GenWaveSquare, SetSoundLooping, DrawRing, etc.) are
# available.  Skipping find_package avoids picking up an older raylib 4.x
# that may be installed via MSYS2, vcpkg, or a system package manager.
# -----------------------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES   OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(raylib)

# -----------------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------------
set(ECHO_SOURCES
    src/main.c
    src/game.c
    src/player.c
    src/sonar.c
    src/renderer.c
    src/enemy.c
    src/map.c
    src/collision.c
    src/procedural.c
    src/echomemory.c
    src/soundprop.c
    src/audio.c
    src/achievements.c
)

add_executable(EchoProtocol ${ECHO_SOURCES})

target_include_directories(EchoProtocol PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(EchoProtocol PRIVATE raylib)

target_compile_definitions(EchoProtocol PRIVATE
    $<$<PLATFORM_ID:Windows>:PLATFORM_WINDOWS>
)

if(MSVC)
    target_compile_options(EchoProtocol PRIVATE /W4)
else()
    target_compile_options(EchoProtocol PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Windows needs these system libs when linking raylib statically.
if(WIN32)
    target_link_libraries(EchoProtocol PRIVATE winmm gdi32 opengl32)
    # Build a windowed (non-console) app in Release configs.
    if(MSVC)
        set_target_properties(EchoProtocol PROPERTIES
            WIN32_EXECUTABLE $<CONFIG:Release>
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# Copy assets next to the built executable so runtime asset paths resolve.
# -----------------------------------------------------------------------------
add_custom_command(TARGET EchoProtocol POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:EchoProtocol>/assets
    COMMENT "Copying assets to output directory"
)

set_target_properties(EchoProtocol PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
