# ---- pump-cmn-stat: status codes (P_OK, P_E*) shared by every layer ----
add_library(pump-cmn-stat STATIC
    STAT_Status.cpp)

add_library(pump::cmn-stat ALIAS pump-cmn-stat)

target_include_directories(pump-cmn-stat PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-cmn-stat PRIVATE pumpkin::warnings)
