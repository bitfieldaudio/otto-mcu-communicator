set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE headers "${OTTO_MCU_SERVICE_SOURCE_DIR}/src/*.hpp")
file(GLOB_RECURSE sources "${OTTO_MCU_SERVICE_SOURCE_DIR}/src/*.cpp")

find_package (Threads)
# Library
add_library(otto_mcu_service_src OBJECT ${sources})
target_include_directories(otto_mcu_service_src PUBLIC ${OTTO_MCU_SERVICE_SOURCE_DIR}/src)
target_link_libraries(otto_mcu_service_src PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

# Executable
add_executable(otto_mcu_service_exec ${OTTO_MCU_SERVICE_SOURCE_DIR}/src/main.cpp)
target_link_libraries(otto_mcu_service_exec PUBLIC otto_mcu_service_src)
set_target_properties(otto_mcu_service_exec PROPERTIES OUTPUT_NAME mcu_service)

add_subdirectory(${OTTO_MCU_SERVICE_EXTERNAL_DIR} ${OTTO_MCU_SERVICE_BINARY_DIR}/external/)

# This updates configurations
otto_add_definitions(otto_mcu_service_src)
otto_add_definitions(otto_mcu_service_exec)

target_link_libraries(otto_mcu_service_src PUBLIC external)
target_link_libraries(otto_mcu_service_src PUBLIC fmt)
target_link_libraries(otto_mcu_service_src PUBLIC nlohmann_json)
target_link_libraries(otto_mcu_service_src PUBLIC loguru)
target_link_libraries(otto_mcu_service_src PUBLIC nanobench)
target_link_libraries(otto_mcu_service_src PUBLIC magic_enum)
target_link_libraries(otto_mcu_service_src PUBLIC concurrentqueue)
target_link_libraries(otto_mcu_service_src PUBLIC function2)
#target_link_libraries(otto_mcu_service_src PUBLIC toml11)
target_link_libraries(otto_mcu_service_src PUBLIC otto_boost)
target_link_libraries(otto_mcu_service_src PUBLIC yamc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OTTO_CXX_FLAGS}")