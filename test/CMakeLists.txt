add_executable(arbiter-test unit.cpp)
target_link_libraries(arbiter-test PRIVATE arbiter)

if (TARGET GTest::GTest)
    target_link_libraries(arbiter-test PRIVATE GTest::GTest GTest::Main)
else()
    target_link_libraries(arbiter-test PRIVATE gtest gtest_main )
endif()

set_target_properties(arbiter-test
    PROPERTIES
        COMPILE_DEFINITIONS ARBITER_DLL_IMPORT)

# We're overriding the test with a custom command for individual test output
# and colors, which cmake doesn't like.
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "No dev warnings")

add_test(
    NAME arbiter_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:arbiter-test>)
