cmake_minimum_required(VERSION 3.16)
project(FamiGFX VERSION 0.1.0 LANGUAGES CXX)

option(FAMIGFX_BUILD_EXAMPLES "Build FamiGFX examples" ON)
set(FAMIGFX_PORT "none" CACHE STRING "Selected FamiGFX port: none, sdl, esp32_ssd1306")
set_property(CACHE FAMIGFX_PORT PROPERTY STRINGS none sdl esp32_ssd1306)

string(TOLOWER "${FAMIGFX_PORT}" FAMIGFX_PORT_LOWER)

set(FAMIGFX_SOURCES
    src/FamiGFXCore.cpp
    src/FamiGFX.cpp
    src/FamiGFXFonts.cpp
)

if(FAMIGFX_PORT_LOWER STREQUAL "sdl")
    find_package(SDL2 REQUIRED)
    find_package(Threads REQUIRED)
    list(APPEND FAMIGFX_SOURCES ports/sdl/FamiGFXPortDefault.cpp)
elseif(FAMIGFX_PORT_LOWER STREQUAL "esp32_ssd1306")
    list(APPEND FAMIGFX_SOURCES ports/esp32/FamiGFXPortDefault.cpp)
elseif(NOT FAMIGFX_PORT_LOWER STREQUAL "none")
    message(FATAL_ERROR "Unknown FAMIGFX_PORT='${FAMIGFX_PORT}'. Use one of: none, sdl, esp32_ssd1306")
endif()

add_library(famigfx ${FAMIGFX_SOURCES})

add_library(famigfx::famigfx ALIAS famigfx)
add_library(famigfx_cpp ALIAS famigfx)

target_include_directories(famigfx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(famigfx PUBLIC cxx_std_11)

if(FAMIGFX_PORT_LOWER STREQUAL "sdl")
    target_compile_definitions(famigfx PRIVATE FAMIGFX_HAS_DEFAULT_PORT=1)
    target_link_libraries(famigfx PUBLIC SDL2::SDL2 Threads::Threads)
elseif(FAMIGFX_PORT_LOWER STREQUAL "esp32_ssd1306")
    target_compile_definitions(famigfx PRIVATE FAMIGFX_HAS_DEFAULT_PORT=1)
endif()

if(MSVC)
    target_compile_options(famigfx PRIVATE /W4)
else()
    target_compile_options(famigfx PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()

if(FAMIGFX_BUILD_EXAMPLES)
    add_executable(famigfx_native_demo examples/native_demo.cpp)
    target_link_libraries(famigfx_native_demo PRIVATE famigfx)

    add_executable(famigfx_mono_demo examples/mono_demo.cpp)
    target_link_libraries(famigfx_mono_demo PRIVATE famigfx)

    if(FAMIGFX_PORT_LOWER STREQUAL "sdl")
        add_executable(famigfx_sdl_demo examples/sdl_demo.cpp)
        target_link_libraries(famigfx_sdl_demo PRIVATE famigfx)

        add_executable(famigfx_sdl_mono_demo examples/sdl_mono_demo.cpp)
        target_link_libraries(famigfx_sdl_mono_demo PRIVATE famigfx)
    endif()
endif()
