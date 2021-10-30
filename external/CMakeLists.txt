set(CMAKE_CXX_STANDARD 20)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic -Wno-unused-function")
elseif(${CMAKE_CXX_COMPILER_ID} EQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic -Wno-unused-function")
endif()

# General purpose externals
file(GLOB_RECURSE external_src
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
add_library(external ${external_src})
target_include_directories(external SYSTEM PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Download External Dependencies via CPM
CPMAddPackage("gh:fmtlib/fmt#7.1.3")
CPMAddPackage("gh:catchorg/Catch2@3.0.0-preview3")
CPMAddPackage("gh:Neargye/magic_enum@0.7.2")
CPMAddPackage("gh:Naios/function2#4.1.0")

CPMAddPackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  VERSION 3.9.1
  DOWNLOAD_ONLY YES
  GIT_SHALLOW ON
)
if (nlohmann_json_ADDED)
  add_library(nlohmann_json INTERFACE)
  target_include_directories(nlohmann_json SYSTEM INTERFACE "${nlohmann_json_SOURCE_DIR}/include")
endif()

CPMAddPackage(
  NAME loguru
  GITHUB_REPOSITORY emilk/loguru 
  VERSION 2.1.0
  DOWNLOAD_ONLY YES
  GIT_SHALLOW ON
) 

if (loguru_ADDED)
  add_library(loguru "${loguru_SOURCE_DIR}/loguru.cpp")
  target_compile_definitions(loguru PUBLIC "-DLOGURU_USE_FMTLIB=1")
  target_link_libraries(loguru PUBLIC fmt)
  target_link_libraries(loguru PUBLIC execinfo)
  target_include_directories(loguru SYSTEM PUBLIC "${loguru_SOURCE_DIR}")
endif()

CPMAddPackage(
  NAME nanobench
  VERSION 4.0.0
  GITHUB_REPOSITORY martinus/nanobench
  DOWNLOAD_ONLY YES
)

if (nanobench_ADDED) 
  add_library(nanobench INTERFACE)
  target_include_directories(nanobench SYSTEM INTERFACE "${nanobench_SOURCE_DIR}/src/include")
endif()


CPMAddPackage(
  NAME concurrentqueue
  VERSION 1.0.1
  GITHUB_REPOSITORY cameron314/concurrentqueue
  DOWNLOAD_ONLY YES
)

if (concurrentqueue_ADDED) 
  add_library(concurrentqueue INTERFACE)
  target_include_directories(concurrentqueue SYSTEM INTERFACE "${concurrentqueue_SOURCE_DIR}")
endif()

CPMAddPackage(
  NAME yamc
  GITHUB_REPOSITORY yohhoy/yamc
  GIT_TAG fc378ad9c4169a876e769b91c7d2bddf669d1a91
  DOWNLOAD_ONLY YES
)

if (yamc_ADDED)
  add_library(yamc INTERFACE)
  target_include_directories(yamc SYSTEM INTERFACE "${yamc_SOURCE_DIR}/include")
endif()

# BOOST

find_package(
  Boost
  1.74.0
)

add_library(otto_boost INTERFACE)

if(Boost_FOUND)
  target_link_libraries(otto_boost INTERFACE Boost::headers)
else()
  # boost-cmake is not updated, so manually set the url to a newer version
  # Also use a fork which has an updated list of sources
  CPMAddPackage(
    NAME boost-cmake
    GITHUB_REPOSITORY Element-0/boost-cmake
    GIT_TAG 7199b5408646f8067a00f7d9e83afffb19dde3e8
    OPTIONS
      "BOOST_URL https://boostorg.jfrog.io/artifactory/main/release/1.74.0/source/boost_1_74_0.tar.bz2"
      "BOOST_URL_SHA256 83bfc1507731a0906e387fc28b7ef5417d591429e51e788417fe9ff025e116b1"
      "BOOST_DISABLE_TESTS ON"
  )
  target_link_libraries(otto_boost INTERFACE Boost::boost)
endif()
