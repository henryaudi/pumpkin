include(FetchContent)

# SYSTEM: third-party headers are treated as system headers, so our -Werror never trips on them.

# ---- spdlog: logging backend, used only inside pump-cmn-log ----
FetchContent_Declare(spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz
    URL_HASH SHA256=15a04e69c222eb6c01094b5c7ff8a249b36bb22788d72519646fb85feb267e67
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)
FetchContent_MakeAvailable(spdlog)

# ---- googletest: only when tests are built ----
if(PUMPKIN_BUILD_TESTS)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SYSTEM)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
