include(GoogleTest)
find_package(Threads REQUIRED)

# ---- pumpkin-core-engine ----
add_executable(pumpkin-core-engine-tests
    pumpkin-core/engine/ENG_KvStoreTest.cpp)
target_link_libraries(pumpkin-core-engine-tests PRIVATE
    pumpkin::core-engine
    pumpkin::warnings
    GTest::gtest_main)
gtest_discover_tests(pumpkin-core-engine-tests)

# ---- pumpkin-middlewares-protocol ----
add_executable(pumpkin-middlewares-protocol-tests
    pumpkin-middlewares/protocol/PWP_CodecTest.cpp)
target_link_libraries(pumpkin-middlewares-protocol-tests PRIVATE
    pumpkin::middlewares-protocol
    pumpkin::warnings
    GTest::gtest_main)
gtest_discover_tests(pumpkin-middlewares-protocol-tests)

# ---- pumpkin-middlewares-dispatch ----
add_executable(pumpkin-middlewares-dispatch-tests
    pumpkin-middlewares/dispatch/CDE_DispatcherTest.cpp)
target_link_libraries(pumpkin-middlewares-dispatch-tests PRIVATE
    pumpkin::middlewares-dispatch
    pumpkin::warnings
    GTest::gtest_main)
gtest_discover_tests(pumpkin-middlewares-dispatch-tests)

# ---- pumpkin-services-network (real TCP on 127.0.0.1) ----
add_executable(pumpkin-services-network-tests
    pumpkin-services/network/NET_TcpServerTest.cpp)
target_link_libraries(pumpkin-services-network-tests PRIVATE
    pumpkin::services-network
    pumpkin::warnings
    GTest::gtest_main
    Threads::Threads)
gtest_discover_tests(pumpkin-services-network-tests)
