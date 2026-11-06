/**
 ******************************************************************************
 * @file    PWP_Message.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Wire Protocol (PWP): messages.
 *          This file declares:
 *           + PwpOpcode / PwpReplyType: request and reply kinds
 *           + PwpRequest / PwpReply: one request from a client, one reply to it
 *           + PWP_MAX_KEY_SIZE
 *          The same messages travel as text lines or as PBWP frames; PWP_Codec.hpp encodes and
 *          decodes both.
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

#ifndef PUMPKIN_PWP_MESSAGE_HPP_
#define PUMPKIN_PWP_MESSAGE_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pumpkin::services {

/*
   Binary encoding inside a PBWP frame. All multi-byte integers are little-endian.
   The frame's type byte holds the PwpOpcode (request) or PwpReplyType (reply).

   Request payload:
   +-----------+------------+-----------------+------------+-----------------+-----
   | Arg count | Arg 1 len  | Arg 1 bytes     | Arg 2 len  | Arg 2 bytes     | ...
   | u16       | u32        | Arg 1 len bytes | u32        | Arg 2 len bytes |
   +-----------+------------+-----------------+------------+-----------------+-----

   Reply payload, by reply type:
     RPL_OK, RPL_NIL, RPL_PONG : empty (payload length 0)
     RPL_VALUE                 : the value bytes
     RPL_INTEGER               : 8-byte signed integer
     RPL_ERROR                 : u8 status (P_E* code), then the error message text

   Example: SET a 1 with request id 7 (24 bytes)
     B1 01 03 00 | 07 00 00 00 | 0C 00 00 00 | 02 00 | 01 00 00 00 61 | 01 00 00 00 31
     PBWP header                             | argc  | key "a"        | value "1"

   Example: error P_EINVARG "bad" for request id 4 (16 bytes)
     B1 01 04 00 | 04 00 00 00 | 04 00 00 00 | 03     | 62 61 64
     PBWP header                             | status | message "bad"
*/

inline constexpr size_t PWP_MAX_KEY_SIZE = 64 * 1024;  // 64 KiB

/**
 * @brief Request opcodes (PBWP type byte of a request). Never renumber, only add.
 */
enum class PwpOpcode : uint8_t {
    OP_INVALID = 0x00,
    OP_PING    = 0x01,
    OP_GET     = 0x02,
    OP_SET     = 0x03,
    OP_DEL     = 0x04,
};

/**
 * @brief Reply types (PBWP type byte of a reply). Never renumber, only add.
 */
enum class PwpReplyType : uint8_t {
    RPL_OK      = 0x00,  // no payload
    RPL_VALUE   = 0x01,  // payload: the value bytes
    RPL_NIL     = 0x02,  // no payload
    RPL_INTEGER = 0x03,  // payload: 8-byte signed integer
    RPL_ERROR   = 0x04,  // payload: u8 status (P_E* code), then the error message text
    RPL_PONG    = 0x05,  // no payload; the reply to OP_PING
};

/**
 * @brief One request from a client. Text requests have request id 0.
 */
struct PwpRequest {
    PwpOpcode                enm_Opcode    = PwpOpcode::OP_INVALID;
    uint32_t                 u32_RequestId = 0;
    std::vector<std::string> vec_Args;
};

/**
 * @brief One reply to a request; u32_RequestId is copied from the request.
 */
struct PwpReply {
    PwpReplyType enm_Type      = PwpReplyType::RPL_OK;
    uint8_t      u8_Status     = 0;  // P_E* code for RPL_ERROR, P_OK otherwise
    uint32_t     u32_RequestId = 0;
    std::string  sz_Payload;       // value for RPL_VALUE, message for RPL_ERROR
    int64_t      s64_Integer = 0;  // RPL_INTEGER
};

}  // namespace pumpkin::services

#endif  // PUMPKIN_PWP_MESSAGE_HPP_
