/**
 ******************************************************************************
 * @file    PWP_Codec.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Wire Protocol (PWP) codec: public interface.
 *          This file declares functions to:
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

#ifndef PUMPKIN_PWP_CODEC_HPP_
#define PUMPKIN_PWP_CODEC_HPP_

#include <cstddef>
#include <optional>
#include <string>

#include "middlewares/pump-mid-pwp/PWP_Command.hpp"
#include "middlewares/pump-mid-pwp/PWP_Reply.hpp"

namespace pumpkin::middlewares {

/* Longest line accepted in bytes. Longer inputs would result in connection closure */
constexpr std::size_t PWP_MAX_LINE_LENGTH = 64 * 1024;
constexpr char        PWP_WHITESPACE[]    = " \t\r";

struct ParseResult {
    std::optional<Command> opt_Command;
    std::string            sz_Error;
};

/**
 * @brief Removes the first complete line from a buffer.
 *
 * @details A line ends with "\n". A "\r" right before it is removed too, so clients that send
 *          "\r\n" (like telnet) work. Bytes after the line stay in the buffer.
 *
 * @param sz_Buffer Bytes received so far. The returned line is removed from it.
 *
 * @return The line without its line ending, or std::nullopt if there is no complete line yet.
 */
std::optional<std::string> PWP_ExtractLine(std::string& sz_Buffer);

/**
 * @brief Parses one line into a command.
 *
 * @details Command names are case-insensitive. Words are separated by spaces or tabs or carriage
 *          returns; keys and values are single words. SET takes one or more key value pairs.
 *
 * @param sz_Line One line, without its line ending.
 *
 * @return The command, or an error message describing what is wrong with the line.
 */
ParseResult PWP_ParseLine(const std::string& sz_Line);

/**
 * @brief Converts a reply into the text sent to the client.
 *
 * @param str_Reply The reply to encode.
 *
 * @return One line of text, including the trailing "\n".
 */
std::string PWP_EncodeReply(const Reply& str_Reply);

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PWP_CODEC_HPP_
