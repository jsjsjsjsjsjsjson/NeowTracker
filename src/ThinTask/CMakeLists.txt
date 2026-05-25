set(THINTASK_PORT "pthread" CACHE STRING "Selected ThinTask port: pthread, freertos")
set_property(CACHE THINTASK_PORT PROPERTY STRINGS pthread freertos)

string(TOLOWER "${THINTASK_PORT}" THINTASK_PORT_LOWER)

set(THINTASK_SOURCES
    ThinTask.c
)

if(THINTASK_PORT_LOWER STREQUAL "pthread")
  find_package(Threads REQUIRED)
  list(APPEND THINTASK_SOURCES ThinTaskPort_Pthread.c)
elseif(THINTASK_PORT_LOWER STREQUAL "freertos")
  list(APPEND THINTASK_SOURCES ThinTaskPort_FreeRTOS.c)
else()
  message(FATAL_ERROR "Unknown THINTASK_PORT='${THINTASK_PORT}'. Use one of: pthread, freertos")
endif()

add_library(ThinTask ${THINTASK_SOURCES})
add_library(ThinTask::ThinTask ALIAS ThinTask)
add_library(thintask::thintask ALIAS ThinTask)

target_include_directories(ThinTask
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ThinTask PUBLIC c_std_11)

if(THINTASK_PORT_LOWER STREQUAL "pthread")
  target_compile_definitions(ThinTask PRIVATE TT_PORT_PTHREAD=1)
  target_link_libraries(ThinTask PUBLIC Threads::Threads)
elseif(THINTASK_PORT_LOWER STREQUAL "freertos")
  target_compile_definitions(ThinTask PRIVATE TT_PORT_FREERTOS=1)
endif()

target_compile_options(ThinTask PRIVATE -Wall -Wextra -Wpedantic)
