include(GoogleTest)
find_package(Threads REQUIRED)

# ---- pump-core-eng ----
add_executable(pump-core-eng-tests
    core/pump-core-eng/ENG_KvStoreTest.cpp)
target_link_libraries(pump-core-eng-tests PRIVATE
    pump::core-eng
    pumpkin::warnings
    GTest::gtest_main)
gtest_discover_tests(pump-core-eng-tests)

# ---- pump-mid-pwp ----
add_executable(pump-mid-pwp-tests
    middlewares/pump-mid-pwp/PWP_CodecTest.cpp)
target_link_libraries(pump-mid-pwp-tests PRIVATE
    pump::mid-pwp
    pumpkin::warnings
    GTest::gtest_main)
gtest_discover_tests(pump-mid-pwp-tests)

# ---- pump-mid-cde ----
add_executable(pump-mid-cde-tests
    middlewares/pump-mid-cde/CDE_DispatcherTest.cpp)
target_link_libraries(pump-mid-cde-tests PRIVATE
    pump::mid-cde
    pumpkin::warnings
    GTest::gtest_main)
gtest_discover_tests(pump-mid-cde-tests)

# ---- pump-srv-net (real TCP on 127.0.0.1) ----
add_executable(pump-srv-net-tests
    services/pump-srv-net/NET_TcpServerTest.cpp)
target_link_libraries(pump-srv-net-tests PRIVATE
    pump::srv-net
    pumpkin::warnings
    GTest::gtest_main
    Threads::Threads)
gtest_discover_tests(pump-srv-net-tests)
