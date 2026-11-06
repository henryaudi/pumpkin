/**
 ******************************************************************************
 * @file    PBWP_Codec.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Binary Wire Protocol (PBWP) frame access: public interface.
 *          This file declares functions to:
 *           + Check whether received bytes start with a complete, valid frame
 *           + Read the header (whole, or one field at a time) and find the payload
 *           + Write a header in front of a payload
 *           + Read / write little-endian integers, the wire byte order
 *          Encoding and decoding what is inside the payload is the user protocol's job (PWP).
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

#ifndef PUMPKIN_PBWP_CODEC_HPP_
#define PUMPKIN_PBWP_CODEC_HPP_

#include <cstdint>

#include "middlewares/pump-mid-pbwp/PBWP_Message.hpp"

namespace pumpkin::middlewares {

/**
 * @brief Validates the header at ptr_Src[0] and whether the whole frame has arrived.
 *
 * @details Checks magic, version, flags (all must be 0 in version 1) and the payload limit. Does
 *          not look inside the payload. The buffer is not modified.
 *
 * @param ptr_Src     Received bytes; may hold a partial frame, or several frames.
 * @param siz_Length  Number of bytes at ptr_Src.
 *
 * @return Frame size in bytes (> 0) once header and payload have fully arrived. 0 if more bytes
 *         are needed. -P_EINVARG for a wrong magic or version, any flag set, or a NULL ptr_Src with
 *         a non-zero length; -P_ERANGE for a payload over PBWP_MAX_PAYLOAD_SIZE. On a negative
 *         result the stream can't be trusted any more, so the caller closes the connection.
 */
int PBWP_ValidateFrame(const uint8_t* ptr_Src, size_t siz_Length);

/**
 * @brief Reads all header fields as they are; no validation is performed here.
 *
 * @param ptr_Src     Received bytes; must hold at least PBWP_HEADER_SIZE bytes.
 * @param siz_Length  Number of bytes at ptr_Src.
 * @param ptr_Header  Pointer to a PbwpHeader structure where the header fields will be stored.
 *
 * @return P_OK on success, -P_ERANGE if fewer than PBWP_HEADER_SIZE bytes; -P_EINVARG for a NULL
 *         pointer.
 */
int PBWP_GetHeader(const uint8_t* ptr_Src, size_t siz_Length, PbwpHeader* ptr_Header);

/**
 * @brief Writes a 12-byte header at ptr_Dst[0..11]: PBWP_MAGIC, PBWP_VERSION and the given
 *        fields. The caller writes the u32_PayloadLength payload bytes right after it.
 *
 * @param ptr_Dst           Where the frame starts.
 * @param siz_Capacity      Bytes available at ptr_Dst.
 * @param u8_Type           Type byte, defined by the user protocol.
 * @param u8_Flags          PBWP_FLAG_* bits; must be 0 in version 1.
 * @param u32_RequestId     Request id.
 * @param u32_PayloadLength Number of payload bytes that follow the header.
 *
 * @return PBWP_HEADER_SIZE; -P_ERANGE if ptr_Dst has fewer than PBWP_HEADER_SIZE bytes or the
 *         payload is over PBWP_MAX_PAYLOAD_SIZE; -P_EINVARG for a NULL ptr_Dst or a flag set.
 *         Nothing is written on failure.
 */
int PBWP_SetHeader(uint8_t* ptr_Dst, size_t siz_Capacity, uint8_t u8_Type, uint8_t u8_Flags,
                   uint32_t u32_RequestId, uint32_t u32_PayloadLength);

// ---- Single header fields: ptr_Frame must hold at least PBWP_HEADER_SIZE bytes -----------------
uint8_t        PBWP_GetMagic(const uint8_t* ptr_Frame);
uint8_t        PBWP_GetVersion(const uint8_t* ptr_Frame);
uint8_t        PBWP_GetType(const uint8_t* ptr_Frame);
uint8_t        PBWP_GetFlags(const uint8_t* ptr_Frame);
uint32_t       PBWP_GetRequestId(const uint8_t* ptr_Frame);
uint32_t       PBWP_GetPayloadLength(const uint8_t* ptr_Frame);
const uint8_t* PBWP_GetPayload(const uint8_t* ptr_Frame);  // first byte after the header

// ---- Little-endian integers: the caller checks the bytes are there ------------------------------
void     PBWP_PutLe16(uint8_t* ptr_Dst, uint16_t u16_Value);
void     PBWP_PutLe32(uint8_t* ptr_Dst, uint32_t u32_Value);
void     PBWP_PutLe64(uint8_t* ptr_Dst, uint64_t u64_Value);
uint16_t PBWP_GetLe16(const uint8_t* ptr_Src);
uint32_t PBWP_GetLe32(const uint8_t* ptr_Src);
uint64_t PBWP_GetLe64(const uint8_t* ptr_Src);

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PBWP_CODEC_HPP_
