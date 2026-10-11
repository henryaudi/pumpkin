/**
 ******************************************************************************
 * @file    LOG_Logger.hpp
 * @author  Shangjie Zheng
 * @brief   Logging shared by every layer: public interface.
 *          This file provides:
 *           + LOG_MODULE_REGISTER: names a source file's module and its level
 *           + LOG_CRT / LOG_ERR / LOG_WRN / LOG_INF / LOG_DBG / LOG_TRC
 *           + LOG_Init: sets the runtime level
 *          Output: [time] [LEVEL   ] [MOD ] [thread id] message, on stderr.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Shangjie Zheng.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 ******************************************************************************
 */

#ifndef PUMPKIN_LOG_LOGGER_HPP_
#define PUMPKIN_LOG_LOGGER_HPP_

/* Log levels, Zephyr style: a higher number means more detail */
inline constexpr int LOG_LEVEL_NONE = 0;
inline constexpr int LOG_LEVEL_CRT  = 1;
inline constexpr int LOG_LEVEL_ERR  = 2;
inline constexpr int LOG_LEVEL_WRN  = 3;
inline constexpr int LOG_LEVEL_INF  = 4;
inline constexpr int LOG_LEVEL_DBG  = 5;
inline constexpr int LOG_LEVEL_TRC  = 6;

#ifdef NDEBUG
inline constexpr int LOG_LEVEL_BUILD_MAX = LOG_LEVEL_INF;  // Maximum log level for release builds
#else
inline constexpr int LOG_LEVEL_BUILD_MAX = LOG_LEVEL_TRC;  // Maximum log level for debug builds
#endif  // NDEBUG

/**
 * @brief Sets the runtime log level: lines with a level higher than this will be ignored.
 *
 * @param sz_Level "none", "critical", "error", "warn", "info", "debug" or "trace".
 *
 * @return P_OK on success; -P_EINVARG for a NULL or unknown level name.
 */
int LOG_Init(const char* sz_Level);

/**
 * @brief Writes one log line. Called by the LOG_* macros; don't call it directly.
 *
 * @details format(printf, 3, 4) makes the compiler check sz_Format against the values, exactly
 *          like printf: a wrong %d / %s is a warning (an error with -Werror).
 *
 * @param sz_Module  Module tag, e.g. "NET".
 * @param int_Level  One of the LOG_LEVEL_* values.
 * @param sz_Format  printf-style format, followed by its values.
 */
void LOG_Write(const char* sz_Module, int int_Level, const char* sz_Format, ...)
    __attribute__((format(printf, 3, 4)));

#define LOG_MODULE_REGISTER(module, level)                                \
    namespace {                                                           \
    [[maybe_unused]] constexpr const char* log_sz_moduleName   = #module; \
    [[maybe_unused]] constexpr int         log_int_moduleLevel = (level); \
    }

#define LOG_WRITE_IF_ENABLED(level, ...)                                                  \
    do {                                                                                  \
        if constexpr ((level) <= log_int_moduleLevel && (level) <= LOG_LEVEL_BUILD_MAX) { \
            LOG_Write(log_sz_moduleName, (level), __VA_ARGS__);                           \
        }                                                                                 \
    } while (0)

#define LOG_CRT(...) LOG_WRITE_IF_ENABLED(LOG_LEVEL_CRT, __VA_ARGS__)
#define LOG_ERR(...) LOG_WRITE_IF_ENABLED(LOG_LEVEL_ERR, __VA_ARGS__)
#define LOG_WRN(...) LOG_WRITE_IF_ENABLED(LOG_LEVEL_WRN, __VA_ARGS__)
#define LOG_INF(...) LOG_WRITE_IF_ENABLED(LOG_LEVEL_INF, __VA_ARGS__)
#define LOG_DBG(...) LOG_WRITE_IF_ENABLED(LOG_LEVEL_DBG, __VA_ARGS__)
#define LOG_TRC(...) LOG_WRITE_IF_ENABLED(LOG_LEVEL_TRC, __VA_ARGS__)

#endif  // PUMPKIN_LOG_LOGGER_HPP_
