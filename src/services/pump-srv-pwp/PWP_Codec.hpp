/**
 ******************************************************************************
 * @file    PWP_Codec.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Wire Protocol (PWP) codec: public interface.
 *          This file declares functions to:
 *           + Text:   extract lines, parse a line into a PwpRequest, encode a PwpReply as text
 *           + Binary: encode / decode PwpRequest and PwpReply as PBWP frames
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

#ifndef PUMPKIN_PWP_CODEC_HPP_
#define PUMPKIN_PWP_CODEC_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "services/pump-srv-pwp/PWP_Message.hpp"

namespace pumpkin::services {

/* Longest line accepted in bytes. Longer inputs would result in connection closure */
constexpr size_t PWP_MAX_LINE_LENGTH = 64 * 1024;
constexpr char   PWP_WHITESPACE[]    = " \t\r";

struct ParseResult {
    std::optional<PwpRequest> opt_Request;
    std::string               sz_Error;
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
 * @brief Parses one line into a request.
 *
 * @details Request names are case-insensitive. Words are separated by spaces or tabs or carriage
 *          returns; keys and values are single words. Only the request name is checked here;
 *          argument counts and key sizes are checked by CDE. The request id is 0.
 *
 * @param sz_Line One line, without its line ending.
 *
 * @return The request, or an error for an empty line or an unknown request name.
 */
ParseResult PWP_ParseLine(const std::string& sz_Line);

/**
 * @brief Converts a reply into the text sent to the client.
 *
 * @param str_Reply The reply to encode.
 *
 * @return One line of text, including the trailing "\n".
 */
std::string PWP_EncodeReply(const PwpReply& str_Reply);

/**
 * @brief Encodes one request as a PBWP frame into a caller-provided buffer.
 *
 * @details Size query: with ptr_Dst == NULL and siz_Capacity == 0, nothing is written and the
 *          required size is returned, so the caller can size the buffer first.
 *
 * @param str_Request  The opcode, request id and arguments to send wrapped in PwpRequest.
 * @param ptr_Dst      Where the PBWP frame is written.
 * @param siz_Capacity Number of bytes available at ptr_Dst.
 *
 * @return Bytes written (> 0), or the required size for a size query. -P_ERANGE if the buffer is
 *         too small or the frame exceeds the protocol limits; -P_EINVARG if ptr_Dst is NULL with a
 *         non-zero capacity. Nothing is written on failure.
 */
int PWP_EncodeRequestFrame(const PwpRequest& str_Request, uint8_t* ptr_Dst, size_t siz_Capacity);

/**
 * @brief Decodes the first request frame in a byte buffer. The buffer is not modified.
 *
 * @details Validates the frame (PBWP_ValidateFrame) and the argument list. Does not check the
 *          opcode value or argument rules: CDE does.
 *
 * @param ptr_Src     Received bytes; may hold a partial frame, or several frames.
 * @param siz_Length  Number of bytes at ptr_Src.
 * @param ptr_Request Receives the decoded request.
 *
 * @return Frame size in bytes (> 0); the caller removes that many bytes. 0 if more bytes are
 *         needed. A negative P_E* code if the bytes are not a valid frame (-P_EINVARG, -P_ERANGE):
 *         the stream can't be trusted any more, so the caller closes the connection.
 *         -P_EINVARG also for a NULL ptr_Request, or a NULL ptr_Src with a non-zero length.
 */
int PWP_DecodeRequestFrame(const uint8_t* ptr_Src, size_t siz_Length, PwpRequest* ptr_Request);

/**
 * @brief Encodes one reply as a PBWP frame. Same size-query and return conventions as
 *        PWP_EncodeRequestFrame(); -P_EINVARG also for an unknown reply type.
 *
 * @param str_Reply    Reply type, request id and payload (or integer). u8_Status is written only
 *                     for RPL_ERROR, as the first payload byte.
 * @param ptr_Dst      Where the frame is written.
 * @param siz_Capacity Bytes available at ptr_Dst.
 *
 * @return Bytes written (> 0), or the required size for a size query. -P_ERANGE if the buffer is
 *         too small or the frame exceeds the protocol limits; -P_EINVARG if ptr_Dst is NULL with a
 *         non-zero capacity, or if the reply type is unknown. Nothing is written on failure.
 */
int PWP_EncodeReplyFrame(const PwpReply& str_Reply, uint8_t* ptr_Dst, size_t siz_Capacity);

/**
 * @brief Decodes the first reply frame in a byte buffer. Same return convention as
 *        PWP_DecodeRequestFrame(); -P_EINVARG also for an unknown reply type or a payload that
 *        does not match it.
 *
 * @param ptr_Src    Received bytes.
 * @param siz_Length Number of bytes at ptr_Src.
 * @param ptr_Reply  Receives the decoded reply.
 *
 * @return Frame size in bytes (> 0); the caller removes that many bytes. 0 if more bytes are
 *         needed. A negative P_E* code if the bytes are not a valid frame (-P_EINVARG, -P_ERANGE):
 *         the stream can't be trusted any more, so the caller closes the connection.
 *         -P_EINVARG also for a NULL ptr_Reply, or a NULL ptr_Src with a non-zero length.
 */
int PWP_DecodeReplyFrame(const uint8_t* ptr_Src, size_t siz_Length, PwpReply* ptr_Reply);

}  // namespace pumpkin::services

#endif  // PUMPKIN_PWP_CODEC_HPP_
