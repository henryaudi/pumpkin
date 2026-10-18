/**
 ******************************************************************************
 * @file    LOG_Logger.cpp
 * @author  Shangjie Zheng
 * @brief   Logging shared by every layer: implementation.
 *          This file provides functions to:
 *           + Set the runtime level from its name
 *           + Format a line and write it to stderr through spdlog
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Shangjie Zheng.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0. See the LICENSE file in the
 * root directory of this repository for the full license text.
 ******************************************************************************
 */

#include "common/pump-cmn-log/LOG_Logger.hpp"

#include <pthread.h>  // retrieve pthread_threadid_np for macOS
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif

#include <array>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

#include "common/pump-cmn-stat/STAT_Status.hpp"

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

constexpr size_t LOG_MAX_LINE_LENGTH = 1024;  // Maximum length of a log line
constexpr char   LOG_LOGGER_NAME[]   = "pumpkin";

/* Runtime level set by LOG_Init() */
std::atomic<int> log_atm_runtimeLevel{LOG_LEVEL_INF};
/* The process-wide logger: constructed before main() runs, configured by LOG_Init() */
spdlog::logger log_obj_logger(LOG_LOGGER_NAME, std::make_shared<spdlog::sinks::stderr_sink_mt>());

/**
 * @brief Applies Pumpkin's output settings to a logger.
 *
 * @param ptr_Logger The logger to configure.
 */
void log_configureLogger(spdlog::logger* ptr_Logger) {
    ptr_Logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
    ptr_Logger->set_level(
        spdlog::level::trace);  // Let everything through for this step, filter will be added by
                                // LOG_WRITE_IF_ENABLED and LOG_Write
}

/**
 * @brief Configures Pumpkin's process-wide logger.
 *
 * @param int_Level One of the LOG_LEVEL_* values; more verbose lines will be dropped.
 */
void log_createPumpkinLogger(int int_Level) {
    log_configureLogger(&log_obj_logger);
    log_atm_runtimeLevel.store(int_Level);
}

/**
 * @brief Gets the OS id of the calling thread (the id shown by top -H, htop, gdb and lldb).
 *
 * @return The thread id.
 */
uint64_t log_getOsThreadId() {
#if defined(__APPLE__)
    uint64_t u64_ThreadId = 0;
    pthread_threadid_np(nullptr, &u64_ThreadId);
    return u64_ThreadId;
#else
    /* Linux */
    return static_cast<uint64_t>(syscall(SYS_gettid));
#endif
}

/**
 * @brief Retrieves the name of a log level.
 *
 * @param int_Level One of the LOG_LEVEL_* constants.
 *
 * @return The name of the log level as a string.
 */
const char* log_getLogLevelName(int int_Level) {
    switch (int_Level) {
        case LOG_LEVEL_CRT:
            return "CRITICAL";
        case LOG_LEVEL_ERR:
            return "ERROR";
        case LOG_LEVEL_WRN:
            return "WARN";
        case LOG_LEVEL_INF:
            return "INFO";
        case LOG_LEVEL_DBG:
            return "DEBUG";
        default:
            return "TRACE";
    }
}

/**
 * @brief Converts our level to spdlog's, so future sinks (files) can filter by it too.
 *
 * @param int_Level One of the LOG_LEVEL_* values.
 *
 * @return The matching spdlog level.
 */
spdlog::level::level_enum log_toSpdlogLevel(int int_Level) {
    switch (int_Level) {
        case LOG_LEVEL_CRT:
            return spdlog::level::critical;
        case LOG_LEVEL_ERR:
            return spdlog::level::err;
        case LOG_LEVEL_WRN:
            return spdlog::level::warn;
        case LOG_LEVEL_INF:
            return spdlog::level::info;
        case LOG_LEVEL_DBG:
            return spdlog::level::debug;
        default:
            return spdlog::level::trace;
    }
}
}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
int LOG_Init(const char* sz_Level) {
    if (sz_Level == nullptr) {
        return -P_EINVARG;
    }

    int int_Level = LOG_LEVEL_INF;
    if (std::strcmp(sz_Level, "none") == 0) {
        int_Level = LOG_LEVEL_NONE;
    } else if (std::strcmp(sz_Level, "critical") == 0) {
        int_Level = LOG_LEVEL_CRT;
    } else if (std::strcmp(sz_Level, "error") == 0) {
        int_Level = LOG_LEVEL_ERR;
    } else if (std::strcmp(sz_Level, "warn") == 0) {
        int_Level = LOG_LEVEL_WRN;
    } else if (std::strcmp(sz_Level, "info") == 0) {
        int_Level = LOG_LEVEL_INF;
    } else if (std::strcmp(sz_Level, "debug") == 0) {
        int_Level = LOG_LEVEL_DBG;
    } else if (std::strcmp(sz_Level, "trace") == 0) {
        int_Level = LOG_LEVEL_TRC;
    } else {
        return -P_EINVARG;
    }

    log_createPumpkinLogger(int_Level);
    return P_OK;
}

void LOG_Write(const char* sz_Module, int int_Level, const char* sz_Format, ...) {
    if (int_Level > log_atm_runtimeLevel.load()) {
        return;
    }

    thread_local const uint64_t           u64_ThreadId = log_getOsThreadId();
    std::array<char, LOG_MAX_LINE_LENGTH> arr_Line;

    /* Prefix: [LEVEL] padded to 8 ("CRITICAL" being the longest so far), [MODULE] padded to 4,
       then the OS thread id.
       e.g. "[INFO    ] [MOD ] [12345] " */
    const int int_PrefixLen = std::snprintf(
        arr_Line.data(), arr_Line.size(), "[%-8s] [%-4s] [%llu] ", log_getLogLevelName(int_Level),
        sz_Module, static_cast<unsigned long long>(u64_ThreadId));
    if (int_PrefixLen < 0 || static_cast<size_t>(int_PrefixLen) >= arr_Line.size()) {
        return;
    }
    const size_t siz_PrefixLen = static_cast<size_t>(int_PrefixLen);

    /* Format the log message with the remaining in va_Args */
    va_list va_Args;
    va_start(va_Args, sz_Format);
    std::vsnprintf(arr_Line.data() + siz_PrefixLen, arr_Line.size() - siz_PrefixLen, sz_Format,
                   va_Args);
    va_end(va_Args);

    /* spdlog adds the timestamp and writes the line to stderr (thread-safe sink) */
    log_obj_logger.log(log_toSpdlogLevel(int_Level), spdlog::string_view_t(arr_Line.data()));
}
