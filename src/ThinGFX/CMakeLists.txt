cmake_minimum_required(VERSION 3.16)
project(ThinGFX VERSION 0.1.0 LANGUAGES CXX)

option(THINGFX_BUILD_EXAMPLES "Build ThinGFX examples" ON)
set(THINGFX_PORT "none" CACHE STRING "Selected ThinGFX port: none, sdl, esp32_ssd1306")
set_property(CACHE THINGFX_PORT PROPERTY STRINGS none sdl esp32_ssd1306)

string(TOLOWER "${THINGFX_PORT}" THINGFX_PORT_LOWER)

set(THINGFX_SOURCES
    src/ThinGFXCore.cpp
    src/ThinGFX.cpp
    src/ThinGFXFonts.cpp
)

if(THINGFX_PORT_LOWER STREQUAL "sdl")
    find_package(SDL2 REQUIRED)
    find_package(Threads REQUIRED)
    list(APPEND THINGFX_SOURCES ports/sdl/ThinGFXPortDefault.cpp)
elseif(THINGFX_PORT_LOWER STREQUAL "esp32_ssd1306")
    list(APPEND THINGFX_SOURCES ports/esp32/ThinGFXPortDefault.cpp)
elseif(NOT THINGFX_PORT_LOWER STREQUAL "none")
    message(FATAL_ERROR "Unknown THINGFX_PORT='${THINGFX_PORT}'. Use one of: none, sdl, esp32_ssd1306")
endif()

add_library(ThinGFX ${THINGFX_SOURCES})

add_library(ThinGFX::ThinGFX ALIAS ThinGFX)
add_library(thingfx::thingfx ALIAS ThinGFX)
add_library(thingfx_cpp ALIAS ThinGFX)

target_include_directories(ThinGFX
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ThinGFX PUBLIC cxx_std_11)

if(THINGFX_PORT_LOWER STREQUAL "sdl")
    target_compile_definitions(ThinGFX PRIVATE THINGFX_HAS_DEFAULT_PORT=1)
    target_link_libraries(ThinGFX PUBLIC SDL2::SDL2 Threads::Threads)
elseif(THINGFX_PORT_LOWER STREQUAL "esp32_ssd1306")
    target_compile_definitions(ThinGFX PRIVATE THINGFX_HAS_DEFAULT_PORT=1)
endif()

if(MSVC)
    target_compile_options(ThinGFX PRIVATE /W4)
else()
    target_compile_options(ThinGFX PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(THINGFX_BUILD_EXAMPLES)
    add_executable(thingfx_native_demo examples/native_demo.cpp)
    target_link_libraries(thingfx_native_demo PRIVATE ThinGFX)

    add_executable(thingfx_mono_demo examples/mono_demo.cpp)
    target_link_libraries(thingfx_mono_demo PRIVATE ThinGFX)

    if(THINGFX_PORT_LOWER STREQUAL "sdl")
        add_executable(thingfx_sdl_demo examples/sdl_demo.cpp)
        target_link_libraries(thingfx_sdl_demo PRIVATE ThinGFX)

        add_executable(thingfx_sdl_mono_demo examples/sdl_mono_demo.cpp)
        target_link_libraries(thingfx_sdl_mono_demo PRIVATE ThinGFX)
    endif()
endif()
