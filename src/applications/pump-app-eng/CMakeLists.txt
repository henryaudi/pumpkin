# ---- pump-app-eng: in-memory key-value storage for one shard ----
add_library(pump-app-eng STATIC
    ENG_KvStore.cpp)
add_library(pump::app-eng ALIAS pump-app-eng)

target_include_directories(pump-app-eng PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-app-eng PRIVATE pumpkin::warnings)
