set(MODULE util)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    SOURCES
    "${BASE}/http.cpp"
    "${BASE}/transforms.cpp"
    "${BASE}/md5.cpp"
    "${BASE}/sha256.cpp"
    "${BASE}/util.cpp"
)

set(
    HEADERS
    "${BASE}/http.hpp"
    "${BASE}/transforms.hpp"
    "${BASE}/macros.hpp"
    "${BASE}/md5.hpp"
    "${BASE}/sha256.hpp"
    "${BASE}/util.hpp"
)

if (WIN32)
    add_library(${MODULE} OBJECT ${SOURCES} ${HEADERS})
else()
    add_library(${MODULE} OBJECT ${SOURCES})
endif()

install(FILES ${HEADERS} DESTINATION include/arbiter/${MODULE})

