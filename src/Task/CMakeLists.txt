set(NEOW_TASK_PORT "pthread" CACHE STRING "Selected NeowTask port: pthread, freertos")
set_property(CACHE NEOW_TASK_PORT PROPERTY STRINGS pthread freertos)

string(TOLOWER "${NEOW_TASK_PORT}" NEOW_TASK_PORT_LOWER)

set(NEOW_TASK_SOURCES
    Task.c
)

if(NEOW_TASK_PORT_LOWER STREQUAL "pthread")
  find_package(Threads REQUIRED)
  list(APPEND NEOW_TASK_SOURCES TaskPort_Pthread.c)
elseif(NEOW_TASK_PORT_LOWER STREQUAL "freertos")
  list(APPEND NEOW_TASK_SOURCES TaskPort_FreeRTOS.c)
else()
  message(FATAL_ERROR "Unknown NEOW_TASK_PORT='${NEOW_TASK_PORT}'. Use one of: pthread, freertos")
endif()

add_library(NeowTask ${NEOW_TASK_SOURCES})
add_library(neow::task ALIAS NeowTask)

target_include_directories(NeowTask
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(NeowTask PUBLIC c_std_11)

if(NEOW_TASK_PORT_LOWER STREQUAL "pthread")
  target_compile_definitions(NeowTask PRIVATE NT_TASK_PORT_PTHREAD=1)
  target_link_libraries(NeowTask PUBLIC Threads::Threads)
elseif(NEOW_TASK_PORT_LOWER STREQUAL "freertos")
  target_compile_definitions(NeowTask PRIVATE NT_TASK_PORT_FREERTOS=1)
endif()

target_compile_options(NeowTask PRIVATE -Wall -Wextra -Wpedantic)
