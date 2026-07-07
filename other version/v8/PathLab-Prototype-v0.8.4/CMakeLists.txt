cmake_minimum_required(VERSION 3.16)

project(PathLabSample VERSION 0.8.4 LANGUAGES CXX)

# CMake 4.x removes compatibility with policy versions older than 3.5.
# raylib 5.0 still declares an older minimum CMake version in its own
# CMakeLists.txt, so Visual Studio with CMake 4.x may fail while configuring
# the fetched dependency. This cache variable tells CMake to configure such
# third-party dependencies with a safe minimum policy version.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version for third-party dependencies" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_SYSTEM_RAYLIB "Use a raylib package already installed on the system" OFF)

if(USE_SYSTEM_RAYLIB)
    find_package(raylib CONFIG REQUIRED)
else()
    include(FetchContent)
    set(FETCHCONTENT_QUIET FALSE)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(PathLabSample
    src/main.cpp
    src/App.cpp
    src/GridMap.cpp
    src/Pathfinder.cpp
)

target_include_directories(PathLabSample PRIVATE src)
target_link_libraries(PathLabSample PRIVATE raylib)

if(MSVC)
    target_compile_options(PathLabSample PRIVATE /W4 /permissive-)
else()
    target_compile_options(PathLabSample PRIVATE -Wall -Wextra -pedantic)
endif()

# Copy non-code assets beside the executable. Drop a font at assets/fonts/ui.ttf
# or assets/fonts/ui.otf to replace raylib's built-in pixel font.
add_custom_command(TARGET PathLabSample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:PathLabSample>/assets
)
add_custom_command(TARGET PathLabSample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:PathLabSample>/maps
)
add_custom_command(TARGET PathLabSample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:PathLabSample>/exports
)
