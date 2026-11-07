# ---- pump-srv-pwp: wire protocol (text lines and PBWP frames <-> requests and replies) ----
add_library(pump-srv-pwp STATIC
    PWP_Codec.cpp)
add_library(pump::srv-pwp ALIAS pump-srv-pwp)

target_include_directories(pump-srv-pwp PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-srv-pwp
    PRIVATE
        pump::mid-pbwp
        pump::cmn-log
        pump::cmn-stat
        pumpkin::warnings)
