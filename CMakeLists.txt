cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_COMPILER clang++)

project(ModPlayerNeow LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# include_directories(${PROJECT_SOURCE_DIR}/include)

set(FAMIGFX_PORT sdl CACHE STRING "" FORCE)

add_subdirectory(src/ThinMix)
add_subdirectory(src/Task)
add_subdirectory(src/FamiGFX)

add_executable(NeowTracker
    src/binio_helper.c
    src/mod_helper.c
    src/LFO.cpp
    src/WAV.cpp
    src/FIRFilter.cpp
    src/mod_file.cpp
    src/main.cpp
)

target_compile_options(NeowTracker PRIVATE -O3)

target_link_libraries(NeowTracker ThinMix famigfx)
