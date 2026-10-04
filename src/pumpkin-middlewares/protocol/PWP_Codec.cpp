/**
 ******************************************************************************
 * @file    PWP_Codec.cpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Wire Protocol (PWP) codec.
 *          This file provides functions to:
 *           + Extract complete lines from received bytes
 *           + Parse a line into a Command
 *           + Encode a Reply into text sent to the client
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

#include "pumpkin-middlewares/protocol/PWP_Codec.hpp"

#include <cctype>
#include <string>

namespace pumpkin::middlewares {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

/**
 * @brief Converts a string to uppercase.
 *
 * @param sz_Text The string to convert to uppercase.
 *
 * @return The uppercase version of the input string.
 */
std::string pwp_toUpper(std::string sz_Text) {
    for (char& ch_Char : sz_Text) {
        ch_Char = static_cast<char>(std::toupper(static_cast<unsigned char>(ch_Char)));
    }
    return sz_Text;
}

/**
 * @brief Extracts the next word (lower memory index) from a string and removes it from the
 *        original string.
 *
 * @param sz_Text The string to extract the word from.
 *
 * @return The extracted word. If no word is found, returns an empty string.
 */
std::string pwp_popNextWord(std::string& sz_Text) {
    const std::size_t siz_StartIdx = sz_Text.find_first_not_of(' ');
    if (siz_StartIdx == std::string::npos) {
        /*  No non-space characters found, clear the string buffer and return empty string */
        sz_Text.clear();
        return "";
    }

    const std::size_t siz_EndIdx = sz_Text.find(' ', siz_StartIdx);
    if (siz_EndIdx == std::string::npos) {
        /*  No more spaces found, take the rest of the string as the word */
        std::string sz_Word = sz_Text.substr(siz_StartIdx);
        sz_Text.clear();
        return sz_Word;
    }

    /*  Extract the word between siz_StartIdx and siz_EndIdx */
    std::string sz_Word = sz_Text.substr(siz_StartIdx, siz_EndIdx - siz_StartIdx);
    sz_Text.erase(0, siz_EndIdx + 1);
    return sz_Word;
}

/**
 * @brief Creates a ParseResult object with an error message.
 *
 * @param sz_Message The error message to set in the ParseResult.
 *
 * @return A ParseResult object with the error message set.
 */
ParseResult pwp_makeError(const std::string& sz_Message) {
    ParseResult str_Result;
    str_Result.sz_Error = sz_Message;
    return str_Result;
}

/**
 * @brief Creates a ParseResult object with a command.
 *
 * @param enm_Type The type of the command.
 * @param sz_Key   The key associated with the command.
 * @param sz_Value The value associated with the command.
 *
 * @return A ParseResult object with the command set.
 */
ParseResult pwp_makeCommand(CommandType enm_Type, const std::string& sz_Key,
                            const std::string& sz_Value) {
    ParseResult str_Result;
    str_Result.opt_Command = Command{enm_Type, sz_Key, sz_Value};
    return str_Result;
}
}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
std::optional<std::string> PWP_ExtractLine(std::string& sz_Buffer) {
    const std::size_t siz_LineLen = sz_Buffer.find('\n');
    if (siz_LineLen == std::string::npos) {
        return std::nullopt;
    }

    /* Extract the line and remove it from the buffer */
    std::string sz_Line = sz_Buffer.substr(0, siz_LineLen);
    sz_Buffer.erase(0, siz_LineLen + 1);

    /* Remove trailing carriage return if present */
    if (!sz_Line.empty() && sz_Line.back() == '\r') {
        sz_Line.pop_back();
    }
    return sz_Line;
}

ParseResult PWP_ParseLine(const std::string& sz_Line) {
    std::string       sz_Tmp  = sz_Line;  // Temporary pointer to traverse the line.
    const std::string sz_Word = pwp_toUpper(pwp_popNextWord(sz_Tmp));

    if (sz_Word.empty()) {
        return pwp_makeError("Empty command");
    }

    if (sz_Word == "PING") {
        if (!sz_Tmp.empty()) {
            return pwp_makeError("PING takes no arguments");
        }
        return pwp_makeCommand(CommandType::CMD_PING, "", "");
    }

    if (sz_Word == "GET" || sz_Word == "DEL") {
        const std::string sz_Key = pwp_popNextWord(sz_Tmp);
        if (sz_Key.empty() || !sz_Tmp.empty()) {  // If the key is empty or extra args
            return pwp_makeError(sz_Word + " takes exactly one key");
        }

        const CommandType enm_Type =
            (sz_Word == "GET") ? CommandType::CMD_GET : CommandType::CMD_DEL;
        return pwp_makeCommand(enm_Type, sz_Key, "");
    }

    if (sz_Word == "SET") {
        const std::string sz_Key = pwp_popNextWord(sz_Tmp);
        if (sz_Key.empty() || sz_Tmp.empty()) {  // If the key or value is missing
            return pwp_makeError("SET takes a key and a value");
        }

        /* Note: the value is the remainder of the line, which might include spaces */
        return pwp_makeCommand(CommandType::CMD_SET, sz_Key, sz_Tmp);
    }

    return pwp_makeError("Unknown command: '" + sz_Word + "'");
}

std::string PWP_EncodeReply(const Reply& str_Reply) {
    switch (str_Reply.enm_Type) {
        case ReplyType::REPLY_OK:
            return "OK\n";
        case ReplyType::REPLY_PONG:
            return "PONG\n";
        case ReplyType::REPLY_VALUE:
            return "VALUE " + str_Reply.sz_Text + "\n";
        case ReplyType::REPLY_NIL:
            return "NIL\n";
        case ReplyType::REPLY_INTEGER:
            return "INT " + std::to_string(str_Reply.s64_Integer) + "\n";
        case ReplyType::REPLY_ERROR:
            return "ERR " + str_Reply.sz_Text + "\n";
    }
    return "ERR internal error\n";
}

}  // namespace pumpkin::middlewares
