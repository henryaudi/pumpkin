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

#include "middlewares/pump-mid-cde/CDE_Dispatcher.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "middlewares/pump-mid-pwp/PWP_Codec.hpp"

LOG_MODULE_REGISTER(CDE, LOG_LEVEL_DBG)

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

/* Longest part of a value shown in the log; longer values end with "..." */
constexpr size_t CDE_LOG_VALUE_PREVIEW = 32;

}  // namespace

namespace pumpkin::middlewares {

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
Reply CDE_ExecuteCommand(const Command& str_Command, core::KvStore& obj_Store) {
    switch (str_Command.enm_Type) {
        case CommandType::CMD_PING:
            LOG_INF("PING");
            return Reply{ReplyType::REPLY_PONG, "", 0};
        case CommandType::CMD_SET: {
            for (size_t siz_Idx = 0; siz_Idx < str_Command.vec_Args.size(); siz_Idx += 2) {
                const std::string& sz_Key = str_Command.vec_Args[siz_Idx];
                const std::string& sz_Val = str_Command.vec_Args[siz_Idx + 1];
                obj_Store.set(sz_Key, sz_Val);

                LOG_INF("SET '%s' (%zu bytes)", sz_Key.c_str(), sz_Val.size());

                const size_t siz_Shown = std::min(sz_Val.size(), CDE_LOG_VALUE_PREVIEW);
                LOG_DBG("SET '%s' = '%.*s'%s", sz_Key.c_str(), static_cast<int>(siz_Shown),
                        sz_Val.c_str(), (sz_Val.size() > siz_Shown) ? "..." : "");
            }
            return Reply{ReplyType::REPLY_OK, "", 0};
        }
        case CommandType::CMD_GET: {
            const std::string&               sz_Key    = str_Command.vec_Args[0];
            const std::optional<std::string> opt_Value = obj_Store.get(sz_Key);
            if (!opt_Value.has_value()) {
                LOG_INF("GET '%s': not found", sz_Key.c_str());
                return Reply{ReplyType::REPLY_NIL, "", 0};
            }
            const std::string& sz_Value = opt_Value.value();
            LOG_INF("GET '%s': found (%zu bytes)", sz_Key.c_str(), sz_Value.size());

            const size_t siz_Shown = std::min(sz_Value.size(), CDE_LOG_VALUE_PREVIEW);
            LOG_DBG("GET '%s' = '%.*s'%s", sz_Key.c_str(), static_cast<int>(siz_Shown),
                    sz_Value.c_str(), (sz_Value.size() > siz_Shown) ? "..." : "");
            return Reply{ReplyType::REPLY_VALUE, sz_Value, 0};
        }
        case CommandType::CMD_DEL: {
            const std::string& sz_Key      = str_Command.vec_Args[0];
            const bool         bol_Removed = obj_Store.remove(sz_Key);
            LOG_INF("DEL '%s': %s", sz_Key.c_str(), bol_Removed ? "removed" : "not found");
            /* Return the number of keys removed */
            return Reply{ReplyType::REPLY_INTEGER, "", bol_Removed ? 1 : 0};
        }
        default:
            /* Unknown command type */
            break;
    }
    LOG_ERR("unknown command type %d", static_cast<int>(str_Command.enm_Type));
    return Reply{ReplyType::REPLY_ERROR, "internal error: unknown command", 0};
}

std::string CDE_ProcessLine(const std::string& sz_Line, core::KvStore& obj_Store) {
    if (sz_Line.find_first_not_of(PWP_WHITESPACE) == std::string::npos) {
        return "";
    }

    const ParseResult str_Parsed = PWP_ParseLine(sz_Line);
    if (!str_Parsed.opt_Command.has_value()) {
        LOG_DBG("rejected command: %s", str_Parsed.sz_Error.c_str());
        return PWP_EncodeReply(Reply{ReplyType::REPLY_ERROR, str_Parsed.sz_Error, 0});
    }

    return PWP_EncodeReply(CDE_ExecuteCommand(str_Parsed.opt_Command.value(), obj_Store));
}

}  // namespace pumpkin::middlewares
