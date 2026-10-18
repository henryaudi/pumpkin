/**
 ******************************************************************************
 * @file    PBWP_Frame.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Binary Wire Protocol (PBWP): wire format definitions.
 *          This file declares:
 *           + Frame constants: magic, version, header size, limits
 *           + PbwpOpcode / PbwpReplyType: the values carried in the header
 *           + PbwpRequest / PbwpReply: decoded frames
 *          Frame: 12-byte header (little-endian), then a body of body-length bytes.
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

#ifndef PUMPKIN_PBWP_FRAME_HPP_
#define PUMPKIN_PBWP_FRAME_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pumpkin::middlewares {

/*
   PBWP frame layout. All multi-byte integers are little-endian.

   Header (12 bytes), the same for requests and replies:
   +---------+---------+---------+---------+---------+---------+---------+---------+
   | Byte 0  | Byte 1  | Byte 2  | Byte 3  | Bytes 4-7         | Bytes 8-11        |
   +---------+---------+---------+---------+-------------------+-------------------+
   | Magic   | Version | Type    | Flags   | Request id        | Payload length    |
   | 0xB1    | 1       | OP_* or | PBWP_   | u32, chosen by    | u32, bytes after  |
   |         |         | RPL_*   | FLAG_*  | the client, copied| the header        |
   |         |         |         |         | into the reply    |                   |
   +---------+---------+---------+---------+-------------------+-------------------+

   Flags: bit 0 = PBWP_FLAG_MORE (reserved for chunked messages); bits 1-7 reserved.

   Request payload (starts at byte 12):
   +-----------+------------+-----------------+------------+-----------------+-----
   | Arg count | Arg 1 len  | Arg 1 bytes     | Arg 2 len  | Arg 2 bytes     | ...
   | u16       | u32        | Arg 1 len bytes | u32        | Arg 2 len bytes |
   +-----------+------------+-----------------+------------+-----------------+-----

   Reply payload (starts at byte 12), by reply type:
     RPL_OK, RPL_NIL : empty (payload length 0)
     RPL_VALUE       : the value bytes
     RPL_INTEGER     : 8-byte signed integer
     RPL_ERROR       : u8 status (P_E* code), then the error message text

   Example: SET a 1 with request id 7 (24 bytes)
     B1 01 03 00 | 07 00 00 00 | 0C 00 00 00 | 02 00 | 01 00 00 00 61 | 01 00 00 00 31
     header                                  | argc  | key "a"        | value "1"

   Example: error P_EINVARG "bad" for request id 4 (16 bytes)
     B1 01 04 00 | 04 00 00 00 | 04 00 00 00 | 03     | 62 61 64
     header                                  | status | message "bad"
*/

/* NOTE: For future design changes, the magic byte must be greater than all opcodes */
inline constexpr uint8_t PBWP_MAGIC            = 0xB1;
inline constexpr uint8_t PBWP_VERSION          = 1;
inline constexpr size_t  PBWP_HEADER_SIZE      = 12;                // 12 Byte
inline constexpr size_t  PBWP_MAX_PAYLOAD_SIZE = 16 * 1024 * 1024;  // 16 MiB
inline constexpr size_t  PBWP_MAX_KEY_SIZE     = 64 * 1024;         // 64 KiB

/* Header byte 3. Reserved in version 1: frames with any flag set are rejected */
inline constexpr uint8_t PBWP_FLAG_MORE = 0x01;  // More frames of this message follow

/**
 * @brief Request opcodes (header byte 2). Values are fixed: never renumber, only add.
 */
enum class PbwpOpcode : uint8_t {
    OP_INVALID = 0x00,
    OP_PING    = 0x01,
    OP_GET     = 0x02,
    OP_SET     = 0x03,
    OP_DEL     = 0x04,
};

/**
 * @brief Reply types (header byte 2 of a reply). Values are fixed: never renumber, only add.
 */
enum class PbwpReplyType : uint8_t {
    RPL_OK      = 0x00,  // no payload
    RPL_VALUE   = 0x01,  // payload: the value bytes
    RPL_NIL     = 0x02,  // no payload
    RPL_INTEGER = 0x03,  // payload: 8-byte signed integer
    RPL_ERROR   = 0x04,  // payload: u8 status (P_E* code), then the error message text
};

/**
 * @brief A decoded request frame.
 */
struct PbwpRequest {
    PbwpOpcode               enm_Opcode    = PbwpOpcode::OP_INVALID;
    uint32_t                 u32_RequestId = 0;
    std::vector<std::string> vec_Args;
};

struct PbwpReply {
    PbwpReplyType enm_Type      = PbwpReplyType::RPL_OK;
    uint8_t       u8_Status     = 0;  // P_E* code for RPL_ERROR, P_OK otherwise
    uint32_t      u32_RequestId = 0;
    std::string   sz_Payload;
    int64_t       s64_Integer = 0;
};

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PBWP_FRAME_HPP_
