cmake_minimum_required(VERSION 3.13)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH} )
project(arbiter)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(CURL)
if (CURL_FOUND)
    message("Found curl")
    find_package(Threads REQUIRED)
    include_directories(${CURL_INCLUDE_DIRS})
    set(ARBITER_CURL TRUE)
    add_definitions("-DARBITER_CURL")
else()
    message("Curl NOT found")
endif()

find_package(OpenSSL 1.0)
if (OPENSSL_FOUND)
    message("Found OpenSSL ${OPENSSL_VERSION}")
    include_directories(${OPENSSL_INCLUDE_DIR})
    set(ARBITER_OPENSSL TRUE)
    add_definitions("-DARBITER_OPENSSL")
else()
    message("OpenSSL NOT found")
endif()

MESSAGE(${CMAKE_CXX_COMPILER_ID})
if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU OR
        ${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
    add_definitions("-DUNIX")
    add_definitions(${CMAKE_CXX_FLAGS} "-Wno-deprecated-declarations")
    add_definitions(${CMAKE_CXX_FLAGS} "-Wall")
    add_definitions(${CMAKE_CXX_FLAGS} "-pedantic")
    add_definitions(${CMAKE_CXX_FLAGS} "-pthread")
    add_definitions(${CMAKE_CXX_FLAGS} "-fexceptions")
    add_definitions(${CMAKE_CXX_FLAGS} "-fPIC")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    add_definitions("-DWINDOWS")
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
    find_library(SHLWAPI Shlwapi.lib)
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")



add_subdirectory(arbiter)

set(OBJS
    $<TARGET_OBJECTS:src>
    $<TARGET_OBJECTS:drivers>
    $<TARGET_OBJECTS:util>)

add_library(arbiter SHARED ${OBJS})

if (${ARBITER_ZLIB})
    target_link_libraries(arbiter PUBLIC ${ZLIB_LIBRARIES})
    target_include_directories(arbiter PRIVATE "${ZLIB_INCLUDE_DIR}")
endif()

if (${ARBITER_CURL})
	target_link_libraries(arbiter PUBLIC ${CURL_LIBRARIES})
    target_include_directories(arbiter PRIVATE "${CURL_INCLUDE_DIR}")
endif()

if (${ARBITER_OPENSSL})
    target_link_libraries(arbiter PUBLIC ${OPENSSL_LIBRARIES})
    target_include_directories(arbiter PRIVATE "${OPENSSL_INCLUDE_DIR}")
endif()

target_link_libraries(arbiter PUBLIC ${SHLWAPI})

include (${CMAKE_SOURCE_DIR}/cmake/gtest.cmake)

add_subdirectory(test)

install(TARGETS arbiter DESTINATION lib)

