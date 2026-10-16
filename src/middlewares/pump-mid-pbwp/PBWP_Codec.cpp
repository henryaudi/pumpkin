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

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"

LOG_MODULE_REGISTER(PBWP, LOG_LEVEL_DBG)

namespace pumpkin::middlewares {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

/* Byte offsets inside the header */
constexpr size_t PBWP_OFFSET_MAGIC       = 0;
constexpr size_t PBWP_OFFSET_VERSION     = 1;
constexpr size_t PBWP_OFFSET_TYPE        = 2;
constexpr size_t PBWP_OFFSET_FLAGS       = 3;
constexpr size_t PBWP_OFFSET_REQ_ID      = 4;
constexpr size_t PBWP_OFFSET_PAYLOAD_LEN = 8;

// ---- Writing: into a byte buffer that the caller already checked is large enough -------------

/**
 * @brief Write a 16-bit little-endian integer to the destination buffer.
 *
 * @param ptr_Dest Pointer to the destination buffer (must have at least 2 bytes).
 * @param u16_Value The 16-bit value to write.
 */
void pbwp_putLe16(uint8_t* ptr_Dest, uint16_t u16_Value) {
    ptr_Dest[0] = static_cast<uint8_t>(u16_Value & 0xFF);
    ptr_Dest[1] = static_cast<uint8_t>((u16_Value >> 8) & 0xFF);
}

/**
 * @brief Write a 32-bit little-endian integer to the destination buffer.
 *
 * @param ptr_Dest Pointer to the destination buffer (must have at least 4 bytes).
 * @param u32_Value The 32-bit value to write.
 */
void pbwp_putLe32(uint8_t* ptr_Dest, uint32_t u32_Value) {
    pbwp_putLe16(ptr_Dest, static_cast<uint16_t>(u32_Value & 0xFFFF));
    pbwp_putLe16(ptr_Dest + 2, static_cast<uint16_t>((u32_Value >> 16) & 0xFFFF));
}

/**
 * @brief Write a 64-bit little-endian integer to the destination buffer.
 *
 * @param ptr_Dest Pointer to the destination buffer (must have at least 8 bytes).
 * @param u64_Value The 64-bit value to write.
 */
void pbwp_putLe64(uint8_t* ptr_Dest, uint64_t u64_Value) {
    pbwp_putLe32(ptr_Dest, static_cast<uint32_t>(u64_Value & 0xFFFFFFFF));
    pbwp_putLe32(ptr_Dest + 4, static_cast<uint32_t>((u64_Value >> 32) & 0xFFFFFFFF));
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
void pbwp_writeHeader(uint8_t* ptr_Out, uint8_t u8_Type, uint8_t u8_Flags, uint32_t u32_RequestId,
                      uint32_t u32_PayloadLength) {
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
int pbwp_checkOutput(size_t siz_FrameSize, const uint8_t* ptr_Out, size_t siz_Capacity) {
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
uint16_t pbwp_getLe16(const uint8_t* ptr_In) {
    return static_cast<uint16_t>((ptr_In[1] << 8) | ptr_In[0]);
}

/**
 * @brief Read a 32-bit little-endian integer from the input buffer.
 *
 * @param ptr_In Pointer to the input buffer (must have at least 4 bytes).
 * @return The 32-bit value read.
 */
uint32_t pbwp_getLe32(const uint8_t* ptr_In) {
    return (static_cast<uint32_t>(pbwp_getLe16(ptr_In + 2)) << 16) |
           (static_cast<uint32_t>(pbwp_getLe16(ptr_In)));
}

/**
 * @brief Read a 64-bit little-endian integer from the input buffer.
 *
 * @param ptr_In Pointer to the input buffer (must have at least 8 bytes).
 * @return The 64-bit value read.
 */
uint64_t pbwp_getLe64(const uint8_t* ptr_In) {
    return (static_cast<uint64_t>(pbwp_getLe32(ptr_In + 4)) << 32) |
           (static_cast<uint64_t>(pbwp_getLe32(ptr_In)));
}

int pbwp_parseHeader(const uint8_t* ptr_In, size_t siz_Length) {
    if (siz_Length < PBWP_HEADER_SIZE) {
        return 0;  // Incomplete frame header.
    }
    if (ptr_In[PBWP_OFFSET_MAGIC] != PBWP_MAGIC || ptr_In[PBWP_OFFSET_VERSION] != PBWP_VERSION) {
        LOG_WRN("bad header: magic 0x%02X, version %u (expected 0x%02X, %u)",
                static_cast<unsigned>(ptr_In[PBWP_OFFSET_MAGIC]),
                static_cast<unsigned>(ptr_In[PBWP_OFFSET_VERSION]),
                static_cast<unsigned>(PBWP_MAGIC), static_cast<unsigned>(PBWP_VERSION));
        return -P_EINVARG;
    }

    if constexpr (PBWP_VERSION < 2) {  // I swear if this was C project I would have used macros...
        /* Flags byte is temporary reserved in version 1: must be 0 */
        LOG_WRN("flags 0x%02X not supported in version %u",
                static_cast<unsigned>(ptr_In[PBWP_OFFSET_FLAGS]),
                static_cast<unsigned>(PBWP_VERSION));
        if (ptr_In[PBWP_OFFSET_FLAGS] != 0) {
            return -P_EINVARG;
        }
    }

    const uint32_t u32_PayloadLength = pbwp_getLe32(ptr_In + PBWP_OFFSET_PAYLOAD_LEN);
    if (u32_PayloadLength > PBWP_MAX_PAYLOAD_SIZE) {
        LOG_WRN("payload length %u over the %zu-byte limit", u32_PayloadLength,
                PBWP_MAX_PAYLOAD_SIZE);
        return -P_ERANGE;
    }
    if (siz_Length < PBWP_HEADER_SIZE + u32_PayloadLength) {
        return 0;  // Incomplete frame.
    }

    return static_cast<int>(PBWP_HEADER_SIZE + u32_PayloadLength);
}
}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
int PBWP_EncodeRequest(const PbwpRequest& str_Request, uint8_t* ptr_Out, size_t siz_Capacity) {
    if (str_Request.vec_Args.size() > std::numeric_limits<uint16_t>::max()) {
        LOG_WRN("request %u: %zu arguments, more than the 16-bit count allows",
                str_Request.u32_RequestId, str_Request.vec_Args.size());
        return -P_ERANGE;
    }

    /* Payload: 2-byte argument count, then per argument a 4-byte length and its bytes */
    size_t siz_PayloadNumBytes = sizeof(uint16_t);  // Argument count field
    for (const std::string& sz_Arg : str_Request.vec_Args) {
        siz_PayloadNumBytes += sizeof(uint32_t) + sz_Arg.size();  // len (32-bit) + Arg bytes
    }
    if (siz_PayloadNumBytes > PBWP_MAX_PAYLOAD_SIZE) {
        LOG_WRN("request %u: payload %zu bytes over the %zu-byte limit", str_Request.u32_RequestId,
                siz_PayloadNumBytes, PBWP_MAX_PAYLOAD_SIZE);
        return -P_ERANGE;
    }

    const size_t siz_FrameSize = PBWP_HEADER_SIZE + siz_PayloadNumBytes;
    const int    int_Ret       = pbwp_checkOutput(siz_FrameSize, ptr_Out, siz_Capacity);
    if (int_Ret != 0) {
        if (int_Ret < 0) {
            LOG_DBG("request %u: cannot encode %zu bytes into %zu: %s", str_Request.u32_RequestId,
                    siz_FrameSize, siz_Capacity, STAT_ToString(-int_Ret));
        }
        return int_Ret;
    }

    /* Write header */
    pbwp_writeHeader(ptr_Out, static_cast<uint8_t>(str_Request.enm_Opcode), 0,
                     str_Request.u32_RequestId, static_cast<uint32_t>(siz_PayloadNumBytes));

    /* Write argument count + payload */
    size_t siz_Pos = PBWP_HEADER_SIZE;
    pbwp_putLe16(ptr_Out + siz_Pos, static_cast<uint16_t>(str_Request.vec_Args.size()));
    siz_Pos += sizeof(uint16_t);
    for (const std::string& sz_Arg : str_Request.vec_Args) {
        pbwp_putLe32(ptr_Out + siz_Pos, static_cast<uint32_t>(sz_Arg.size()));
        siz_Pos += sizeof(uint32_t);
        std::memcpy(ptr_Out + siz_Pos, sz_Arg.data(), sz_Arg.size());
        siz_Pos += sz_Arg.size();
    }

    return static_cast<int>(siz_FrameSize);
}
}  // namespace pumpkin::middlewares
