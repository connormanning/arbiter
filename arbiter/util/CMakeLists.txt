set(MODULE util)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    SOURCES
    "${BASE}/transforms.cpp"
    "${BASE}/md5.cpp"
    "${BASE}/sha256.cpp"
)

set(
    HEADERS
    "${BASE}/transforms.hpp"
    "${BASE}/macros.hpp"
    "${BASE}/md5.hpp"
    "${BASE}/sha256.hpp"
)

if (WIN32)
    add_library(${MODULE} OBJECT ${SOURCES} ${HEADERS})
else()
    add_library(${MODULE} OBJECT ${SOURCES})
endif()

install(FILES ${HEADERS} DESTINATION include/arbiter/${MODULE})

