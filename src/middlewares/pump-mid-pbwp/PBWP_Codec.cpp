/**
 ******************************************************************************
 * @file    PBWP_Codec.cpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Binary Wire Protocol (PBWP) codec: implementation.
 *          This file provides functions to:
 *           + Read / write little-endian integers byte by byte
 *           + Encode and decode request and reply frames
 *           + Validate requests and convert them into Commands
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

#include "middlewares/pump-mid-pbwp/PBWP_Codec.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "common/pump-cmn-stat/STAT_Status.hpp"

namespace pumpkin::middlewares {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

/* Byte offsets inside the header */
constexpr std::size_t PBWP_OFFSET_MAGIC       = 0;
constexpr std::size_t PBWP_OFFSET_VERSION     = 1;
constexpr std::size_t PBWP_OFFSET_TYPE        = 2;
constexpr std::size_t PBWP_OFFSET_FLAGS       = 3;
constexpr std::size_t PBWP_OFFSET_REQ_ID      = 4;
constexpr std::size_t PBWP_OFFSET_PAYLOAD_LEN = 8;

// ---- Writing: into a byte buffer that the caller already checked is large enough -------------

/**
 * @brief Write a 16-bit little-endian integer to the destination buffer.
 *
 * @param ptr_Dest Pointer to the destination buffer (must have at least 2 bytes).
 * @param u16_Value The 16-bit value to write.
 */
void pbwp_putLe16(std::uint8_t* ptr_Dest, std::uint16_t u16_Value) {
    ptr_Dest[0] = static_cast<std::uint8_t>(u16_Value & 0xFF);
    ptr_Dest[1] = static_cast<std::uint8_t>((u16_Value >> 8) & 0xFF);
}

/**
 * @brief Write a 32-bit little-endian integer to the destination buffer.
 *
 * @param ptr_Dest Pointer to the destination buffer (must have at least 4 bytes).
 * @param u32_Value The 32-bit value to write.
 */
void pbwp_putLe32(std::uint8_t* ptr_Dest, std::uint32_t u32_Value) {
    pbwp_putLe16(ptr_Dest, static_cast<std::uint16_t>(u32_Value & 0xFFFF));
    pbwp_putLe16(ptr_Dest + 2, static_cast<std::uint16_t>((u32_Value >> 16) & 0xFFFF));
}

/**
 * @brief Write a 64-bit little-endian integer to the destination buffer.
 *
 * @param ptr_Dest Pointer to the destination buffer (must have at least 8 bytes).
 * @param u64_Value The 64-bit value to write.
 */
void pbwp_putLe64(std::uint8_t* ptr_Dest, std::uint64_t u64_Value) {
    pbwp_putLe32(ptr_Dest, static_cast<std::uint32_t>(u64_Value & 0xFFFFFFFF));
    pbwp_putLe32(ptr_Dest + 4, static_cast<std::uint32_t>((u64_Value >> 32) & 0xFFFFFFFF));
}

/**
 * @brief Writes the 12-byte header at ptr_Out[0..11].
 *
 * @param ptr_Out           Start of the frame.
 * @param u8_Type           Opcode (request) or reply type (reply).
 * @param u8_Flags          PBWP_FLAG_* bits; always 0 in version 1.
 * @param u32_RequestId     Request id.
 * @param u32_PayloadLength Number of payload bytes that follow.
 */
void pbwp_writeHeader(std::uint8_t* ptr_Out, std::uint8_t u8_Type, std::uint8_t u8_Flags,
                      std::uint32_t u32_RequestId, std::uint32_t u32_PayloadLength) {
    ptr_Out[PBWP_OFFSET_MAGIC]   = PBWP_MAGIC;
    ptr_Out[PBWP_OFFSET_VERSION] = PBWP_VERSION;
    ptr_Out[PBWP_OFFSET_TYPE]    = u8_Type;
    ptr_Out[PBWP_OFFSET_FLAGS]   = u8_Flags;
    pbwp_putLe32(ptr_Out + PBWP_OFFSET_REQ_ID, u32_RequestId);
    pbwp_putLe32(ptr_Out + PBWP_OFFSET_PAYLOAD_LEN, u32_PayloadLength);
}

/**
 * @brief Common checks before encoding: size query, NULL buffer, capacity.
 *
 * @return 0 if encoding may go ahead; otherwise the value the encoder must return: the frame size
 *         for a size query, -P_EINVARG for a NULL buffer, -P_ERANGE if it doesn't fit.
 */
int pbwp_checkOutput(std::size_t siz_FrameSize, const std::uint8_t* ptr_Out,
                     std::size_t siz_Capacity) {
    if (ptr_Out == nullptr) {
        return (siz_Capacity == 0) ? static_cast<int>(siz_FrameSize) : -P_EINVARG;
    }
    if (siz_Capacity < siz_FrameSize) {
        return -P_ERANGE;
    }
    return 0;
}

// ---- Reading: from received bytes that the caller already checked are there ------------------
/**
 * @brief Read a 16-bit little-endian integer from the input buffer.
 *
 * @param ptr_In Pointer to the input buffer (must have at least 2 bytes).
 * @return The 16-bit value read.
 */
std::uint16_t pbwp_getLe16(const std::uint8_t* ptr_In) {
    return static_cast<std::uint16_t>((ptr_In[1] << 8) | ptr_In[0]);
}

/**
 * @brief Read a 32-bit little-endian integer from the input buffer.
 *
 * @param ptr_In Pointer to the input buffer (must have at least 4 bytes).
 * @return The 32-bit value read.
 */
std::uint32_t pbwp_getLe32(const std::uint8_t* ptr_In) {
    return static_cast<std::uint32_t>((pbwp_getLe16(ptr_In + 2) << 16) | pbwp_getLe16(ptr_In));
}

/**
 * @brief Read a 64-bit little-endian integer from the input buffer.
 *
 * @param ptr_In Pointer to the input buffer (must have at least 8 bytes).
 * @return The 64-bit value read.
 */
std::uint64_t pbwp_getLe64(const std::uint8_t* ptr_In) {
    return static_cast<std::uint64_t>((pbwp_getLe32(ptr_In + 4) << 32) | pbwp_getLe32(ptr_In));
}



}  // namespace

}  // namespace pumpkin::middlewares
