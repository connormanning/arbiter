set(MODULE third)

if (EXTERNAL_JSON)
    target_compile_definitions(arbiter PRIVATE ARBITER_EXTERNAL_JSON)
else()
    add_subdirectory(json)
endif()

if (EXTERNAL_XML)
    target_compile_definitions(arbiter PRIVATE ARBITER_EXTERNAL_XML)
else()
    add_subdirectory(xml)
endif()
