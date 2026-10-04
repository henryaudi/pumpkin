# ---- pumpkin-core-engine: in-memory key-value storage for one shard ----
add_library(pumpkin-core-engine STATIC
    ENG_KvStore.cpp)
add_library(pumpkin::core-engine ALIAS pumpkin-core-engine)

target_include_directories(pumpkin-core-engine PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pumpkin-core-engine PRIVATE pumpkin::warnings)
