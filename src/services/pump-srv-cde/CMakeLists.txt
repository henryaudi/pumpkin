# ---- pump-srv-cde: validate requests and run them against a KvStore ----
add_library(pump-srv-cde STATIC
    CDE_Dispatcher.cpp)
add_library(pump::srv-cde ALIAS pump-srv-cde)

target_include_directories(pump-srv-cde PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-srv-cde
    PUBLIC
        pump::app-eng
        pump::mid-pbwp       # CDE's API uses PbwpRequest / PbwpReply
    PRIVATE
        pump::cmn-log
        pump::cmn-stat
        pumpkin::warnings)
