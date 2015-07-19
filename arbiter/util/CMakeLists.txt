set(MODULE util)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    SOURCES
    "${BASE}/crypto.cpp"
)

set(
    HEADERS
    "${BASE}/crypto.hpp"
)

install(FILES ${HEADERS} DESTINATION include/arbiter/${MODULE})
add_library(${MODULE} OBJECT ${SOURCES})

