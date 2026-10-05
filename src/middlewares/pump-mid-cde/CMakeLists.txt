# ---- pump-mid-cde: runs commands against a KvStore ----
add_library(pump-mid-cde STATIC
    CDE_Dispatcher.cpp)
add_library(pump::mid-cde ALIAS pump-mid-cde)

target_include_directories(pump-mid-cde PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-mid-cde
    PUBLIC
        pump::mid-pwp
        pump::core-eng
    PRIVATE
        pumpkin::warnings)
