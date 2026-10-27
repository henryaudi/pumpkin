# ---- pump-srv-cms: Config Management Service (command line → module setters) ----
add_library(pump-srv-cms STATIC
    CMS_Manager.cpp)
add_library(pump::srv-cms ALIAS pump-srv-cms)

target_include_directories(pump-srv-cms PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pump-srv-cms
    PUBLIC
        pump::srv-net        # CMS configures NET; its users always need NET too
    PRIVATE
        pump::cmn-log
        pump::cmn-stat
        pumpkin::warnings)
