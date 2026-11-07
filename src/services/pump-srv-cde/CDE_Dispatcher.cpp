/**
 ******************************************************************************
 * @file    CDE_Dispatcher.cpp
 * @author  Shangjie Zheng
 * @brief   Command Dispatch Engine (CDE).
 *          This file provides functions to:
 *           + Validate a request and run it against a KvStore
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
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"

LOG_MODULE_REGISTER(CDE, LOG_LEVEL_DBG)

namespace pumpkin::services {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================
/* Longest part of a value shown in the log; longer values end with "..." */
constexpr size_t CDE_LOG_VALUE_PREVIEW = 32;

/**
 * @brief Gives the name of an opcode, for logs and error messages.
 *
 * @param enm_Opcode The opcode to get the string for.
 *
 * @return The string representation of the opcode.
 */
const char* cde_getOpcodeString(PwpOpcode enm_Opcode) {
    switch (enm_Opcode) {
        case PwpOpcode::OP_PING:
            return "PING";
        case PwpOpcode::OP_GET:
            return "GET";
        case PwpOpcode::OP_SET:
            return "SET";
        case PwpOpcode::OP_DEL:
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
 *         opcode; -P_ERANGE for a key longer than PWP_MAX_KEY_SIZE.
 */
int cde_validateRequest(const PwpRequest& str_Request, std::string* ptr_Error) {
    const std::vector<std::string>& vec_Args     = str_Request.vec_Args;
    const size_t                    siz_ArgCount = vec_Args.size();

    switch (str_Request.enm_Opcode) {
        case PwpOpcode::OP_PING:
            if (siz_ArgCount != 0) {
                *ptr_Error = "PING takes no arguments";
                return -P_EINVARG;
            }
            return P_OK;
        case PwpOpcode::OP_GET:
        case PwpOpcode::OP_DEL:
            if (siz_ArgCount != 1) {
                *ptr_Error = std::string(cde_getOpcodeString(str_Request.enm_Opcode)) +
                             " takes exactly one argument";
                return -P_EINVARG;
            }
            break;
        case PwpOpcode::OP_SET:
            if (siz_ArgCount == 0 || siz_ArgCount % 2 != 0) {
                *ptr_Error = "SET takes key value pairs";
                return -P_EINVARG;
            }
            break;
        default:
            *ptr_Error = "Unknown opcode";
            return -P_EINVARG;
    }

    /* SET checks every key (even indexes), GET/DEL their one key */
    const size_t siz_Step = (str_Request.enm_Opcode == PwpOpcode::OP_SET) ? 2 : 1;
    for (size_t siz_Idx = 0; siz_Idx < siz_ArgCount; siz_Idx += siz_Step) {
        if (vec_Args[siz_Idx].size() > PWP_MAX_KEY_SIZE) {
            *ptr_Error = "key too long";
            return -P_ERANGE;
        }
    }

    return P_OK;
}

/**
 * @brief Builds an RPL_ERROR reply.
 *
 * @param u32_RequestId The id of the request being answered.
 * @param int_Status    A positive P_E* code, sent as the status byte.
 * @param sz_Message    Error text for the client.
 *
 * @return The error reply.
 */
PwpReply cde_makeError(uint32_t u32_RequestId, int int_Status, const std::string& sz_Message) {
    PwpReply str_Reply;
    str_Reply.enm_Type      = PwpReplyType::RPL_ERROR;
    str_Reply.u8_Status     = static_cast<uint8_t>(int_Status);
    str_Reply.u32_RequestId = u32_RequestId;
    str_Reply.sz_Payload    = sz_Message;
    return str_Reply;
}

}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
PwpReply CDE_ExecuteRequest(const PwpRequest& str_Request, applications::KvStore& obj_Store) {
    const uint32_t u32_Id = str_Request.u32_RequestId;

    std::string sz_Error;
    const int   int_Ret = cde_validateRequest(str_Request, &sz_Error);
    if (int_Ret < 0) {
        LOG_DBG("rejected %s request %u: %s", cde_getOpcodeString(str_Request.enm_Opcode), u32_Id,
                sz_Error.c_str());
        return cde_makeError(u32_Id, -int_Ret, sz_Error);
    }

    PwpReply str_Reply;
    str_Reply.u32_RequestId = u32_Id;

    switch (str_Request.enm_Opcode) {
        case PwpOpcode::OP_PING:
            LOG_INF("PING");
            str_Reply.enm_Type = PwpReplyType::RPL_PONG;
            return str_Reply;
        case PwpOpcode::OP_SET: {
            const std::vector<std::string>& vec_Args = str_Request.vec_Args;
            for (size_t siz_Idx = 0; siz_Idx < vec_Args.size(); siz_Idx += 2) {
                const std::string& sz_Key = vec_Args[siz_Idx];
                const std::string& sz_Val = vec_Args[siz_Idx + 1];
                obj_Store.set(sz_Key, sz_Val);

                LOG_INF("SET '%s' (%zu bytes)", sz_Key.c_str(), sz_Val.size());

                const size_t siz_Shown = std::min(sz_Val.size(), CDE_LOG_VALUE_PREVIEW);
                LOG_DBG("SET '%s' = '%.*s'%s", sz_Key.c_str(), static_cast<int>(siz_Shown),
                        sz_Val.c_str(), (sz_Val.size() > siz_Shown) ? "..." : "");
            }
            str_Reply.enm_Type = PwpReplyType::RPL_OK;
            return str_Reply;
        }
        case PwpOpcode::OP_GET: {
            const std::string&         sz_Key    = str_Request.vec_Args[0];
            std::optional<std::string> opt_Value = obj_Store.get(sz_Key);
            if (!opt_Value.has_value()) {
                LOG_INF("GET '%s': not found", sz_Key.c_str());
                str_Reply.enm_Type = PwpReplyType::RPL_NIL;
                return str_Reply;
            }
            const std::string& sz_Value = opt_Value.value();
            LOG_INF("GET '%s': found (%zu bytes)", sz_Key.c_str(), sz_Value.size());

            const size_t siz_Shown = std::min(sz_Value.size(), CDE_LOG_VALUE_PREVIEW);
            LOG_DBG("GET '%s' = '%.*s'%s", sz_Key.c_str(), static_cast<int>(siz_Shown),
                    sz_Value.c_str(), (sz_Value.size() > siz_Shown) ? "..." : "");

            str_Reply.enm_Type   = PwpReplyType::RPL_VALUE;
            str_Reply.sz_Payload = std::move(opt_Value.value());  // no copy of the value
            return str_Reply;
        }
        case PwpOpcode::OP_DEL: {
            const std::string& sz_Key      = str_Request.vec_Args[0];
            const bool         bol_Removed = obj_Store.remove(sz_Key);
            LOG_INF("DEL '%s': %s", sz_Key.c_str(), bol_Removed ? "removed" : "not found");

            /* Return the number of keys removed */
            str_Reply.enm_Type    = PwpReplyType::RPL_INTEGER;
            str_Reply.s64_Integer = bol_Removed ? 1 : 0;
            return str_Reply;
        }
        default:
            /* cde_validateRequest() already rejects unknown opcodes */
            break;
    }
    LOG_ERR("unhandled opcode %d", static_cast<int>(str_Request.enm_Opcode));
    return cde_makeError(u32_Id, P_EINVARG, "internal error: unhandled opcode");
}

}  // namespace pumpkin::services
