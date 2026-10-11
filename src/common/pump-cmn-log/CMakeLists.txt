# ---- pump-cmn-log: logging (LOG_MODULE_REGISTER, LOG_INF, ...) on top of spdlog ----
add_library(pump-cmn-log STATIC
    LOG_Logger.cpp)
add_library(pump::cmn-log ALIAS pump-cmn-log)

target_include_directories(pump-cmn-log PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-cmn-log
    PRIVATE
        pump::cmn-stat
        spdlog::spdlog
        pumpkin::warnings)
