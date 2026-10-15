/**
 ******************************************************************************
 * @file    PBWP_Codec.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Binary Wire Protocol (PBWP) codec: public interface.
 *          This file declares functions to:
 *           + Encode / decode request frames (client side encodes, server decodes)
 *           + Turn a decoded request into the protocol-independent Command
 *           + Encode / decode reply frames (server side encodes, client decodes)
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

#ifndef PUMPKIN_PBWP_CODEC_HPP_
#define PUMPKIN_PBWP_CODEC_HPP_

#include <cstdint>
#include <string>

#include "middlewares/pump-mid-pbwp/PBWP_Frame.hpp"
#include "middlewares/pump-mid-pwp/PWP_Command.hpp"
#include "middlewares/pump-mid-pwp/PWP_Reply.hpp"

namespace pumpkin::middlewares {

/**
 * @brief Encodes one request frame into a caller-provided byte buffer.
 *
 * @details Size query: with ptr_Out == NULL and siz_Capacity == 0, nothing is written and the
 *          required size is returned, so the caller can size the buffer first.
 *
 * @param str_Request  The opcode, request id and arguments to send.
 * @param ptr_Out      Where the frame is written, starting at ptr_Out[0].
 * @param siz_Capacity Bytes available at ptr_Out buffer.
 *
 * @return Bytes written (> 0), or the required size for a size query. -P_ERANGE if the buffer is
 *         too small or the frame exceeds the protocol limits; -P_EINVARG if ptr_Out is NULL with a
 *         non-zero capacity. Nothing is written on failure.
 */
int PBWP_EncodeRequest(const PbwpRequest& str_Request, std::uint8_t* ptr_Out,
                       std::size_t siz_Capacity);

/**
 * @brief Decodes the first request frame in a byte buffer. The buffer is not modified.
 *
 * @param ptr_In      Received bytes; may hold a partial frame, or several frames.
 * @param siz_Length  Number of bytes at ptr_In.
 * @param ptr_Request Receives the decoded request.
 *
 * @return Frame size in bytes (> 0); the caller removes that many bytes. 0 if more bytes are
 *         needed. A negative P_E* code if the bytes are not a valid frame (-P_EINVARG, -P_ERANGE):
 *         the stream can't be trusted any more, so the caller closes the connection.
 *         -P_EINVARG also for a NULL ptr_Request, or a NULL ptr_In with a non-zero length.
 */
int PBWP_DecodeRequest(const std::uint8_t* ptr_In, std::size_t siz_Length,
                       PbwpRequest* ptr_Request);

/**
 * @brief Checks a decoded request's opcode and arguments, and turns it into a Command.
 *
 * @details Errors here are about one request, not the stream: the caller sends an error reply
 *          and keeps the connection open, like a bad text command.
 *
 * @param ptr_Request The decoded request; its arguments are moved into the command.
 * @param ptr_Command Receives the command.
 * @param ptr_Error   Receives the error message on failure.
 *
 * @return P_OK; -P_EINVARG for an unknown opcode or a wrong argument count; -P_ERANGE if a key is
 *         longer than PBWP_MAX_KEY_SIZE.
 */
int PBWP_PbwpToCommand(PbwpRequest* ptr_Request, Command* ptr_Command, std::string* ptr_Error);

/**
 * @brief Encodes one reply frame into a caller-provided byte buffer. Same size-query and return
 *        conventions as PBWP_EncodeRequest().
 *
 * @param str_Reply    Reply type, request id and payload (or integer) to send. u8_Status is
 *                     written only for RPL_ERROR, as the first payload byte.
 * @param ptr_Out      Where the frame is written, starting at ptr_Out[0].
 * @param siz_Capacity Bytes available at ptr_Out.
 *
 * @return Bytes written (> 0), or the required size for a size query; -P_ERANGE, -P_EINVARG as
 *         for PBWP_EncodeRequest(), and -P_EINVARG for an unknown reply type.
 */
int PBWP_EncodeReply(const PbwpReply& str_Reply, std::uint8_t* ptr_Out, std::size_t siz_Capacity);

/**
 * @brief Decodes the first reply frame in a byte buffer. Same return convention as
 *        PBWP_DecodeRequest().
 *
 * @param ptr_In     Received bytes.
 * @param siz_Length Number of bytes at ptr_In.
 * @param ptr_Reply  Receives the decoded reply.
 *
 * @return Frame size (> 0), 0 if more bytes are needed, or a negative P_E* code.
 */
int PBWP_DecodeReply(const std::uint8_t* ptr_In, std::size_t siz_Length, PbwpReply* ptr_Reply);

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PBWP_CODEC_HPP_
