cmake_minimum_required(VERSION 3.12)

project(arbiter)

option(ARBITER_EXTERNAL_JSON "Use system nlohmann json  as dependency" ON)
option(ARBITER_EXTERNAL_XML "Use system rapidxml as dependency" ON)
option(ARBITER_CURL "Use curl for HTTP requests" ON)
option(ARBITER_OPENSSL "Use OpenSSL for HTTPS requests" ON)
option(ARBITER_ZLIB "Use zlib for compression" ON)

if (ARBITER_CURL)
    find_package(Curl REQUIRED)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    find_package(Threads REQUIRED)
endif()

if (ARBITER_OPENSSL)
    find_package(OpenSSL REQUIRED)
endif()

MESSAGE(${CMAKE_CXX_COMPILER_ID})
if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU OR
        ${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
    add_definitions("-DUNIX")
    add_definitions(${CMAKE_CXX_FLAGS} "-Wno-deprecated-declarations")
    add_definitions(${CMAKE_CXX_FLAGS} "-Wall")
    add_definitions(${CMAKE_CXX_FLAGS} "-pedantic")
    add_definitions(${CMAKE_CXX_FLAGS} "-fexceptions")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    add_definitions("-DWINDOWS")
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
    find_library(SHLWAPI Shlwapi.lib)
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_library(${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

add_subdirectory(${PROJECT_NAME})

set(OBJS
    $<TARGET_OBJECTS:src>
    $<TARGET_OBJECTS:drivers>
    $<TARGET_OBJECTS:util>)
target_sources(${PROJECT_NAME} PRIVATE ${OBJS})

if (ARBITER_CURL)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ARBITER_CURL)
    target_link_libraries(${PROJECT_NAME} PUBLIC CURL::libcurl Threads::Threads)
endif()

if (ARBITER_ZLIB)
    find_package(ZLIB REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC ZLIB::ZLIB)
endif()

if (ARBITER_OPENSSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ARBITER_OPENSSL)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${SHLWAPI})

if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(third/gtest-1.7.0)
    include_directories(src third/gtest-1.7.0/include third/gtest-1.7.0)
    add_subdirectory(test)
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
