# ---- pump-mid-pbwp: Pumpkin Binary Wire Protocol (frame header access) ----
add_library(pump-mid-pbwp STATIC
    PBWP_Codec.cpp)
add_library(pump::mid-pbwp ALIAS pump-mid-pbwp)

target_include_directories(pump-mid-pbwp PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-mid-pbwp
    PUBLIC
        pump::cmn-stat
    PRIVATE
        pump::cmn-log
        pumpkin::warnings)
