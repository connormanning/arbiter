set(MODULE util)
set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    SOURCES
    "${BASE}/curl.cpp"
    "${BASE}/http.cpp"
    "${BASE}/ini.cpp"
    "${BASE}/md5.cpp"
    "${BASE}/sha256.cpp"
    "${BASE}/time.cpp"
    "${BASE}/transforms.cpp"
    "${BASE}/util.cpp"
)

set(
    HEADERS
    "${BASE}/curl.hpp"
    "${BASE}/exports.hpp"
    "${BASE}/http.hpp"
    "${BASE}/ini.hpp"
    "${BASE}/macros.hpp"
    "${BASE}/md5.hpp"
    "${BASE}/sha256.hpp"
    "${BASE}/time.hpp"
    "${BASE}/transforms.hpp"
    "${BASE}/types.hpp"
    "${BASE}/util.hpp"
)


if (WIN32)
    add_library(${MODULE} OBJECT ${SOURCES} ${HEADERS})
    set_target_properties(${MODULE}
        PROPERTIES
            COMPILE_DEFINITIONS ARBITER_DLL_EXPORT)
    target_include_directories(util PRIVATE "${CURL_INCLUDE_DIR}")

else()
    add_library(${MODULE} OBJECT ${SOURCES})
endif()

install(FILES ${HEADERS} DESTINATION include/arbiter/${MODULE})

