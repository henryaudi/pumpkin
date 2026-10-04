# ---- pumpkin-middleware-protocol: text protocol (lines <-> commands and replies) ----
add_library(pumpkin-middlewares-protocol STATIC
    PROTO_Codec.cpp)
add_library(pumpkin::middlewares-protocol ALIAS pumpkin-middlewares-protocol)

target_include_directories(pumpkin-middlewares-protocol PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pumpkin-middlewares-protocol PRIVATE pumpkin::warnings)
