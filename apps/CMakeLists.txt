add_executable(pumpkin-server pumpkin-server/main.cpp)
target_link_libraries(pumpkin-server PRIVATE pump::cmn-stat pump::srv-net pumpkin::warnings)
