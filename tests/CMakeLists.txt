include(GoogleTest)
find_package(Threads REQUIRED)

# ---- pump-cmn-stat ----
add_executable(pump-cmn-stat-tests
    common/pump-cmn-stat/STAT_StatusTest.cpp)
target_link_libraries(pump-cmn-stat-tests PRIVATE
    pump::cmn-stat
    pumpkin::warnings
    GTest::gtest_main)
gtest_discover_tests(pump-cmn-stat-tests)

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
    pump::cmn-stat
    pump::srv-net
    pumpkin::warnings
    GTest::gtest_main
    Threads::Threads)
gtest_discover_tests(pump-srv-net-tests)
