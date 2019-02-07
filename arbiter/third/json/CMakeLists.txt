set(MODULE json)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    HEADERS
    "${BASE}/json.hpp"
)

install(FILES ${HEADERS} DESTINATION include/arbiter/third/${MODULE})

