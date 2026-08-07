cmake_minimum_required(VERSION 3.16)
project(mqme LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MQME_BUILD_SHARED "Build mqme as a shared library" ON)
option(MQME_BUILD_STATIC "Build mqme as a static library" OFF)
option(MQME_BUILD_TESTS  "Build mqme integration tests" OFF)

if(NOT MQME_BUILD_SHARED AND NOT MQME_BUILD_STATIC)
  message(FATAL_ERROR
    "At least one of MQME_BUILD_SHARED or MQME_BUILD_STATIC must be enabled")
endif()

#
# Output naming:
#
#   mqme64.dll
#   mqme64d.dll
#   mqme64.lib
#   mqme64d.lib
#   mqme64s.lib
#   mqme64sd.lib
#
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(MQME_ARCHITECTURE "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(MQME_ARCHITECTURE "32")
else()
  message(FATAL_ERROR
    "Unsupported pointer size: ${CMAKE_SIZEOF_VOID_P}")
endif()

#
# Equivalent to:
#
#   $(SolutionDir)bin
#   $(SolutionDir)lib
#
# CMAKE_SOURCE_DIR is the root source directory passed to CMake, even when
# this project adds Dependencies/Pool as a subdirectory.
#
set(MQME_BIN_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(MQME_LIB_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")

#
# Pool dependency
#
# Request the variants needed by the enabled mqme targets before adding
# Pool's subdirectory.
#
if(MQME_BUILD_SHARED)
  set(POOL_BUILD_SHARED ON CACHE BOOL
    "Build Pool as a shared library" FORCE)
endif()

if(MQME_BUILD_STATIC)
  set(POOL_BUILD_STATIC ON CACHE BOOL
    "Build Pool as a static library" FORCE)
endif()

if(NOT TARGET Pool AND NOT TARGET Pool_static)
  add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Pool"
    "${CMAKE_BINARY_DIR}/Pool"
  )
endif()

if(MQME_BUILD_SHARED AND NOT TARGET Pool)
  message(FATAL_ERROR
    "MQME_BUILD_SHARED requires the Pool shared-library target named Pool")
endif()

if(MQME_BUILD_STATIC AND NOT TARGET Pool_static)
  message(FATAL_ERROR
    "MQME_BUILD_STATIC requires the Pool static-library target named Pool_static")
endif()

set(MQME_SOURCES
  Source/mqme.cpp
  Source/Socket.cpp
  Source/Packet.cpp
  Source/PacketQueue.cpp
  Source/Client.cpp
  Source/Server.cpp
)

#
# Apply settings shared by the dynamic and static mqme targets.
#
function(configure_mqme_target target pool_target)
  target_include_directories(${target}
    PUBLIC
      "${CMAKE_CURRENT_SOURCE_DIR}/Include"

    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/Source"
  )

  target_compile_features(${target}
    PUBLIC
      cxx_std_20
  )

  target_link_libraries(${target}
    PRIVATE
      ${pool_target}
  )

  if(WIN32)
    target_link_libraries(${target}
      PRIVATE
        ws2_32
        ole32
    )
  else()
    target_link_libraries(${target}
      PRIVATE
        uuid
        pthread
    )
  endif()

  if(MSVC)
    target_compile_options(${target}
      PRIVATE
        /W4
        /permissive-
    )
  else()
    target_compile_options(${target}
      PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
  endif()

  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
      "${MQME_BIN_DIRECTORY}"

    LIBRARY_OUTPUT_DIRECTORY
      "${MQME_BIN_DIRECTORY}"

    ARCHIVE_OUTPUT_DIRECTORY
      "${MQME_LIB_DIRECTORY}"

    DEBUG_POSTFIX
      "d"
  )
endfunction()

#
# Shared mqme library
#
# Windows:
#
#   bin/mqme64.dll
#   bin/mqme64d.dll
#   lib/mqme64.lib
#   lib/mqme64d.lib
#
if(MQME_BUILD_SHARED)
  add_library(mqme SHARED
    ${MQME_SOURCES}
  )

  set_target_properties(mqme PROPERTIES
    OUTPUT_NAME
      "mqme${MQME_ARCHITECTURE}"
  )

  target_compile_definitions(mqme
    PRIVATE
      MQME_EXPORTS
  )

  configure_mqme_target(mqme Pool)
endif()

#
# Static mqme library
#
# Windows:
#
#   lib/mqme64s.lib
#   lib/mqme64sd.lib
#
if(MQME_BUILD_STATIC)
  add_library(mqme_static STATIC
    ${MQME_SOURCES}
  )

  set_target_properties(mqme_static PROPERTIES
    OUTPUT_NAME
      "mqme${MQME_ARCHITECTURE}s"
  )

  target_compile_definitions(mqme_static
    PUBLIC
      MQME_STATIC
  )

  configure_mqme_target(mqme_static Pool_static)
endif()

#
# Integration test
#
if(MQME_BUILD_TESTS)
  enable_testing()

  add_executable(mqme_integration_test
    Tests/Integration.cpp
  )

  set_target_properties(mqme_integration_test PROPERTIES
    OUTPUT_NAME
      "mqme_integration_test${MQME_ARCHITECTURE}"

    DEBUG_POSTFIX
      "d"

    RUNTIME_OUTPUT_DIRECTORY
      "${MQME_BIN_DIRECTORY}"
  )

  #
  # Prefer testing the shared build when present. Otherwise, use static.
  #
  if(TARGET mqme)
    target_link_libraries(mqme_integration_test
      PRIVATE
        mqme
    )
  elseif(TARGET mqme_static)
    target_link_libraries(mqme_integration_test
      PRIVATE
        mqme_static
    )
  else()
    message(FATAL_ERROR
      "MQME_BUILD_TESTS requires an mqme library target")
  endif()

  add_test(
    NAME mqme_integration
    COMMAND mqme_integration_test
  )
endif()