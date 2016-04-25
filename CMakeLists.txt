cmake_minimum_required(VERSION 2.8.8)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH} )
project(arbiter)


set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
find_package(Curl REQUIRED)

MESSAGE(${CMAKE_CXX_COMPILER_ID})
if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU OR
        ${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
    add_definitions("-DUNIX")
    add_definitions(${CMAKE_CXX_FLAGS} "-std=c++11")
    add_definitions(${CMAKE_CXX_FLAGS} "-Wno-deprecated-declarations")
    add_definitions(${CMAKE_CXX_FLAGS} "-Wall")
    add_definitions(${CMAKE_CXX_FLAGS} "-pedantic")
    add_definitions(${CMAKE_CXX_FLAGS} "-pthread")
    add_definitions(${CMAKE_CXX_FLAGS} "-fexceptions")
    add_definitions(${CMAKE_CXX_FLAGS} "-fPIC")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    add_definitions("-DWINDOWS")
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CURL_INCLUDE_DIR}")

add_subdirectory(arbiter)

set(OBJS
    $<TARGET_OBJECTS:src>
    $<TARGET_OBJECTS:drivers>
    $<TARGET_OBJECTS:third>
    $<TARGET_OBJECTS:util>)

add_library(arbiter SHARED ${OBJS})
target_link_libraries(arbiter ${CURL_LIBRARIES})

install(TARGETS arbiter DESTINATION lib)

