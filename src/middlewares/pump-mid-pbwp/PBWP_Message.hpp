/**
 ******************************************************************************
 * @file    PBWP_Message.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Binary Wire Protocol (PBWP): frame format definitions.
 *          This file declares:
 *           + Wire constants: magic, version, header size, limits, flags
 *           + PbwpHeader: the fields of a frame header
 *          A frame is a 12-byte header (little-endian), then a payload of payload-length bytes.
 *          PBWP does not look inside the payload: what the type byte and the payload mean is up
 *          to the protocol that uses PBWP (PWP).
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

#ifndef PUMPKIN_PBWP_MESSAGE_HPP_
#define PUMPKIN_PBWP_MESSAGE_HPP_

#include <cstddef>
#include <cstdint>

namespace pumpkin::middlewares {

/*
   PBWP frame layout. All multi-byte integers are little-endian.

   Header (12 bytes):
   +---------+---------+---------+---------+-------------------+-------------------+
   | Byte 0  | Byte 1  | Byte 2  | Byte 3  | Bytes 4-7         | Bytes 8-11        |
   +---------+---------+---------+---------+-------------------+-------------------+
   | Magic   | Version | Type    | Flags   | Request id        | Payload length    |
   | 0xB1    | 1       | set by  | PBWP_   | u32, chosen by    | u32, bytes after  |
   |         |         | the user| FLAG_*  | the client, copied| the header        |
   |         |         | protocol|         | into the reply    |                   |
   +---------+---------+---------+---------+-------------------+-------------------+

   Flags: bit 0 = PBWP_FLAG_MORE (reserved for chunked messages); bits 1-7 reserved.

   Payload (starts at byte 12): payload-length bytes, opaque to PBWP. PWP_Message.hpp
   describes what PWP puts there.
*/

/* NOTE: For future design changes, the magic byte must be greater than all opcodes */
inline constexpr uint8_t PBWP_MAGIC            = 0xB1;
inline constexpr uint8_t PBWP_VERSION          = 1;
inline constexpr size_t  PBWP_HEADER_SIZE      = 12;                // 12 Byte
inline constexpr size_t  PBWP_MAX_PAYLOAD_SIZE = 16 * 1024 * 1024;  // 16 MiB

/* Header byte 3. Reserved in version 1: frames with any flag set are rejected */
inline constexpr uint8_t PBWP_FLAG_MORE = 0x01;  // More frames of this message follow

struct PbwpHeader {
    uint8_t  u8_Magic          = 0;
    uint8_t  u8_Version        = 0;
    uint8_t  u8_Type           = 0;
    uint8_t  u8_Flags          = 0;
    uint32_t u32_RequestId     = 0;
    uint32_t u32_PayloadLength = 0;
};

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PBWP_MESSAGE_HPP_
