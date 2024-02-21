cmake_minimum_required(VERSION 3.13)
project(arbiter LANGUAGES CXX)

include(GNUInstallDirs)

option(ARBITER_EXTERNAL_JSON "Use system nlohmann json  as dependency" ON)
option(ARBITER_EXTERNAL_XML "Use system rapidxml as dependency" ON)
option(ARBITER_CURL "Use curl for HTTP requests" ON)
option(ARBITER_OPENSSL "Use OpenSSL for HTTPS requests" ON)

if (ARBITER_CURL)
    find_package(CURL REQUIRED)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    find_package(Threads REQUIRED)
endif()

if (ARBITER_OPENSSL)
    find_package(OpenSSL REQUIRED)
endif()

file(GLOB_RECURSE SOURCES arbiter/*.cpp arbiter/drivers/*.cpp arbiters/util/*.cpp)
file(GLOB_RECURSE HEADERS arbiter/*.hpp arbiter/drivers/*.hpp arbiters/util/*.hpp)

add_library(arbiter ${SOURCES} ${HEADERS})
target_compile_features(arbiter PUBLIC cxx_std_11)
target_include_directories(arbiter PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if (ARBITER_CURL)
    target_compile_definitions(arbiter PUBLIC ARBITER_CURL)
    target_link_libraries(arbiter PUBLIC CURL::libcurl Threads::Threads)
endif()

if (ARBITER_OPENSSL)
    target_link_libraries(arbiter PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(arbiter PUBLIC ARBITER_OPENSSL)
endif()

if (WIN32 AND BUILD_SHARED_LIBS)
    set_target_properties(arbiter
        PROPERTIES
            COMPILE_DEFINITIONS ARBITER_DLL_EXPORT)
endif()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU OR
        ${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
    target_compile_definitions(arbiter PUBLIC UNIX)
    target_compile_options(arbiter PUBLIC -Wall -Wno-deprecated-declarations -pedantic -fexceptions)
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    target_compile_definitions(arbiter PUBLIC WINDOWS _CRT_SECURE_NO_WARNINGS)
    find_library(SHLWAPI Shlwapi.lib)
    target_link_libraries(arbiter PUBLIC ${SHLWAPI})
endif()

if (BUILD_TESTING)
    enable_testing()
    option(ARBITER_EXTERNAL_GTEST "Use system gtest as dependency" ON)
    if (ARBITER_EXTERNAL_GTEST)
        find_package(GTest REQUIRED)
    else()
        add_subdirectory(third/gtest-1.7.0)
        include_directories(third/gtest-1.7.0/include third/gtest-1.7.0)
    endif()
    add_subdirectory(test)
endif()

install(TARGETS arbiter
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY arbiter/ DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")
