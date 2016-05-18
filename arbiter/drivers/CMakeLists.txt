set(MODULE drivers)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    SOURCES
    "${BASE}/http.cpp"
    "${BASE}/dropbox.cpp"
    "${BASE}/fs.cpp"
    "${BASE}/s3.cpp"
)

set(
    HEADERS
    "${BASE}/http.hpp"
    "${BASE}/dropbox.hpp"
    "${BASE}/fs.hpp"
    "${BASE}/s3.hpp"
)

install(FILES ${HEADERS} DESTINATION include/arbiter/${MODULE})

if (WIN32)
    add_library(${MODULE} OBJECT ${SOURCES} ${HEADERS})
else()
    add_library(${MODULE} OBJECT ${SOURCES})
endif()

