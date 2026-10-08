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

#include "middlewares/pump-mid-pwp/PWP_Codec.hpp"

#include <cctype>
#include <string>
#include <vector>

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
 * @brief Returns a copy of the text without leading and trailing whitespace.
 *
 * @param sz_Text The text to trim.
 *
 * @return The trimmed text, or an empty string if the text is only whitespace.
 */
std::string pwp_trim(const std::string& sz_Text) {
    const std::size_t siz_FirstIdx = sz_Text.find_first_not_of(PWP_WHITESPACE);
    if (siz_FirstIdx == std::string::npos) {
        return "";
    }
    const std::size_t siz_LastIdx = sz_Text.find_last_not_of(PWP_WHITESPACE);

    return sz_Text.substr(siz_FirstIdx, siz_LastIdx - siz_FirstIdx + 1);
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
    const std::size_t siz_StartIdx = sz_Text.find_first_not_of(PWP_WHITESPACE);
    if (siz_StartIdx == std::string::npos) {
        sz_Text.clear();
        return "";
    }

    /* Find the space/tab right after the last char of the word. */
    const std::size_t siz_EndIdx = sz_Text.find_first_of(PWP_WHITESPACE, siz_StartIdx);
    if (siz_EndIdx == std::string::npos) {
        /*  No more spaces found, take the rest of the string as the word */
        std::string sz_Word = sz_Text.substr(siz_StartIdx);
        sz_Text.clear();
        return sz_Word;
    }

    /*  Extract the word between siz_StartIdx and siz_EndIdx */
    std::string sz_Word = sz_Text.substr(siz_StartIdx, siz_EndIdx - siz_StartIdx);

    sz_Text.erase(0, siz_EndIdx);

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
 * @brief Creates a ParseResult object with a command
 *
 * @param enm_Type The type of the command.
 * @param vec_Args The arguments associated with the command.
 *
 * @return A ParseResult object with the command set.
 */
ParseResult pwp_makeCommand(CommandType enm_Type, const std::vector<std::string>& vec_Args) {
    ParseResult str_Res;
    str_Res.opt_Command = Command{enm_Type, vec_Args};
    return str_Res;
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
    std::string       sz_Tmp  = pwp_trim(sz_Line);
    const std::string sz_Word = pwp_toUpper(pwp_popNextWord(sz_Tmp));  // gEt -> GET

    if (sz_Word.empty()) {
        return pwp_makeError("Empty command");
    }

    /* Split the rest of the line into words */
    std::vector<std::string> vec_Args;
    while (!sz_Tmp.empty()) {
        vec_Args.push_back(pwp_popNextWord(sz_Tmp));
    }
    const std::size_t siz_ArgCount = vec_Args.size();

    if (sz_Word == "PING") {
        if (siz_ArgCount != 0) {
            return pwp_makeError("PING takes no arguments");
        }
        return pwp_makeCommand(CommandType::CMD_PING, vec_Args);
    }
    if (sz_Word == "GET" || sz_Word == "DEL") {
        if (siz_ArgCount != 1) {
            return pwp_makeError(sz_Word + " takes exactly one argument.");
        }

        const CommandType enm_Type =
            (sz_Word == "GET") ? CommandType::CMD_GET : CommandType::CMD_DEL;
        return pwp_makeCommand(enm_Type, vec_Args);
    }
    if (sz_Word == "SET") {
        if (siz_ArgCount == 0 || siz_ArgCount % 2 != 0) {
            return pwp_makeError("SET takes key value pairs");
        }
        return pwp_makeCommand(CommandType::CMD_SET, vec_Args);
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
