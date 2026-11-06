/**
 ******************************************************************************
 * @file    PBWP_Codec.cpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Binary Wire Protocol (PBWP) frame access: implementation.
 *          This file provides functions to:
 *           + Read / write little-endian integers byte by byte
 *           + Check a frame: magic, version, flags, sizes
 *           + Read and write the header fields
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

#include "middlewares/pump-mid-pbwp/PBWP_Codec.hpp"

#include <cstddef>
#include <cstdint>

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

}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================

// ---- Little-endian integers ---------------------------------------------------------------------

void PBWP_PutLe16(uint8_t* ptr_Dest, uint16_t u16_Value) {
    ptr_Dest[0] = static_cast<uint8_t>(u16_Value & 0xFF);
    ptr_Dest[1] = static_cast<uint8_t>((u16_Value >> 8) & 0xFF);
}

void PBWP_PutLe32(uint8_t* ptr_Dest, uint32_t u32_Value) {
    PBWP_PutLe16(ptr_Dest, static_cast<uint16_t>(u32_Value & 0xFFFF));
    PBWP_PutLe16(ptr_Dest + 2, static_cast<uint16_t>((u32_Value >> 16) & 0xFFFF));
}

void PBWP_PutLe64(uint8_t* ptr_Dest, uint64_t u64_Value) {
    PBWP_PutLe32(ptr_Dest, static_cast<uint32_t>(u64_Value & 0xFFFFFFFF));
    PBWP_PutLe32(ptr_Dest + 4, static_cast<uint32_t>((u64_Value >> 32) & 0xFFFFFFFF));
}

uint16_t PBWP_GetLe16(const uint8_t* ptr_In) {
    return static_cast<uint16_t>((ptr_In[1] << 8) | ptr_In[0]);
}

uint32_t PBWP_GetLe32(const uint8_t* ptr_In) {
    return (static_cast<uint32_t>(PBWP_GetLe16(ptr_In + 2)) << 16) |
           (static_cast<uint32_t>(PBWP_GetLe16(ptr_In)));
}

uint64_t PBWP_GetLe64(const uint8_t* ptr_In) {
    return (static_cast<uint64_t>(PBWP_GetLe32(ptr_In + 4)) << 32) |
           (static_cast<uint64_t>(PBWP_GetLe32(ptr_In)));
}

// ---- Single header fields ----------------------------------------------------------------------

uint8_t PBWP_GetMagic(const uint8_t* ptr_Frame) {
    return ptr_Frame[PBWP_OFFSET_MAGIC];
}

uint8_t PBWP_GetVersion(const uint8_t* ptr_Frame) {
    return ptr_Frame[PBWP_OFFSET_VERSION];
}

uint8_t PBWP_GetType(const uint8_t* ptr_Frame) {
    return ptr_Frame[PBWP_OFFSET_TYPE];
}

uint8_t PBWP_GetFlags(const uint8_t* ptr_Frame) {
    return ptr_Frame[PBWP_OFFSET_FLAGS];
}

uint32_t PBWP_GetRequestId(const uint8_t* ptr_Frame) {
    return PBWP_GetLe32(ptr_Frame + PBWP_OFFSET_REQ_ID);
}

uint32_t PBWP_GetPayloadLength(const uint8_t* ptr_Frame) {
    return PBWP_GetLe32(ptr_Frame + PBWP_OFFSET_PAYLOAD_LEN);
}

const uint8_t* PBWP_GetPayload(const uint8_t* ptr_Frame) {
    return ptr_Frame + PBWP_HEADER_SIZE;
}

// ---- Frames ------------------------------------------------------------------------------------

int PBWP_ValidateFrame(const uint8_t* ptr_Src, size_t siz_Length) {
    if (ptr_Src == nullptr && siz_Length > 0) {
        LOG_ERR("ValidateFrame called with a NULL pointer");
        return -P_EINVARG;
    }
    if (siz_Length < PBWP_HEADER_SIZE) {
        return 0;  // Incomplete frame header
    }
    if (PBWP_GetMagic(ptr_Src) != PBWP_MAGIC || PBWP_GetVersion(ptr_Src) != PBWP_VERSION) {
        LOG_WRN("bad header: magic 0x%02X, version %u (expected 0x%02X, %u)",
                static_cast<unsigned>(PBWP_GetMagic(ptr_Src)),
                static_cast<unsigned>(PBWP_GetVersion(ptr_Src)), static_cast<unsigned>(PBWP_MAGIC),
                static_cast<unsigned>(PBWP_VERSION));
        return -P_EINVARG;
    }
    if constexpr (PBWP_VERSION < 2) {  // I swear if this was C project I would have used macros...
        if (PBWP_GetFlags(ptr_Src) != 0) {
            /* Flags byte is temporary reserved in version 1: must be 0 */
            LOG_WRN("flags 0x%02X not supported in version %u",
                    static_cast<unsigned>(PBWP_GetFlags(ptr_Src)),
                    static_cast<unsigned>(PBWP_VERSION));
            return -P_EINVARG;
        }
    }

    const uint32_t u32_PayloadLength = PBWP_GetPayloadLength(ptr_Src);
    if (u32_PayloadLength > PBWP_MAX_PAYLOAD_SIZE) {
        LOG_WRN("payload length %u over the %zu-byte limit", u32_PayloadLength,
                PBWP_MAX_PAYLOAD_SIZE);
        return -P_ERANGE;
    }
    if (siz_Length < PBWP_HEADER_SIZE + u32_PayloadLength) {
        return 0;  // Incomplete frame
    }

    return static_cast<int>(PBWP_HEADER_SIZE + u32_PayloadLength);
}

int PBWP_GetHeader(const uint8_t* ptr_Src, size_t siz_Length, PbwpHeader* ptr_Header) {
    if (ptr_Src == nullptr || ptr_Header == nullptr) {
        LOG_ERR("GetHeader called with a NULL pointer");
        return -P_EINVARG;
    }
    if (siz_Length < PBWP_HEADER_SIZE) {
        return -P_ERANGE;
    }

    ptr_Header->u8_Magic          = PBWP_GetMagic(ptr_Src);
    ptr_Header->u8_Version        = PBWP_GetVersion(ptr_Src);
    ptr_Header->u8_Type           = PBWP_GetType(ptr_Src);
    ptr_Header->u8_Flags          = PBWP_GetFlags(ptr_Src);
    ptr_Header->u32_RequestId     = PBWP_GetRequestId(ptr_Src);
    ptr_Header->u32_PayloadLength = PBWP_GetPayloadLength(ptr_Src);

    return P_OK;
}

int PBWP_SetHeader(uint8_t* ptr_Dst, size_t siz_Capacity, uint8_t u8_Type, uint8_t u8_Flags,
                   uint32_t u32_RequestId, uint32_t u32_PayloadLength) {
    if (ptr_Dst == nullptr) {
        LOG_ERR("SetHeader called with a NULL pointer");
        return -P_EINVARG;
    }
    if (siz_Capacity < PBWP_HEADER_SIZE || u32_PayloadLength > PBWP_MAX_PAYLOAD_SIZE) {
        return -P_ERANGE;
    }
    if constexpr (PBWP_VERSION < 2) {
        if (u8_Flags != 0) {
            return -P_EINVARG;  // Version 1 stabs flags to 0
        }
    }

    ptr_Dst[PBWP_OFFSET_MAGIC] = PBWP_MAGIC;
    ptr_Dst[PBWP_OFFSET_VERSION] = PBWP_VERSION;
    ptr_Dst[PBWP_OFFSET_TYPE] = u8_Type;
    ptr_Dst[PBWP_OFFSET_FLAGS] = u8_Flags;
    PBWP_PutLe32(ptr_Dst + PBWP_OFFSET_REQ_ID, u32_RequestId);
    PBWP_PutLe32(ptr_Dst + PBWP_OFFSET_PAYLOAD_LEN, u32_PayloadLength);
    return static_cast<int>(PBWP_HEADER_SIZE);
}

}  // namespace pumpkin::middlewares
