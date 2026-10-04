#ifndef PUMPKIN_PROTO_CODEC_HPP_
#define PUMPKIN_PROTO_CODEC_HPP_

#include <cstddef>
#include <optional>
#include <string>

#include "pumpkin-middlewares/protocol/PROTO_Command.hpp"
#include "pumpkin-middlewares/protocol/PROTO_Reply.hpp"

namespace pumpkin::middlewares {

/* Longest line accepted in bytes. Longer inputs would result in connection closure */
constexpr std::size_t PROTO_MAX_LINE_LENGTH = 64 * 1024;

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
std::optional<std::string> PROTO_ExtractLine(std::string& sz_Buffer);

/**
 * @brief Parses one line into a command.
 *
 * @details Command names are case-insensitive. For SET, the value is the rest of the line after
 *          the key, so it may contain spaces.
 *
 * @param sz_Line One line, without its line ending.
 *
 * @return The command, or an error message describing what is wrong with the line.
 */
ParseResult PROTO_ParseCommand(const std::string& sz_Line);

/**
 * @brief Converts a reply into the text sent to the client.
 *
 * @param str_Reply The reply to encode.
 *
 * @return One line of text, including the trailing "\n".
 */
std::string PROTO_EncodeReply(const Reply& str_Reply);

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PROTO_CODEC_HPP_
