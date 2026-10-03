# ---- pumpkin-services-network: single-threaded TCP server (poll) ----
add_library(pumpkin-services-network STATIC
    tcp_server.cpp)
add_library(pumpkin::services-network ALIAS pumpkin-services-network)

target_include_directories(pumpkin-services-network PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pumpkin-services-network
    PUBLIC
        pumpkin::core-engine
    PRIVATE
        pumpkin::middlewares-dispatch
        pumpkin::warnings)
