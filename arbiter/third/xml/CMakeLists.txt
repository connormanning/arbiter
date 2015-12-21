set(MODULE xml)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    HEADERS
    "${BASE}/rapidxml.hpp"
    "${BASE}/xml.hpp"
)

set(THIRD_SOURCES ${THIRD_SOURCES} ${SOURCES} PARENT_SCOPE)
install(FILES ${HEADERS} DESTINATION include/arbiter/third/${MODULE})

