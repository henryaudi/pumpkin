/**
 ******************************************************************************
 * @file    PBWP_CodecTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the Pumpkin Binary Wire Protocol (PBWP) frame access.
 *          This file tests:
 *           + PBWP_ValidateFrame: complete, partial, several and broken frames, flags
 *           + PBWP_GetHeader and the single-field getters
 *           + PBWP_SetHeader against the exact bytes of a header
 *           + Little-endian helpers
 *          The payload is opaque here: what it means is tested in PWP_CodecTest.cpp.
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

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/pump-cmn-stat/STAT_Status.hpp"

namespace pumpkin::middlewares {
namespace {

/* A 24-byte frame: type 0x03, request id 7, 12 payload bytes that PBWP does not interpret */
constexpr std::array<uint8_t, 24> PBWP_TEST_FRAME = {
    0xB1, 0x01, 0x03, 0x00,  // magic, version, type, flags
    0x07, 0x00, 0x00, 0x00,  // request id 7
    0x0C, 0x00, 0x00, 0x00,  // payload length 12
    0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 'a', 0x01, 0x00, 0x00, 0x00, '1',  // payload
};

// ---- ValidateFrame -----------------------------------------------------------------------------

TEST(PbwpFrame, CompleteFrameReturnsItsSize) {
    EXPECT_EQ(PBWP_ValidateFrame(PBWP_TEST_FRAME.data(), PBWP_TEST_FRAME.size()), 24);
}

TEST(PbwpFrame, PartialFrameNeedsMoreBytes) {
    EXPECT_EQ(PBWP_ValidateFrame(PBWP_TEST_FRAME.data(), 11), 0);  // header
    EXPECT_EQ(PBWP_ValidateFrame(PBWP_TEST_FRAME.data(), 20), 0);  // payload
    EXPECT_EQ(PBWP_ValidateFrame(nullptr, 0), 0);                  // empty
}

TEST(PbwpFrame, OnlyTheFirstOfTwoFramesCounts) {
    std::vector<uint8_t> vec_TwoFrames(PBWP_TEST_FRAME.begin(), PBWP_TEST_FRAME.end());
    vec_TwoFrames.insert(vec_TwoFrames.end(), PBWP_TEST_FRAME.begin(), PBWP_TEST_FRAME.end());
    EXPECT_EQ(PBWP_ValidateFrame(vec_TwoFrames.data(), vec_TwoFrames.size()), 24);
}

TEST(PbwpFrame, BrokenHeadersAreRejected) {
    std::array<uint8_t, 24> arr_BadMagic = PBWP_TEST_FRAME;
    arr_BadMagic[0]                      = 0x50;  // 'P'
    EXPECT_EQ(PBWP_ValidateFrame(arr_BadMagic.data(), arr_BadMagic.size()), -P_EINVARG);

    std::array<uint8_t, 24> arr_BadVersion = PBWP_TEST_FRAME;
    arr_BadVersion[1]                      = 0x02;
    EXPECT_EQ(PBWP_ValidateFrame(arr_BadVersion.data(), arr_BadVersion.size()), -P_EINVARG);

    std::array<uint8_t, 24> arr_TooLarge = PBWP_TEST_FRAME;  // payload length 0x01000001
    arr_TooLarge[8]                      = 0x01;             // = 16 MiB + 1
    arr_TooLarge[11]                     = 0x01;
    EXPECT_EQ(PBWP_ValidateFrame(arr_TooLarge.data(), arr_TooLarge.size()), -P_ERANGE);

    EXPECT_EQ(PBWP_ValidateFrame(nullptr, 24), -P_EINVARG);
}

TEST(PbwpFrame, AnyFlagSetIsRejectedInVersion1) {
    std::array<uint8_t, 24> arr_More = PBWP_TEST_FRAME;
    arr_More[3]                      = PBWP_FLAG_MORE;  // reserved, not supported yet
    EXPECT_EQ(PBWP_ValidateFrame(arr_More.data(), arr_More.size()), -P_EINVARG);

    std::array<uint8_t, 24> arr_Unknown = PBWP_TEST_FRAME;
    arr_Unknown[3]                      = 0x80;  // a bit no version defines yet
    EXPECT_EQ(PBWP_ValidateFrame(arr_Unknown.data(), arr_Unknown.size()), -P_EINVARG);
}

// ---- Reading the header ------------------------------------------------------------------------

TEST(PbwpHeader, GetHeaderReadsEveryField) {
    PbwpHeader str_Header;
    ASSERT_EQ(PBWP_GetHeader(PBWP_TEST_FRAME.data(), PBWP_TEST_FRAME.size(), &str_Header), P_OK);
    EXPECT_EQ(str_Header.u8_Magic, PBWP_MAGIC);
    EXPECT_EQ(str_Header.u8_Version, PBWP_VERSION);
    EXPECT_EQ(str_Header.u8_Type, 0x03);
    EXPECT_EQ(str_Header.u8_Flags, 0x00);
    EXPECT_EQ(str_Header.u32_RequestId, 7u);
    EXPECT_EQ(str_Header.u32_PayloadLength, 12u);
}

TEST(PbwpHeader, GetHeaderNeedsTwelveBytes) {
    PbwpHeader str_Header;
    EXPECT_EQ(PBWP_GetHeader(PBWP_TEST_FRAME.data(), 11, &str_Header), -P_ERANGE);
    EXPECT_EQ(PBWP_GetHeader(nullptr, 24, &str_Header), -P_EINVARG);
    EXPECT_EQ(PBWP_GetHeader(PBWP_TEST_FRAME.data(), 24, nullptr), -P_EINVARG);
}

TEST(PbwpHeader, SingleFieldGettersMatchTheHeader) {
    const uint8_t* ptr_Frame = PBWP_TEST_FRAME.data();
    EXPECT_EQ(PBWP_GetMagic(ptr_Frame), PBWP_MAGIC);
    EXPECT_EQ(PBWP_GetVersion(ptr_Frame), PBWP_VERSION);
    EXPECT_EQ(PBWP_GetType(ptr_Frame), 0x03);
    EXPECT_EQ(PBWP_GetFlags(ptr_Frame), 0x00);
    EXPECT_EQ(PBWP_GetRequestId(ptr_Frame), 7u);
    EXPECT_EQ(PBWP_GetPayloadLength(ptr_Frame), 12u);
    EXPECT_EQ(PBWP_GetPayload(ptr_Frame), ptr_Frame + PBWP_HEADER_SIZE);
}

// ---- Writing the header ------------------------------------------------------------------------

TEST(PbwpHeader, SetHeaderWritesTheExactBytes) {
    std::array<uint8_t, 12> arr_Out{};
    ASSERT_EQ(PBWP_SetHeader(arr_Out.data(), arr_Out.size(), 0x03, 0, 7, 12),
              static_cast<int>(PBWP_HEADER_SIZE));
    for (size_t siz_Idx = 0; siz_Idx < PBWP_HEADER_SIZE; siz_Idx++) {
        EXPECT_EQ(arr_Out[siz_Idx], PBWP_TEST_FRAME[siz_Idx]) << "byte " << siz_Idx;
    }
}

TEST(PbwpHeader, SetHeaderRejectsBadInput) {
    std::array<uint8_t, 12> arr_Out{};
    EXPECT_EQ(PBWP_SetHeader(arr_Out.data(), 11, 0x03, 0, 7, 12), -P_ERANGE);  // too small
    EXPECT_EQ(PBWP_SetHeader(arr_Out.data(), arr_Out.size(), 0x03, 0, 7,
                             static_cast<uint32_t>(PBWP_MAX_PAYLOAD_SIZE + 1)),
              -P_ERANGE);
    EXPECT_EQ(PBWP_SetHeader(arr_Out.data(), arr_Out.size(), 0x03, PBWP_FLAG_MORE, 7, 12),
              -P_EINVARG);
    EXPECT_EQ(PBWP_SetHeader(nullptr, 12, 0x03, 0, 7, 12), -P_EINVARG);
    EXPECT_EQ(arr_Out[0], 0);  // nothing was written
}

// ---- Little-endian helpers ---------------------------------------------------------------------

TEST(PbwpLittleEndian, RoundTripsKeepEveryBit) {
    std::array<uint8_t, 8> arr_Buf{};

    PBWP_PutLe16(arr_Buf.data(), 0xBEEF);
    EXPECT_EQ(arr_Buf[0], 0xEF);  // low byte first
    EXPECT_EQ(PBWP_GetLe16(arr_Buf.data()), 0xBEEF);

    PBWP_PutLe32(arr_Buf.data(), 0xDEADBEEF);
    EXPECT_EQ(PBWP_GetLe32(arr_Buf.data()), 0xDEADBEEFu);

    PBWP_PutLe64(arr_Buf.data(), 0xFFFFFFFFFFFFFFFEull);  // -2: the high half must survive
    EXPECT_EQ(PBWP_GetLe64(arr_Buf.data()), 0xFFFFFFFFFFFFFFFEull);
}

}  // namespace
}  // namespace pumpkin::middlewares
