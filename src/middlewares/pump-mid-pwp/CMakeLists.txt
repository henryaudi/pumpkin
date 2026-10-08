# ---- pump-mid-pwp: text protocol (lines <-> commands and replies) ----
add_library(pump-mid-pwp STATIC
    PWP_Codec.cpp)
add_library(pump::mid-pwp ALIAS pump-mid-pwp)

target_include_directories(pump-mid-pwp PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-mid-pwp PRIVATE pumpkin::warnings)
