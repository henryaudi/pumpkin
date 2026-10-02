add_library(pumpkin_warnings INTERFACE)
add_library(pumpkin::warnings ALIAS pumpkin_warnings)

target_compile_options(pumpkin_warnings INTERFACE
  -Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wsign-conversion
  -Wold-style-cast -Wcast-align
  -Wnon-virtual-dtor -Woverloaded-virtual
  -Wimplicit-fallthrough
  $<$<BOOL:${PUMPKIN_WERROR}>:-Werror>)