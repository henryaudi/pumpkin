# ---- pump-core-eng: in-memory key-value storage for one shard ----
add_library(pump-core-eng STATIC
    ENG_KvStore.cpp)
add_library(pump::core-eng ALIAS pump-core-eng)

target_include_directories(pump-core-eng PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-core-eng PRIVATE pumpkin::warnings)
