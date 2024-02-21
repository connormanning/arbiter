set(MODULE drivers)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    SOURCES
    "${BASE}/http.cpp"
    "${BASE}/dropbox.cpp"
    "${BASE}/fs.cpp"
    "${BASE}/google.cpp"
    "${BASE}/s3.cpp"
    "${BASE}/az.cpp"
)

set(
    HEADERS
    "${BASE}/http.hpp"
    "${BASE}/dropbox.hpp"
    "${BASE}/fs.hpp"
    "${BASE}/google.hpp"
    "${BASE}/s3.hpp"
    "${BASE}/az.hpp"
    "${BASE}/test.hpp"
)

add_library(${MODULE} OBJECT ${SOURCES} ${HEADERS})

if (WIN32 AND BUILD_SHARED_LIBS)
    set_target_properties(${MODULE}
        PROPERTIES
            COMPILE_DEFINITIONS ARBITER_DLL_EXPORT)
endif()

install(FILES ${HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/arbiter/${MODULE}")
