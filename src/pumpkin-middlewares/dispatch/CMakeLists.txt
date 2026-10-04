# ---- pumpkin-middlewares-dispatch: runs commands against a KvStore ----
add_library(pumpkin-middlewares-dispatch STATIC
    DISP_Dispatcher.cpp)
add_library(pumpkin::middlewares-dispatch ALIAS pumpkin-middlewares-dispatch)

target_include_directories(pumpkin-middlewares-dispatch PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pumpkin-middlewares-dispatch
    PUBLIC
        pumpkin::middlewares-protocol
        pumpkin::core-engine
    PRIVATE
        pumpkin::warnings)
