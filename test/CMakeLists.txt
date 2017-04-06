add_executable(arbiter-test unit.cpp)

target_link_libraries(arbiter-test PRIVATE arbiter gtest gtest_main)
set_target_properties(arbiter-test
    PROPERTIES
        COMPILE_DEFINITIONS ARBITER_DLL_IMPORT)
    add_definitions("-DARBITER_DLL_IMPORT")
# We're overriding the test with a custom command for individual test output
# and colors, which cmake doesn't like.
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "No dev warnings")

add_custom_target(
    test
    COMMAND arbiter-test
    DEPENDS arbiter-test
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)

