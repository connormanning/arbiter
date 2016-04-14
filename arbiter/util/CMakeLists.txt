set(MODULE util)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    SOURCES
    "${BASE}/crypto.cpp"
    "${BASE}/sha256.cpp"
)

set(
    HEADERS
    "${BASE}/crypto.hpp"
    "${BASE}/macros.hpp"
    "${BASE}/sha256.hpp"
)

install(FILES ${HEADERS} DESTINATION include/arbiter/${MODULE})

if (WIN32)
    add_library(${MODULE} OBJECT ${SOURCES} ${HEADERS})
else()
    add_library(${MODULE} OBJECT ${SOURCES})
endif()

