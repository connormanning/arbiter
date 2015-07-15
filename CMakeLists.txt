cmake_minimum_required(VERSION 2.8.8)

project(arbiter)

add_definitions(${CMAKE_CXX_FLAGS} "-std=c++11")
add_definitions(${CMAKE_CXX_FLAGS} "-Wno-deprecated-declarations")
add_definitions(${CMAKE_CXX_FLAGS} "-Wall")
add_definitions(${CMAKE_CXX_FLAGS} "-pedantic")
add_definitions(${CMAKE_CXX_FLAGS} "-pthread")
add_definitions(${CMAKE_CXX_FLAGS} "-fexceptions")
add_definitions(${CMAKE_CXX_FLAGS} "-fPIC")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(arbiter)

set(OBJS $<TARGET_OBJECTS:src> $<TARGET_OBJECTS:drivers>)
add_library(arbiter SHARED ${OBJS})

target_link_libraries(arbiter curl crypto pthread)

install(TARGETS arbiter DESTINATION lib)

