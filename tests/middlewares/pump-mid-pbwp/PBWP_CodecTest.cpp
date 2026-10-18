/**
 ******************************************************************************
 * @file    PBWP_CodecTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the Pumpkin Binary Wire Protocol (PBWP) codec.
 *          This file tests:
 *           + Request encoding against the exact bytes of the spec example
 *           + Size queries and too-small buffers
 *           + Request decoding: partial frames, several frames, broken frames, flags
 *           + Request validation and conversion to Command
 *           + Reply encoding / decoding for every reply type, error status in the payload
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
#include <string>
#include <vector>

#include "common/pump-cmn-stat/STAT_Status.hpp"

namespace pumpkin::middlewares {
namespace {

/* "SET a 1" with request id 7: the worked example from PBWP_Frame.hpp (24 bytes) */
constexpr std::array<uint8_t, 24> PBWP_TEST_SET_A_1 = {
    0xB1, 0x01, 0x03, 0x00,  // magic, version, opcode SET, flags
    0x07, 0x00, 0x00, 0x00,  // request id 7
    0x0C, 0x00, 0x00, 0x00,  // payload length 12
    0x02, 0x00,              // 2 arguments
    0x01, 0x00, 0x00, 0x00,  // key length 1
    'a',                     // key
    0x01, 0x00, 0x00, 0x00,  // value length 1
    '1',                     // value
};

/**
 * @brief Builds the "SET a 1" request with request id 7.
 */
PbwpRequest pbwp_makeSetA1() {
    PbwpRequest str_Request;
    str_Request.enm_Opcode    = PbwpOpcode::OP_SET;
    str_Request.u32_RequestId = 7;
    str_Request.vec_Args      = {"a", "1"};
    return str_Request;
}

// ---- Requests: encoding ----------------------------------------------------------------------

TEST(PbwpRequest, EncodeMatchesSpecBytes) {
    std::array<uint8_t, 24> arr_Out{};  // exactly the frame size: tests a full fit
    ASSERT_EQ(PBWP_EncodeRequest(pbwp_makeSetA1(), arr_Out.data(), arr_Out.size()), 24);
    EXPECT_EQ(arr_Out, PBWP_TEST_SET_A_1);
}

TEST(PbwpRequest, SizeQueryAndTooSmallBuffer) {
    EXPECT_EQ(PBWP_EncodeRequest(pbwp_makeSetA1(), nullptr, 0), 24);  // size query

    std::array<uint8_t, 10> arr_Small{};
    EXPECT_EQ(PBWP_EncodeRequest(pbwp_makeSetA1(), arr_Small.data(), arr_Small.size()), -P_ERANGE);
    EXPECT_EQ(arr_Small[0], 0);  // nothing was written

    EXPECT_EQ(PBWP_EncodeRequest(pbwp_makeSetA1(), nullptr, 64), -P_EINVARG);
}

// ---- Requests: decoding ----------------------------------------------------------------------

TEST(PbwpRequest, DecodeReturnsFrameSizeAndFields) {
    PbwpRequest str_Request;
    ASSERT_EQ(PBWP_DecodeRequest(PBWP_TEST_SET_A_1.data(), PBWP_TEST_SET_A_1.size(), &str_Request),
              24);
    EXPECT_EQ(str_Request.enm_Opcode, PbwpOpcode::OP_SET);
    EXPECT_EQ(str_Request.u32_RequestId, 7u);
    EXPECT_EQ(str_Request.vec_Args, (std::vector<std::string>{"a", "1"}));
}

TEST(PbwpRequest, PartialFrameNeedsMoreBytes) {
    PbwpRequest str_Request;
    EXPECT_EQ(PBWP_DecodeRequest(PBWP_TEST_SET_A_1.data(), 11, &str_Request), 0);  // header
    EXPECT_EQ(PBWP_DecodeRequest(PBWP_TEST_SET_A_1.data(), 20, &str_Request), 0);  // payload
    EXPECT_EQ(PBWP_DecodeRequest(nullptr, 0, &str_Request), 0);                    // empty
}

TEST(PbwpRequest, DecodesOnlyTheFirstOfTwoFrames) {
    std::vector<uint8_t> vec_TwoFrames(PBWP_TEST_SET_A_1.begin(), PBWP_TEST_SET_A_1.end());
    vec_TwoFrames.insert(vec_TwoFrames.end(), PBWP_TEST_SET_A_1.begin(), PBWP_TEST_SET_A_1.end());

    PbwpRequest str_Request;
    EXPECT_EQ(PBWP_DecodeRequest(vec_TwoFrames.data(), vec_TwoFrames.size(), &str_Request), 24);
}

TEST(PbwpRequest, ValuesMayContainAnyBytes) {
    PbwpRequest str_Request;
    str_Request.enm_Opcode = PbwpOpcode::OP_SET;
    str_Request.vec_Args   = {"key", std::string("a\0b\n c", 6)};

    std::array<uint8_t, 64> arr_Out{};
    const int int_Length = PBWP_EncodeRequest(str_Request, arr_Out.data(), arr_Out.size());
    ASSERT_GT(int_Length, 0);

    PbwpRequest str_Decoded;
    ASSERT_EQ(PBWP_DecodeRequest(arr_Out.data(), static_cast<size_t>(int_Length), &str_Decoded),
              int_Length);
    EXPECT_EQ(str_Decoded.vec_Args, str_Request.vec_Args);
}

TEST(PbwpRequest, BrokenFramesAreRejected) {
    PbwpRequest str_Request;

    std::array<uint8_t, 24> arr_BadMagic = PBWP_TEST_SET_A_1;
    arr_BadMagic[0]                      = 0x50;  // 'P'
    EXPECT_EQ(PBWP_DecodeRequest(arr_BadMagic.data(), arr_BadMagic.size(), &str_Request),
              -P_EINVARG);

    std::array<uint8_t, 24> arr_BadVersion = PBWP_TEST_SET_A_1;
    arr_BadVersion[1]                      = 0x02;
    EXPECT_EQ(PBWP_DecodeRequest(arr_BadVersion.data(), arr_BadVersion.size(), &str_Request),
              -P_EINVARG);

    std::array<uint8_t, 24> arr_TooLarge = PBWP_TEST_SET_A_1;  // payload length 0x01000001
    arr_TooLarge[8]                      = 0x01;               // = 16 MiB + 1
    arr_TooLarge[11]                     = 0x01;
    EXPECT_EQ(PBWP_DecodeRequest(arr_TooLarge.data(), arr_TooLarge.size(), &str_Request),
              -P_ERANGE);

    std::array<uint8_t, 24> arr_LyingLength = PBWP_TEST_SET_A_1;  // key length 5, 1 follows
    arr_LyingLength[14]                     = 0x05;
    EXPECT_EQ(PBWP_DecodeRequest(arr_LyingLength.data(), arr_LyingLength.size(), &str_Request),
              -P_EINVARG);

    std::array<uint8_t, 24> arr_HugeCount = PBWP_TEST_SET_A_1;  // claims 65535 arguments
    arr_HugeCount[12]                     = 0xFF;
    arr_HugeCount[13]                     = 0xFF;
    EXPECT_EQ(PBWP_DecodeRequest(arr_HugeCount.data(), arr_HugeCount.size(), &str_Request),
              -P_EINVARG);

    EXPECT_EQ(PBWP_DecodeRequest(nullptr, 24, &str_Request), -P_EINVARG);
}

TEST(PbwpRequest, LeftoverBytesAreRejected) {
    /* SET a 1 claiming one argument: "1" and its length are left over after the key */
    std::array<uint8_t, 24> arr_Leftover = PBWP_TEST_SET_A_1;
    arr_Leftover[12]                     = 0x01;

    PbwpRequest str_Request;
    EXPECT_EQ(PBWP_DecodeRequest(arr_Leftover.data(), arr_Leftover.size(), &str_Request),
              -P_EINVARG);
}

TEST(PbwpRequest, AnyFlagSetIsRejectedInVersion1) {
    PbwpRequest str_Request;

    std::array<uint8_t, 24> arr_More = PBWP_TEST_SET_A_1;
    arr_More[3]                      = PBWP_FLAG_MORE;  // reserved, not supported yet
    EXPECT_EQ(PBWP_DecodeRequest(arr_More.data(), arr_More.size(), &str_Request), -P_EINVARG);

    std::array<uint8_t, 24> arr_Unknown = PBWP_TEST_SET_A_1;
    arr_Unknown[3]                      = 0x80;  // a bit no version defines yet
    EXPECT_EQ(PBWP_DecodeRequest(arr_Unknown.data(), arr_Unknown.size(), &str_Request), -P_EINVARG);
}

// ---- Requests: conversion to Command ---------------------------------------------------------

TEST(PbwpToCommand, SetBecomesCommandWithArguments) {
    PbwpRequest str_Request;
    str_Request.enm_Opcode = PbwpOpcode::OP_SET;
    str_Request.vec_Args   = {"a", "1", "b", "2"};

    Command     str_Command;
    std::string sz_Error;
    ASSERT_EQ(PBWP_PbwpToCommand(&str_Request, &str_Command, &sz_Error), P_OK);
    EXPECT_EQ(str_Command.enm_Type, CommandType::CMD_SET);
    EXPECT_EQ(str_Command.vec_Args, (std::vector<std::string>{"a", "1", "b", "2"}));
}

TEST(PbwpToCommand, PingGetDelBecomeCommands) {
    Command     str_Command;
    std::string sz_Error;

    PbwpRequest str_Ping;
    str_Ping.enm_Opcode = PbwpOpcode::OP_PING;
    ASSERT_EQ(PBWP_PbwpToCommand(&str_Ping, &str_Command, &sz_Error), P_OK);
    EXPECT_EQ(str_Command.enm_Type, CommandType::CMD_PING);

    PbwpRequest str_Get;
    str_Get.enm_Opcode = PbwpOpcode::OP_GET;
    str_Get.vec_Args   = {"k"};
    ASSERT_EQ(PBWP_PbwpToCommand(&str_Get, &str_Command, &sz_Error), P_OK);
    EXPECT_EQ(str_Command.enm_Type, CommandType::CMD_GET);
    EXPECT_EQ(str_Command.vec_Args, (std::vector<std::string>{"k"}));

    PbwpRequest str_Del;
    str_Del.enm_Opcode = PbwpOpcode::OP_DEL;
    str_Del.vec_Args   = {"k"};
    ASSERT_EQ(PBWP_PbwpToCommand(&str_Del, &str_Command, &sz_Error), P_OK);
    EXPECT_EQ(str_Command.enm_Type, CommandType::CMD_DEL);
}

TEST(PbwpToCommand, BadRequestsGiveErrors) {
    Command     str_Command;
    std::string sz_Error;

    PbwpRequest str_OddSet;
    str_OddSet.enm_Opcode = PbwpOpcode::OP_SET;
    str_OddSet.vec_Args   = {"a", "1", "b"};
    EXPECT_EQ(PBWP_PbwpToCommand(&str_OddSet, &str_Command, &sz_Error), -P_EINVARG);
    EXPECT_FALSE(sz_Error.empty());

    PbwpRequest str_GetTwoKeys;
    str_GetTwoKeys.enm_Opcode = PbwpOpcode::OP_GET;
    str_GetTwoKeys.vec_Args   = {"a", "b"};
    EXPECT_EQ(PBWP_PbwpToCommand(&str_GetTwoKeys, &str_Command, &sz_Error), -P_EINVARG);

    PbwpRequest str_Unknown;
    str_Unknown.enm_Opcode = static_cast<PbwpOpcode>(0x7F);
    EXPECT_EQ(PBWP_PbwpToCommand(&str_Unknown, &str_Command, &sz_Error), -P_EINVARG);

    PbwpRequest str_LongKey;
    str_LongKey.enm_Opcode = PbwpOpcode::OP_GET;
    str_LongKey.vec_Args   = {std::string(PBWP_MAX_KEY_SIZE + 1, 'k')};
    EXPECT_EQ(PBWP_PbwpToCommand(&str_LongKey, &str_Command, &sz_Error), -P_ERANGE);

    EXPECT_EQ(PBWP_PbwpToCommand(nullptr, &str_Command, &sz_Error), -P_EINVARG);
}

// ---- Replies ---------------------------------------------------------------------------------

TEST(PbwpReply, OkMatchesSpecBytes) {
    constexpr std::array<uint8_t, 12> arr_Expected = {
        0xB1, 0x01, 0x00, 0x00,  // magic, version, RPL_OK, flags
        0x07, 0x00, 0x00, 0x00,  // request id 7
        0x00, 0x00, 0x00, 0x00,  // no payload
    };
    PbwpReply str_Reply;
    str_Reply.enm_Type      = PbwpReplyType::RPL_OK;
    str_Reply.u32_RequestId = 7;

    std::array<uint8_t, 12> arr_Out{};
    ASSERT_EQ(PBWP_EncodeReply(str_Reply, arr_Out.data(), arr_Out.size()), 12);
    EXPECT_EQ(arr_Out, arr_Expected);
}

TEST(PbwpReply, SizeQueryAndTooSmallBuffer) {
    PbwpReply str_Reply;
    str_Reply.enm_Type   = PbwpReplyType::RPL_VALUE;
    str_Reply.sz_Payload = "pumpkin";  // 12 + 7 = 19 bytes

    EXPECT_EQ(PBWP_EncodeReply(str_Reply, nullptr, 0), 19);  // size query

    std::array<uint8_t, 10> arr_Small{};
    EXPECT_EQ(PBWP_EncodeReply(str_Reply, arr_Small.data(), arr_Small.size()), -P_ERANGE);
    EXPECT_EQ(arr_Small[0], 0);  // nothing was written

    EXPECT_EQ(PBWP_EncodeReply(str_Reply, nullptr, 64), -P_EINVARG);
}

TEST(PbwpReply, ErrorCarriesStatusInPayload) {
    constexpr std::array<uint8_t, 16> arr_Expected = {
        0xB1, 0x01, 0x04, 0x00,  // magic, version, RPL_ERROR, flags
        0x04, 0x00, 0x00, 0x00,  // request id 4
        0x04, 0x00, 0x00, 0x00,  // payload length 4: status + "bad"
        0x03,                    // status P_EINVARG
        'b',  'a',  'd',         // message
    };
    PbwpReply str_Reply;
    str_Reply.enm_Type      = PbwpReplyType::RPL_ERROR;
    str_Reply.u8_Status     = static_cast<uint8_t>(P_EINVARG);
    str_Reply.u32_RequestId = 4;
    str_Reply.sz_Payload    = "bad";

    std::array<uint8_t, 16> arr_Out{};
    ASSERT_EQ(PBWP_EncodeReply(str_Reply, arr_Out.data(), arr_Out.size()), 16);
    EXPECT_EQ(arr_Out, arr_Expected);

    /* An error frame without even the status byte is broken */
    constexpr std::array<uint8_t, 12> arr_NoStatus = {0xB1, 0x01, 0x04, 0x00, 0x04, 0x00,
                                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    PbwpReply                         str_Decoded;
    EXPECT_EQ(PBWP_DecodeReply(arr_NoStatus.data(), arr_NoStatus.size(), &str_Decoded), -P_EINVARG);
}

TEST(PbwpReply, IntegerKeepsAll64Bits) {
    PbwpReply str_Reply;
    str_Reply.enm_Type    = PbwpReplyType::RPL_INTEGER;
    str_Reply.s64_Integer = -2;  // FE FF FF FF FF FF FF FF: the high half must survive

    std::array<uint8_t, 20> arr_Out{};
    ASSERT_EQ(PBWP_EncodeReply(str_Reply, arr_Out.data(), arr_Out.size()), 20);

    PbwpReply str_Decoded;
    ASSERT_EQ(PBWP_DecodeReply(arr_Out.data(), arr_Out.size(), &str_Decoded), 20);
    EXPECT_EQ(str_Decoded.s64_Integer, -2);
}

TEST(PbwpReply, MalformedRepliesAreRejected) {
    PbwpReply str_Decoded;

    /* RPL_OK must be empty */
    constexpr std::array<uint8_t, 13> arr_OkWithPayload = {0xB1, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
                                                           0x00, 0x01, 0x00, 0x00, 0x00, 0x2A};
    EXPECT_EQ(PBWP_DecodeReply(arr_OkWithPayload.data(), arr_OkWithPayload.size(), &str_Decoded),
              -P_EINVARG);

    /* RPL_INTEGER must carry exactly 8 bytes */
    constexpr std::array<uint8_t, 13> arr_ShortInteger = {0xB1, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00,
                                                          0x00, 0x01, 0x00, 0x00, 0x00, 0x2A};
    EXPECT_EQ(PBWP_DecodeReply(arr_ShortInteger.data(), arr_ShortInteger.size(), &str_Decoded),
              -P_EINVARG);

    /* Unknown reply type */
    constexpr std::array<uint8_t, 12> arr_UnknownType = {0xB1, 0x01, 0x7F, 0x00, 0x01, 0x00,
                                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(PBWP_DecodeReply(arr_UnknownType.data(), arr_UnknownType.size(), &str_Decoded),
              -P_EINVARG);
}

TEST(PbwpReply, EveryReplyTypeRoundTrips) {
    const std::vector<PbwpReply> vec_Replies = {
        PbwpReply{PbwpReplyType::RPL_OK, 0, 1, "", 0},
        PbwpReply{PbwpReplyType::RPL_VALUE, 0, 2, "pumpkin", 0},
        PbwpReply{PbwpReplyType::RPL_NIL, 0, 3, "", 0},
        PbwpReply{PbwpReplyType::RPL_INTEGER, 0, 4, "", 1},
        PbwpReply{PbwpReplyType::RPL_ERROR, static_cast<uint8_t>(P_EINVARG), 5, "bad", 0},
    };

    /* Encode all of them back to back into one buffer, advancing by each frame's size */
    std::array<uint8_t, 256> arr_Out{};
    size_t                   siz_Used = 0;
    for (const PbwpReply& str_Reply : vec_Replies) {
        const int int_Length =
            PBWP_EncodeReply(str_Reply, arr_Out.data() + siz_Used, arr_Out.size() - siz_Used);
        ASSERT_GT(int_Length, 0);
        siz_Used += static_cast<size_t>(int_Length);
    }

    /* Decode them one by one, the same way: advance a position through the buffer */
    size_t siz_Pos = 0;
    for (const PbwpReply& str_Expected : vec_Replies) {
        PbwpReply str_Decoded;
        const int int_Length =
            PBWP_DecodeReply(arr_Out.data() + siz_Pos, siz_Used - siz_Pos, &str_Decoded);
        ASSERT_GT(int_Length, 0);
        EXPECT_EQ(str_Decoded.enm_Type, str_Expected.enm_Type);
        EXPECT_EQ(str_Decoded.u8_Status, str_Expected.u8_Status);
        EXPECT_EQ(str_Decoded.u32_RequestId, str_Expected.u32_RequestId);
        EXPECT_EQ(str_Decoded.sz_Payload, str_Expected.sz_Payload);
        EXPECT_EQ(str_Decoded.s64_Integer, str_Expected.s64_Integer);
        siz_Pos += static_cast<size_t>(int_Length);
    }
    EXPECT_EQ(siz_Pos, siz_Used);  // every byte consumed
}

}  // namespace
}  // namespace pumpkin::middlewares
