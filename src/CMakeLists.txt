# ---- common: header-only utilities shared by every subsystem ----
add_library(pumpkin_common INTERFACE)
add_library(pumpkin::common ALIAS pumpkin_common)

configure_file(common/version.hpp.in ${PROJECT_BINARY_DIR}/generated/common/version.hpp @ONLY)

target_include_directories(pumpkin_common INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}/generated)
target_compile_features(pumpkin_common INTERFACE cxx_std_20)