cmake_minimum_required(VERSION 3.20)
project(fractal_xplorer VERSION 1.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2 - install via: pacman -S mingw-w64-x86_64-SDL2
find_package(SDL2 REQUIRED)

# libpng - install via: pacman -S mingw-w64-x86_64-libpng
find_package(PNG REQUIRED)

# SLEEF - install via: pacman -S mingw-w64-x86_64-sleef
find_package(PkgConfig REQUIRED)
pkg_check_modules(SLEEF REQUIRED sleef)

# libjxl (optional) - install via: pacman -S mingw-w64-x86_64-libjxl
pkg_check_modules(LIBJXL libjxl)

# Dear ImGui - fetched automatically at configure time
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(fractal_xplorer
    src/main.cpp
    src/ui_panels.cpp
    src/cpu_renderer.cpp
    src/cpu_renderer_avx.cpp
    src/palette.cpp
    src/export.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(fractal_xplorer PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${SLEEF_INCLUDE_DIRS}
)

target_link_libraries(fractal_xplorer PRIVATE
    SDL2::SDL2main
    SDL2::SDL2
    opengl32
    PNG::PNG
    ${SLEEF_LINK_LIBRARIES}
)

if(LIBJXL_FOUND)
    message(STATUS "JPEG XL support enabled (libjxl ${LIBJXL_VERSION})")
    target_compile_definitions(fractal_xplorer PRIVATE HAVE_JXL)
    target_include_directories(fractal_xplorer PRIVATE ${LIBJXL_INCLUDE_DIRS})
    target_link_libraries(fractal_xplorer PRIVATE ${LIBJXL_LINK_LIBRARIES})
else()
    message(STATUS "JPEG XL support disabled (libjxl not found)")
endif()

# General optimisation for all sources
target_compile_options(fractal_xplorer PRIVATE -O2)

# AVX2 + FMA only for the file that actually uses intrinsics
set_source_files_properties(src/cpu_renderer_avx.cpp PROPERTIES
    COMPILE_OPTIONS "-O2;-mavx2;-mfma"
)

# Hide console window on Windows (SDL2main bridges WinMain -> main)
if (WIN32)
    target_link_options(fractal_xplorer PRIVATE -mwindows)
endif()
