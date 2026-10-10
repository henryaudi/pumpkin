/**
 ******************************************************************************
 * @file    STAT_Status.cpp
 * @author  Shangjie Zheng
 * @brief   Status codes shared by every layer: public interface.
 *          This file provides the implementation for:
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

#include "common/pump-cmn-stat/STAT_Status.hpp"

#include <cerrno>

namespace pumpkin::common {

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
const char* STAT_ToString(int int_Status) {
    switch (int_Status) {
        case P_OK:
            return "success";
        case P_EIO:
            return "input/output error";
        case P_EINVARG:
            return "invalid argument";
        case P_ENOMEM:
            return "out of memory";
        case P_EALREADY:
            return "already done";
        case P_ENOTINIT:
            return "not initialized";
        case P_ERANGE:
            return "value out of range";
        case P_EADDRINUSE:
            return "address already in use";
        case P_EACCES:
            return "permission denied";
        case P_ECONNRESET:
            return "connection reset by peer";
        case P_EPIPE:
            return "connection closed by peer";
        default:
            return "unknown error";
    }
}

int STAT_MapErrno(int int_Errno) {
    switch (int_Errno) {
        case EINVAL:
            return P_EINVARG;
        case ENOMEM:
            return P_ENOMEM;
        case EALREADY:
            return P_EALREADY;
        case ERANGE:
            return P_ERANGE;
        case EADDRINUSE:
            return P_EADDRINUSE;
        case EACCES:
        case EPERM:
            return P_EACCES;
        case ECONNRESET:
            return P_ECONNRESET;
        case EPIPE:
            return P_EPIPE;
        default:
            return P_EIO;  // every other system error
    }
}

}  // namespace pumpkin::common
