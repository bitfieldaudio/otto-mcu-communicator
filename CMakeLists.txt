cmake_minimum_required(VERSION 3.14)
project(OTTO_MCU_SERVICE LANGUAGES CXX C)

set(CMAKE_CXX_EXTENSIONS OFF)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0" CACHE STRING "" FORCE)

include(cmake/utils.cmake)

otto_option(ENABLE_ASAN "Enable the adress sanitizer on development builds" OFF)
otto_option(ENABLE_UBSAN "Enable the undefined behaviour sanitizer on development builds" OFF)
otto_option(ENABLE_LTO "Enable link time optimization on release builds. Only works on clang" OFF)

# Specific for RPi ARM platform
set(CMAKE_LINKER_FLAGS_RELEASE "${CMAKE_LINKER_FLAGS_RELEASE} -ffast-math -funsafe-math-optimizations -mfpu=neon-vfpv4")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math -funsafe-math-optimizations -mfpu=neon-vfpv4")

include(cmake/CPM.cmake)
set(CPM_USE_LOCAL_PACKAGES OFF)
set(CPM_LOCAL_PACKAGES_ONLY OFF)

if (OTTO_ENABLE_ASAN) 
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fno-sanitize=vptr")
  set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fno-sanitize=vptr")
endif()

if (OTTO_ENABLE_UBSAN)
  set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined")
  set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=undefined")
endif()

if (DEFINED CMAKE_CXX_FLAGS_DEBUG_INIT AND  
    "${CMAKE_CXX_FLAGS_DEBUG_INIT}" STREQUAL "${CMAKE_CXX_FLAGS_DEBUG}")
    # Overwrite the init values choosen by CMake
    if (CMAKE_COMPILER_IS_GNUCXX)
    endif()
endif()

option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts-diagnostics-depth=1000")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.2)
        message(FATAL_ERROR "GCC version must be at least 10.2!")
    endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
        message(FATAL_ERROR "Clang version must be at least 10.0!")
    endif()
else()
    message(WARNING "You are using an unsupported compiler! Compilation has only been tested with Clang and GCC.")
endif()

set(OTTO_MCU_SERVICE_EXTERNAL_DIR ${OTTO_MCU_SERVICE_SOURCE_DIR}/external/)


# Enable warnings for local code
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(OTTO_CXX_FLAGS "-Wall -Wno-long-long \
-Wno-c++14-compat -Wno-psabi \
-Wno-unknown-pragmas \
-Wno-vla \
-Wno-sign-compare \
-Wno-error=pedantic \
-Werror=unused-result \
")
# Don't remove unused-result, use [[maybe_unused]] to circumvent necessary
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(OTTO_CXX_FLAGS "-Wall -Wno-long-long \
-Wno-c++11-compat -Wno-c++98-compat -Wno-c++14-compat -Wno-c++17-compat -Wc++20-compat \
-Wno-shadow-field-in-constructor \
-Wno-documentation-unknown-command \
-Wno-unknown-pragmas \
-Wno-missing-braces \
-Wno-vla-extension \
-Wno-c99-extensions \
-Wno-gnu-zero-variadic-macro-arguments \
-Wno-gnu-anonymous-struct \
-Wno-nested-anon-types \
-Werror=unused-result \
")
# Don't remove unused-result, use [[maybe_unused]] to circumvent when necessary
endif()

add_subdirectory(src)

otto_debug_definitions()

if (OTTO_ENABLE_LTO) 
  include(CheckIPOSupported)
  check_ipo_supported(RESULT supported OUTPUT error)

  if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET otto_exec PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET otto_test PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(STATUS "IPO / LTO not supported: <${error}>")
  endif()
endif()
