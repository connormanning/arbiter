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

add_library(${MODULE} OBJECT ${SOURCES} ${HEADERS})

install(FILES ${HEADERS} DESTINATION include/arbiter/${MODULE})


