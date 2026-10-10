/**
 ******************************************************************************
 * @file    STAT_Status.hpp
 * @author  Shangjie Zheng
 * @brief   Status codes shared by every layer: public interface.
 *          This file provides:
 *           + P_OK and the P_E* error codes returned by functions that can fail
 *           + STAT_ToString: readable text for a code
 *           + STAT_MapErrno: an OS errno translated into a code
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

#ifndef PUMPKIN_STAT_STATUS_HPP_
#define PUMPKIN_STAT_STATUS_HPP_

/* Success */
inline constexpr int P_OK = 0;

/* Errors */
inline constexpr int P_EIO        = 1;
inline constexpr int P_ENOMEM     = 2;
inline constexpr int P_EINVARG    = 3;
inline constexpr int P_EALREADY   = 4;
inline constexpr int P_ENOTINIT   = 5;
inline constexpr int P_ERANGE     = 6;
inline constexpr int P_EADDRINUSE = 7;
inline constexpr int P_EACCES     = 8;
inline constexpr int P_ECONNRESET = 9;
inline constexpr int P_EPIPE      = 10;

/**
 * @brief Gets readable text for a status code.
 *
 * @param int_Status P_OK or one of the P_E* error codes.
 *
 * @return Readable text for the status code.
 */
const char* STAT_ToString(int int_Status);

/**
 * @brief Translates an OS errno into a status code.
 *
 * @details Call it right after a failing system call: STAT_MapErrno(errno). errno values that have
 *          no code of their own become P_EIO.
 *
 * @param int_Errno The OS errno value to translate.
 *
 * @return The corresponding status code.
 */
int STAT_MapErrno(int int_Errno);

#endif  // PUMPKIN_STAT_STATUS_HPP_
