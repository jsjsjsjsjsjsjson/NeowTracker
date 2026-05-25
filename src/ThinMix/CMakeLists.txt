set(THINMIX_PORT "portaudio" CACHE STRING "Selected ThinMix port: portaudio, null, esp32s3_i2s")
set_property(CACHE THINMIX_PORT PROPERTY STRINGS portaudio null esp32s3_i2s)

option(THINMIX_BUILD_EXAMPLES "Build ThinMix examples and tests" OFF)

string(TOLOWER "${THINMIX_PORT}" THINMIX_PORT_LOWER)

set(THINMIX_SOURCES
    ThinMix.cpp
)

if(THINMIX_PORT_LOWER STREQUAL "portaudio")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)
  list(APPEND THINMIX_SOURCES ThinMixPort_PortAudio.cpp)
elseif(THINMIX_PORT_LOWER STREQUAL "null")
  list(APPEND THINMIX_SOURCES ThinMixPort_Null.cpp)
elseif(THINMIX_PORT_LOWER STREQUAL "esp32s3_i2s")
  list(APPEND THINMIX_SOURCES ThinMixPort_ESP32S3_I2S.cpp)
else()
  message(FATAL_ERROR "Unknown THINMIX_PORT='${THINMIX_PORT}'. Use one of: portaudio, null, esp32s3_i2s")
endif()

add_library(ThinMix ${THINMIX_SOURCES})
add_library(thinmix::thinmix ALIAS ThinMix)

target_include_directories(ThinMix
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ThinMix PUBLIC cxx_std_11)

target_compile_options(ThinMix PRIVATE -O3 -ffast-math)

if(THINMIX_PORT_LOWER STREQUAL "portaudio")
  target_compile_definitions(ThinMix PRIVATE THINMIX_PORT_PORTAUDIO=1)
  target_link_libraries(ThinMix PUBLIC PkgConfig::PORTAUDIO)
elseif(THINMIX_PORT_LOWER STREQUAL "null")
  target_compile_definitions(ThinMix PRIVATE THINMIX_PORT_NULL=1)
elseif(THINMIX_PORT_LOWER STREQUAL "esp32s3_i2s")
  target_compile_definitions(ThinMix PRIVATE THINMIX_PORT_ESP32S3_I2S=1)
endif()

if(THINMIX_BUILD_EXAMPLES)
  add_executable(thinmix_sine_example thinmix_sine_example.cpp)
  target_link_libraries(thinmix_sine_example PRIVATE ThinMix)

  add_executable(thinmix_pcm_convert_test thinmix_pcm_convert_test.cpp)
  target_link_libraries(thinmix_pcm_convert_test PRIVATE ThinMix)
endif()
