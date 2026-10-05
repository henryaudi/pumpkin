# ---- pump-srv-net: single-threaded TCP server (poll) ----
add_library(pump-srv-net STATIC
    NET_TcpServer.cpp)
add_library(pump::srv-net ALIAS pump-srv-net)

target_include_directories(pump-srv-net PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-srv-net
    PUBLIC
        pump::core-eng
    PRIVATE
        pump::mid-cde
        pumpkin::warnings)
