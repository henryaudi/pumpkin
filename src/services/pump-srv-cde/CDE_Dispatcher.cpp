/**
 ******************************************************************************
 * @file    CDE_Dispatcher.cpp
 * @author  Shangjie Zheng
 * @brief   Command Dispatch Engine (CDE).
 *          This file provides functions to:
 *           + Execute a parsed Command against a KvStore
 *           + Process one PWP line end to end (parse, execute, encode)
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

#include "services/pump-srv-cde/CDE_Dispatcher.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"

LOG_MODULE_REGISTER(CDE, LOG_LEVEL_DBG)

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

/* Longest part of a value shown in the log; longer values end with "..." */
constexpr size_t CDE_LOG_VALUE_PREVIEW = 32;

}  // namespace

namespace pumpkin::services {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================
/* Longest part of a value shown in the log; longer values end with "..." */
constexpr size_t CDE_LOG_VALUE_PREVIEW = 32;

/**
 * @brief Gives the command name of an opcode, for logs and error messages.
 *
 * @param enm_Opcode The opcode to get the string for.
 *
 * @return The string representation of the opcode.
 */
const char* cde_getOpcodeString(middlewares::PbwpOpcode enm_Opcode) {
    switch (enm_Opcode) {
        case middlewares::PbwpOpcode::OP_PING:
            return "PING";
        case middlewares::PbwpOpcode::OP_GET:
            return "GET";
        case middlewares::PbwpOpcode::OP_SET:
            return "SET";
        case middlewares::PbwpOpcode::OP_DEL:
            return "DEL";
        default:
            break;
    }
    return "UNKNOWN";
}

/**
 * @brief Checks the argument count and key sizes of a request.
 *
 * @param str_Request The request to check.
 * @param ptr_Error   Receives the error message when the request is rejected.
 *
 * @return P_OK if the request can run; -P_EINVARG for a wrong argument count or an unknown
 *         opcode; -P_ERANGE for a key longer than PBWP_MAX_KEY_SIZE.
 */
int cde_validateRequest(const middlewares::PbwpRequest& str_Request, std::string* ptr_Error) {
    const std::vector<std::string>& vec_Args     = str_Request.vec_Args;
    const size_t                    siz_ArgCount = vec_Args.size();

    switch (str_Request.enm_Opcode) {
        case middlewares::PbwpOpcode::OP_PING:
            if (siz_ArgCount != 0) {
                *ptr_Error = "PING takes no arguments";
                return -P_EINVARG;
            }
            return P_OK;
        case middlewares::PbwpOpcode::OP_GET:
        case middlewares::PbwpOpcode::OP_DEL:
            if (siz_ArgCount != 1) {
                *ptr_Error = std::string(cde_getOpcodeString(str_Request.enm_Opcode)) +
                             " takes exactly one argument";
                return -P_EINVARG;
            }
            break;
        case middlewares::PbwpOpcode::OP_SET:
            if (siz_ArgCount == 0 || siz_ArgCount % 2 != 0) {
                *ptr_Error = "SET takes key value pairs";
                return -P_EINVARG;
            }
            break;
        default:
            *ptr_Error = "Unknown opcode";
            return -P_EINVARG;
    }

    const size_t siz_Step = (str_Request.enm_Opcode == middlewares::PbwpOpcode::OP_SET) ? 2 : 1;
    for (size_t i = 0; i < siz_ArgCount; i += siz_Step) {
        if (vec_Args[i].size() > middlewares::PBWP_MAX_KEY_SIZE) {
            *ptr_Error = "key too long";
            return -P_ERANGE;
        }
    }

    return P_OK;
}

middlewares::PbwpReply cde_makeError(uint32_t u32_RequestId, int int_Status,
                                     const std::string& sz_Message) {
    middlewares::PbwpReply str_Reply;
    str_Reply.enm_Type      = middlewares::PbwpReplyType::RPL_ERROR;
    str_Reply.u8_Status     = static_cast<uint8_t>(int_Status);
    str_Reply.u32_RequestId = u32_RequestId;
    str_Reply.sz_Payload    = sz_Message;
    return str_Reply;
}

}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
middlewares::PbwpReply CDE_ExecuteCommand(const middlewares::PbwpRequest& str_Request,
                                          applications::KvStore&          obj_Store) {
    const uint32_t u32_Id = str_Request.u32_RequestId;

    std::string sz_Error;
    const int   int_Ret = cde_validateRequest(str_Request, &sz_Error);
    if (int_Ret < 0) {
        LOG_DBG("rejected %s request %u: %s", cde_getOpcodeString(str_Request.enm_Opcode), u32_Id,
                sz_Error.c_str());
        return cde_makeError(u32_Id, -int_Ret, sz_Error);
    }

    middlewares::PbwpReply str_Reply;
    str_Reply.u32_RequestId = u32_Id;

    switch (str_Request.enm_Opcode) {}
}

}  // namespace pumpkin::services
