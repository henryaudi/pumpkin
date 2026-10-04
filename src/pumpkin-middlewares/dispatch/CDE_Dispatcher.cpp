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
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 ******************************************************************************
 */

#include "pumpkin-middlewares/dispatch/CDE_Dispatcher.hpp"

#include <optional>
#include <string>

#include "pumpkin-middlewares/protocol/PWP_Codec.hpp"

namespace pumpkin::middlewares {

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
Reply CDE_ExecuteCommand(const Command& str_Command, core::KvStore& obj_Store) {
    switch (str_Command.enm_Type) {
        case CommandType::CMD_PING:
            return Reply{ReplyType::REPLY_PONG, "", 0};
        case CommandType::CMD_SET:
            obj_Store.set(str_Command.sz_Key, str_Command.sz_Value);
            return Reply{ReplyType::REPLY_OK, "", 0};
        case CommandType::CMD_GET: {
            const std::optional<std::string> opt_Value = obj_Store.get(str_Command.sz_Key);
            if (!opt_Value.has_value()) {
                return Reply{ReplyType::REPLY_NIL, "", 0};
            }
            return Reply{ReplyType::REPLY_VALUE, opt_Value.value(), 0};
        }
        case CommandType::CMD_DEL:
            const bool bol_Removed = obj_Store.remove(str_Command.sz_Key);
            /* Return the number of keys removed */
            return Reply{ReplyType::REPLY_INTEGER, "", bol_Removed ? 1 : 0};
    }
    return Reply{ReplyType::REPLY_ERROR, "internal error: unknown command", 0};
}

std::string CDE_ProcessLine(const std::string& sz_Line, core::KvStore& obj_Store) {
    if (sz_Line.empty()) {
        return "";
    }

    const ParseResult str_Parsed = PWP_ParseLine(sz_Line);
    if (!str_Parsed.opt_Command.has_value()) {
        return PWP_EncodeReply(Reply{ReplyType::REPLY_ERROR, str_Parsed.sz_Error, 0});
    }

    return PWP_EncodeReply(CDE_ExecuteCommand(str_Parsed.opt_Command.value(), obj_Store));
}

}  // namespace pumpkin::middlewares
