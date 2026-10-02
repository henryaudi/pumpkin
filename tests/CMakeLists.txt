include(GoogleTest)

add_executable(pumpkin_unit_tests
    unit/smoke_test.cpp)
target_link_libraries(pumpkin_unit_tests PRIVATE
    pumpkin::warnings
    GTest::gtest_main)

gtest_discover_tests(pumpkin_unit_tests)
