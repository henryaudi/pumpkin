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
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0. See the LICENSE file in the
 * root directory of this repository for the full license text.
 ******************************************************************************
 */

#include "services/pump-srv-pwp/PWP_Codec.hpp"

#include <cctype>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"
#include "middlewares/pump-mid-pbwp/PBWP_Codec.hpp"

LOG_MODULE_REGISTER(PWP, LOG_LEVEL_DBG)

namespace pumpkin::services {

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
    const size_t siz_FirstIdx = sz_Text.find_first_not_of(PWP_WHITESPACE);
    if (siz_FirstIdx == std::string::npos) {
        return "";
    }
    const size_t siz_LastIdx = sz_Text.find_last_not_of(PWP_WHITESPACE);

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
    const size_t siz_StartIdx = sz_Text.find_first_not_of(PWP_WHITESPACE);
    if (siz_StartIdx == std::string::npos) {
        sz_Text.clear();
        return "";
    }

    /* Find the space/tab right after the last char of the word. */
    const size_t siz_EndIdx = sz_Text.find_first_of(PWP_WHITESPACE, siz_StartIdx);
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
 * @brief Common checks before encoding a frame: size query, NULL buffer, capacity.
 *
 * @param siz_FrameSize The size of the frame to be encoded.
 * @param ptr_Dst The destination buffer for the encoded frame.
 * @param siz_Capacity The capacity of the destination buffer.
 *
 * @return 0 if the output is valid, a negative error code otherwise.
 */
int pwp_checkOutput(size_t siz_FrameSize, const uint8_t* ptr_Dst, size_t siz_Capacity) {
    if (ptr_Dst == nullptr) {
        return (siz_Capacity == 0) ? static_cast<int>(siz_FrameSize) : -P_EINVARG;
    }
    if (siz_Capacity < siz_FrameSize) {
        return -P_ERANGE;
    }

    return P_OK;
}

}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
std::optional<std::string> PWP_ExtractLine(std::string& sz_Buffer) {
    const size_t siz_LineLen = sz_Buffer.find('\n');
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
    std::string sz_Tmp = pwp_trim(sz_Line);

    /* Extract the first word (command) */
    const std::string sz_Cmd = pwp_toUpper(pwp_popNextWord(sz_Tmp));
    if (sz_Cmd.empty()) {
        return pwp_makeError("Empty request");
    }

    PwpRequest str_Request;

    /* Map the command word to op code */
    if (sz_Cmd == "PING") {
        str_Request.enm_Opcode = PwpOpcode::OP_PING;
    } else if (sz_Cmd == "GET") {
        str_Request.enm_Opcode = PwpOpcode::OP_GET;
    } else if (sz_Cmd == "SET") {
        str_Request.enm_Opcode = PwpOpcode::OP_SET;
    } else if (sz_Cmd == "DEL") {
        str_Request.enm_Opcode = PwpOpcode::OP_DEL;
    } else {
        return pwp_makeError("Unknown request: '" + sz_Cmd + "'");
    }

    /* Extract args */
    while (!sz_Tmp.empty()) {
        str_Request.vec_Args.push_back(pwp_popNextWord(sz_Tmp));
    }

    ParseResult str_Result;
    str_Result.opt_Request = std::move(str_Request);

    return str_Result;
}

std::string PWP_EncodeReply(const PwpReply& str_Reply) {
    switch (str_Reply.enm_Type) {
        case PwpReplyType::RPL_OK:
            return "OK\n";
        case PwpReplyType::RPL_PONG:
            return "PONG\n";
        case PwpReplyType::RPL_VALUE:
            return "VALUE " + str_Reply.sz_Payload + "\n";
        case PwpReplyType::RPL_NIL:
            return "NIL\n";
        case PwpReplyType::RPL_INTEGER:
            return "INT " + std::to_string(str_Reply.s64_Integer) + "\n";
        case PwpReplyType::RPL_ERROR:
            return "ERR " + str_Reply.sz_Payload + "\n";
    }
    return "ERR internal error\n";
}

int PWP_EncodeRequestFrame(const PwpRequest& str_Request, uint8_t* ptr_Dst, size_t siz_Capacity) {
    if (str_Request.vec_Args.size() > std::numeric_limits<uint16_t>::max()) {
        LOG_WRN("request %u: %zu arguments, more than the 16-bit count allows",
                str_Request.u32_RequestId, str_Request.vec_Args.size());
        return -P_ERANGE;
    }

    /* Payload: 2-byte argument count, then per argument a 4-byte length and its bytes */
    size_t siz_PayloadLength = sizeof(uint16_t);
    for (const std::string& sz_Arg : str_Request.vec_Args) {
        siz_PayloadLength += sizeof(uint32_t) + sz_Arg.size();
    }
    if (siz_PayloadLength > middlewares::PBWP_MAX_PAYLOAD_SIZE) {
        LOG_WRN("request %u: payload %zu bytes over the %zu-byte limit", str_Request.u32_RequestId,
                siz_PayloadLength, middlewares::PBWP_MAX_PAYLOAD_SIZE);
        return -P_ERANGE;
    }

    const size_t siz_FrameSize = middlewares::PBWP_HEADER_SIZE + siz_PayloadLength;
    const int    int_Ret       = pwp_checkOutput(siz_FrameSize, ptr_Dst, siz_Capacity);
    if (int_Ret != 0) {
        return int_Ret;  // size query, or the buffer is NULL/too small
    }

    /* Encode header */
    middlewares::PBWP_SetHeader(ptr_Dst, siz_Capacity, static_cast<uint8_t>(str_Request.enm_Opcode),
                                0, str_Request.u32_RequestId,
                                static_cast<uint32_t>(siz_PayloadLength));

    /* Encode arguments */
    size_t siz_Pos = middlewares::PBWP_HEADER_SIZE;
    middlewares::PBWP_PutLe16(ptr_Dst + siz_Pos,
                              static_cast<uint16_t>(str_Request.vec_Args.size()));
    siz_Pos += sizeof(uint16_t);

    for (const std::string& sz_Arg : str_Request.vec_Args) {
        middlewares::PBWP_PutLe32(ptr_Dst + siz_Pos, static_cast<uint32_t>(sz_Arg.size()));
        siz_Pos += sizeof(uint32_t);
        std::memcpy(ptr_Dst + siz_Pos, sz_Arg.data(), sz_Arg.size());
        siz_Pos += sz_Arg.size();
    }

    return static_cast<int>(siz_FrameSize);
}

int PWP_DecodeRequestFrame(const uint8_t* ptr_Src, size_t siz_Length, PwpRequest* ptr_Request) {
    if (ptr_Request == nullptr || (ptr_Src == nullptr && siz_Length > 0)) {
        LOG_ERR("DecodeRequestFrame called with a NULL pointer");
        return -P_EINVARG;
    }

    const int int_FrameSize = middlewares::PBWP_ValidateFrame(ptr_Src, siz_Length);
    if (int_FrameSize <= 0) {
        return int_FrameSize;  // 0: need more bytes, < 0: broken stream
    }

    middlewares::PbwpHeader str_Header;
    middlewares::PBWP_GetHeader(ptr_Src, siz_Length, &str_Header);

    /* Offsets below are relative to the payload, not the frame header */
    const uint8_t* ptr_Payload = middlewares::PBWP_GetPayload(ptr_Src);
    const size_t   siz_End     = str_Header.u32_PayloadLength;
    size_t         siz_Pos     = 0;

    if (siz_End < sizeof(uint16_t)) {
        LOG_WRN("request %u: payload of %zu bytes has no argument count", str_Header.u32_RequestId,
                siz_End);
        return -P_EINVARG;
    }

    const uint16_t u16_ArgCount = middlewares::PBWP_GetLe16(ptr_Payload);
    siz_Pos += sizeof(uint16_t);
    /* Each argument requires at least 4 bytes */
    if (u16_ArgCount > ((siz_End - siz_Pos) / sizeof(uint32_t))) {
        LOG_WRN("request %u: %u arguments can't fit in %zu payload bytes", str_Header.u32_RequestId,
                static_cast<unsigned>(u16_ArgCount), siz_End - siz_Pos);
        return -P_EINVARG;
    }

    std::vector<std::string> vec_Args;
    vec_Args.reserve(u16_ArgCount);
    for (uint16_t i = 0; i < u16_ArgCount; i++) {
        if (siz_End - siz_Pos < sizeof(uint32_t)) {  // arg size + arg requires at least 4 bytes
            LOG_WRN("request %u: argument %u has hit the end of the payload and beyond",
                    str_Header.u32_RequestId, static_cast<unsigned>(i));
            return -P_EINVARG;
        }
        const uint32_t u32_ArgLength = middlewares::PBWP_GetLe32(ptr_Payload + siz_Pos);
        siz_Pos += sizeof(uint32_t);
        if (siz_End - siz_Pos < u32_ArgLength) {  // Hmm, someone is lying
            LOG_WRN("request %u: argument %u claims %u bytes, only %zu left",
                    str_Header.u32_RequestId, static_cast<unsigned>(i), u32_ArgLength,
                    siz_End - siz_Pos);
            return -P_EINVARG;
        }

        vec_Args.emplace_back(reinterpret_cast<const char*>(ptr_Payload + siz_Pos), u32_ArgLength);
        siz_Pos += u32_ArgLength;
    }

    if (siz_Pos != siz_End) {
        LOG_WRN("request %u: %zu bytes left over after the last argument", str_Header.u32_RequestId,
                siz_End - siz_Pos);
        return -P_EINVARG;
    }

    LOG_TRC("request %u: opcode %u, %u arguments, %d bytes", str_Header.u32_RequestId,
            static_cast<unsigned>(str_Header.u8_Type), static_cast<unsigned>(u16_ArgCount),
            int_FrameSize);

    ptr_Request->enm_Opcode    = static_cast<PwpOpcode>(str_Header.u8_Type);
    ptr_Request->u32_RequestId = str_Header.u32_RequestId;
    ptr_Request->vec_Args      = std::move(vec_Args);

    return int_FrameSize;
}

int PWP_EncodeReplyFrame(const PwpReply& str_Reply, uint8_t* ptr_Dst, size_t siz_Capacity) {
    /* Determine the payload length by the reply type */
    size_t siz_PayloadLength = 0;
    switch (str_Reply.enm_Type) {
        case PwpReplyType::RPL_OK:
        case PwpReplyType::RPL_NIL:
        case PwpReplyType::RPL_PONG:
            siz_PayloadLength = 0;
            break;
        case PwpReplyType::RPL_VALUE:
            siz_PayloadLength = str_Reply.sz_Payload.size();
            break;
        case PwpReplyType::RPL_ERROR:
            siz_PayloadLength = sizeof(uint8_t) + str_Reply.sz_Payload.size();
            break;
        case PwpReplyType::RPL_INTEGER:
            siz_PayloadLength = sizeof(int64_t);
            break;
        default:
            LOG_ERR("reply %u: unknown reply type %u", str_Reply.u32_RequestId,
                    static_cast<unsigned>(str_Reply.enm_Type));
            return -P_EINVARG;
    }

    if (siz_PayloadLength > middlewares::PBWP_MAX_PAYLOAD_SIZE) {
        LOG_WRN("reply %u: payload %zu bytes over the %zu-byte limit", str_Reply.u32_RequestId,
                siz_PayloadLength, middlewares::PBWP_MAX_PAYLOAD_SIZE);
        return -P_ERANGE;
    }

    const size_t siz_FrameSize = middlewares::PBWP_HEADER_SIZE + siz_PayloadLength;
    const int    int_Ret       = pwp_checkOutput(siz_FrameSize, ptr_Dst, siz_Capacity);
    if (int_Ret != 0) {
        return int_Ret;
    }

    middlewares::PBWP_SetHeader(ptr_Dst, siz_Capacity, static_cast<uint8_t>(str_Reply.enm_Type), 0,
                                str_Reply.u32_RequestId, static_cast<uint32_t>(siz_PayloadLength));

    /* Encode the payload data */
    uint8_t* ptr_Payload = ptr_Dst + middlewares::PBWP_HEADER_SIZE;
    switch (str_Reply.enm_Type) {
        case PwpReplyType::RPL_VALUE:
            std::memcpy(ptr_Payload, str_Reply.sz_Payload.data(), str_Reply.sz_Payload.size());
            break;
        case PwpReplyType::RPL_ERROR:
            ptr_Payload[0] = str_Reply.u8_Status;
            std::memcpy(ptr_Payload + sizeof(uint8_t), str_Reply.sz_Payload.data(),
                        str_Reply.sz_Payload.size());
            break;
        case PwpReplyType::RPL_INTEGER:
            middlewares::PBWP_PutLe64(ptr_Payload, static_cast<uint64_t>(str_Reply.s64_Integer));
            break;
        default:
            break;  // RPL_OK, RPL_NIL, RPL_PONG have no payload
    }

    return static_cast<int>(siz_FrameSize);
}

int PWP_DecodeReplyFrame(const uint8_t* ptr_Src, size_t siz_Length, PwpReply* ptr_Reply) {
    if (ptr_Reply == nullptr || (ptr_Src == nullptr && siz_Length > 0)) {
        LOG_ERR("DecodeReplyFrame called with a NULL pointer");
        return -P_EINVARG;
    }

    const int int_FrameSize = middlewares::PBWP_ValidateFrame(ptr_Src, siz_Length);
    if (int_FrameSize <= 0) {
        return int_FrameSize;
    }

    middlewares::PbwpHeader str_Header;
    middlewares::PBWP_GetHeader(ptr_Src, siz_Length, &str_Header);

    const uint8_t*     ptr_Payload       = middlewares::PBWP_GetPayload(ptr_Src);
    const PwpReplyType enm_Type          = static_cast<PwpReplyType>(str_Header.u8_Type);
    const uint32_t     u32_RequestId     = str_Header.u32_RequestId;
    const uint32_t     u32_PayloadLength = str_Header.u32_PayloadLength;
    uint8_t            u8_Status         = P_OK;

    switch (enm_Type) {
        case PwpReplyType::RPL_OK:
        case PwpReplyType::RPL_NIL:
        case PwpReplyType::RPL_PONG:
            if (u32_PayloadLength != 0) {
                LOG_WRN("reply %u: type %u must be empty, has %u bytes", u32_RequestId,
                        static_cast<unsigned>(enm_Type), u32_PayloadLength);
                return -P_EINVARG;
            }
            ptr_Reply->sz_Payload.clear();
            ptr_Reply->s64_Integer = 0;
            break;
        case PwpReplyType::RPL_VALUE:
            ptr_Reply->sz_Payload.assign(reinterpret_cast<const char*>(ptr_Payload),
                                         u32_PayloadLength);
            ptr_Reply->s64_Integer = 0;
            break;
        case PwpReplyType::RPL_ERROR:
            if (u32_PayloadLength < sizeof(uint8_t)) {
                LOG_WRN("reply %u: error reply has no status byte", u32_RequestId);
                return -P_EINVARG;
            }
            u8_Status = ptr_Payload[0];
            ptr_Reply->sz_Payload.assign(
                reinterpret_cast<const char*>(ptr_Payload + sizeof(uint8_t)),
                u32_PayloadLength - sizeof(uint8_t));
            ptr_Reply->s64_Integer = 0;
            break;
        case PwpReplyType::RPL_INTEGER:
            if (u32_PayloadLength != sizeof(uint64_t)) {
                LOG_WRN("reply %u: integer reply has %u bytes, expected %zu", u32_RequestId,
                        u32_PayloadLength, sizeof(uint64_t));
                return -P_EINVARG;
            }
            ptr_Reply->sz_Payload.clear();
            ptr_Reply->s64_Integer = static_cast<int64_t>(middlewares::PBWP_GetLe64(ptr_Payload));
            break;
        default:
            LOG_WRN("reply %u: unknown reply type %u", u32_RequestId,
                    static_cast<unsigned>(enm_Type));
            return -P_EINVARG;
    }

    LOG_TRC("reply %u: type %u, status %u, %d bytes", u32_RequestId,
            static_cast<unsigned>(enm_Type), static_cast<unsigned>(u8_Status), int_FrameSize);

    ptr_Reply->enm_Type      = enm_Type;
    ptr_Reply->u8_Status     = u8_Status;
    ptr_Reply->u32_RequestId = u32_RequestId;

    return int_FrameSize;
}

}  // namespace pumpkin::services
