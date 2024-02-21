cmake_minimum_required(VERSION 3.12)
project(arbiter LANGUAGES CXX)

include(GNUInstallDirs)

option(ARBITER_EXTERNAL_JSON "Use system nlohmann json  as dependency" ON)
option(ARBITER_EXTERNAL_XML "Use system rapidxml as dependency" ON)
option(ARBITER_CURL "Use curl for HTTP requests" ON)
option(ARBITER_OPENSSL "Use OpenSSL for HTTPS requests" ON)

if (ARBITER_CURL)
    find_package(Curl REQUIRED)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    find_package(Threads REQUIRED)
endif()

if (ARBITER_OPENSSL)
    find_package(OpenSSL REQUIRED)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(arbiter)

add_library(arbiter)
target_sources(arbiter PRIVATE  $<TARGET_OBJECTS:src> $<TARGET_OBJECTS:drivers> $<TARGET_OBJECTS:util>)
target_compile_features(arbiter PUBLIC cxx_std_11)
target_include_directories(arbiter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU OR
        ${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
    target_compile_definitions(arbiter PUBLIC UNIX)
    target_compile_options(arbiter PRIVATE -Wall -Wno-deprecated-declarations -pedantic -fexceptions)
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    target_compile_definitions(arbiter PUBLIC WINDOWS _CRT_SECURE_NO_WARNINGS)
    find_library(SHLWAPI Shlwapi.lib)
    target_link_libraries(arbiter PUBLIC ${SHLWAPI})
endif()

if (ARBITER_CURL)
    target_compile_definitions(arbiter PUBLIC ARBITER_CURL)
    target_link_libraries(arbiter PUBLIC CURL::libcurl Threads::Threads)
endif()

if (ARBITER_OPENSSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(arbiter PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(arbiter PUBLIC ARBITER_OPENSSL)
endif()

if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(third/gtest-1.7.0)
    include_directories(src third/gtest-1.7.0/include third/gtest-1.7.0)
    add_subdirectory(test)
endif()

install(TARGETS arbiter
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
