include(GoogleTest)

# ---- pumpkin-core-engine ----
add_executable(pumpkin-core-engine-tests
    pumpkin-core/engine/kv_store_test.cpp)
target_link_libraries(pumpkin-core-engine-tests PRIVATE
    pumpkin::core-engine
    pumpkin::warnings
    GTest::gtest_main)
gtest_discover_tests(pumpkin-core-engine-tests)
