/**
 ******************************************************************************
 * @file    PWP_CodecTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the Pumpkin Wire Protocol (PWP) codec.
 *          This file tests:
 *           + PWP_ExtractLine: splitting received bytes into lines
 *           + PWP_ParseLine: valid and invalid requests
 *           + PWP_EncodeReply: the text of every reply type
 *           + Request frames: encoding against the spec bytes, size queries, decoding partial,
 *             several and broken frames
 *           + Reply frames: every reply type, error status in the payload, malformed replies
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

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/pump-cmn-stat/STAT_Status.hpp"
#include "middlewares/pump-mid-pbwp/PBWP_Message.hpp"  // PBWP_FLAG_MORE in the flags test

namespace pumpkin::services {
namespace {

// ================================================================================================
// TEXT
// ================================================================================================

TEST(ExtractLine, ReturnsNothingWithoutNewline) {
    std::string sz_Buffer = "PING";
    EXPECT_FALSE(PWP_ExtractLine(sz_Buffer).has_value());
    EXPECT_EQ(sz_Buffer, "PING");
}

TEST(ExtractLine, ReturnsFirstLineAndKeepsTheRest) {
    std::string sz_Buffer = "PING\nGET na";
    EXPECT_EQ(PWP_ExtractLine(sz_Buffer), "PING");
    EXPECT_EQ(sz_Buffer, "GET na");
}

TEST(ExtractLine, RemovesCarriageReturn) {
    std::string sz_Buffer = "PING\r\n";
    EXPECT_EQ(PWP_ExtractLine(sz_Buffer), "PING");
    EXPECT_EQ(sz_Buffer, "");
}

TEST(ParseLine, Ping) {
    const ParseResult str_Result = PWP_ParseLine("PING");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    EXPECT_EQ(str_Result.opt_Request.value().enm_Opcode, PwpOpcode::OP_PING);
    EXPECT_EQ(str_Result.opt_Request.value().u32_RequestId, 0u);  // text has no request ids
}

TEST(ParseLine, RequestNameIsCaseInsensitive) {
    const ParseResult str_Result = PWP_ParseLine("get name");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    EXPECT_EQ(str_Result.opt_Request.value().enm_Opcode, PwpOpcode::OP_GET);
    EXPECT_EQ(str_Result.opt_Request.value().vec_Args, std::vector<std::string>{"name"});
}

/* Bug 4: SET takes key value pairs */
TEST(ParseLine, SetTakesKeyValuePairs) {
    const ParseResult str_Result = PWP_ParseLine("SET person/name/01 jack person/name/02 tim");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    EXPECT_EQ(str_Result.opt_Request.value().enm_Opcode, PwpOpcode::OP_SET);
    const std::vector<std::string> vec_Expected = {"person/name/01", "jack", "person/name/02",
                                                   "tim"};
    EXPECT_EQ(str_Result.opt_Request.value().vec_Args, vec_Expected);
}

TEST(ParseLine, DelTakesOneKey) {
    const ParseResult str_Result = PWP_ParseLine("DEL name");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    EXPECT_EQ(str_Result.opt_Request.value().enm_Opcode, PwpOpcode::OP_DEL);
    EXPECT_EQ(str_Result.opt_Request.value().vec_Args, std::vector<std::string>{"name"});
}

TEST(ParseLine, EmptyLineGivesError) {
    EXPECT_EQ(PWP_ParseLine("").sz_Error, "Empty request");
    EXPECT_EQ(PWP_ParseLine("   ").sz_Error, "Empty request");
}

/* Argument counts are CDE's job: the parser passes them through unchecked */
TEST(ParseLine, ArgumentsAreNotCheckedHere) {
    const ParseResult str_Result = PWP_ParseLine("GET a b");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    const std::vector<std::string> vec_Expected = {"a", "b"};
    EXPECT_EQ(str_Result.opt_Request.value().vec_Args, vec_Expected);

    EXPECT_TRUE(PWP_ParseLine("PING extra").opt_Request.has_value());
    EXPECT_TRUE(PWP_ParseLine("SET a 1 b").opt_Request.has_value());
}

TEST(ParseLine, UnknownRequestGivesError) {
    const ParseResult str_Result = PWP_ParseLine("hello");
    EXPECT_FALSE(str_Result.opt_Request.has_value());
    EXPECT_EQ(str_Result.sz_Error, "Unknown request: 'HELLO'");
}

/* Bug 1: trailing whitespace */
TEST(ParseLine, TrailingWhitespaceIsIgnored) {
    EXPECT_TRUE(PWP_ParseLine("PING      ").opt_Request.has_value());

    const ParseResult str_Result = PWP_ParseLine("GET name  ");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    EXPECT_EQ(str_Result.opt_Request.value().vec_Args, std::vector<std::string>{"name"});
}

/* Bug 2: extra whitespace between words */
TEST(ParseLine, ExtraSpacesBetweenWordsAreIgnored) {
    const ParseResult str_Result = PWP_ParseLine("SET   name    pumpkin");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    const std::vector<std::string> vec_Expected = {"name", "pumpkin"};
    EXPECT_EQ(str_Result.opt_Request.value().vec_Args, vec_Expected);
}

/* Bug 5: tabs */
TEST(ParseLine, TabsSeparateWords) {
    const ParseResult str_Result = PWP_ParseLine("SET\tname\t\tpumpkin\t");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    const std::vector<std::string> vec_Expected = {"name", "pumpkin"};
    EXPECT_EQ(str_Result.opt_Request.value().vec_Args, vec_Expected);
}

/* Bug 7: an extra "r" before the line ending is ignored */
TEST(ParseLine, ExtraCarriageReturnIsIgnored) {
    std::string                      sz_Buffer = "PING\r\r\n";
    const std::optional<std::string> opt_Line  = PWP_ExtractLine(sz_Buffer);
    ASSERT_TRUE(opt_Line.has_value());
    EXPECT_TRUE(PWP_ParseLine(opt_Line.value()).opt_Request.has_value());
}

/* Bug 10: "\r" separates words, so it can never end up inside a key or value */
TEST(ParseLine, CarriageReturnSeparatesWords) {
    const ParseResult str_Result = PWP_ParseLine("SET a\rb");
    ASSERT_TRUE(str_Result.opt_Request.has_value());
    const std::vector<std::string> vec_Expected = {"a", "b"};
    EXPECT_EQ(str_Result.opt_Request.value().vec_Args, vec_Expected);
}

TEST(EncodeReply, EncodesEveryReplyType) {
    EXPECT_EQ(PWP_EncodeReply(PwpReply{PwpReplyType::RPL_OK, 0, 0, "", 0}), "OK\n");
    EXPECT_EQ(PWP_EncodeReply(PwpReply{PwpReplyType::RPL_PONG, 0, 0, "", 0}), "PONG\n");
    EXPECT_EQ(PWP_EncodeReply(PwpReply{PwpReplyType::RPL_VALUE, 0, 0, "pumpkin", 0}),
              "VALUE pumpkin\n");
    EXPECT_EQ(PWP_EncodeReply(PwpReply{PwpReplyType::RPL_NIL, 0, 0, "", 0}), "NIL\n");
    EXPECT_EQ(PWP_EncodeReply(PwpReply{PwpReplyType::RPL_INTEGER, 0, 0, "", 1}), "INT 1\n");
    EXPECT_EQ(PWP_EncodeReply(PwpReply{PwpReplyType::RPL_ERROR, 3, 0, "bad", 0}), "ERR bad\n");
}

// ================================================================================================
// BINARY: REQUEST FRAMES
// ================================================================================================

/* "SET a 1" with request id 7: the worked example from PWP_Message.hpp (24 bytes) */
constexpr std::array<uint8_t, 24> PWP_TEST_SET_A_1 = {
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
PwpRequest pwp_makeSetA1() {
    PwpRequest str_Request;
    str_Request.enm_Opcode    = PwpOpcode::OP_SET;
    str_Request.u32_RequestId = 7;
    str_Request.vec_Args      = {"a", "1"};
    return str_Request;
}

TEST(RequestFrame, EncodeMatchesSpecBytes) {
    std::array<uint8_t, 24> arr_Out{};  // exactly the frame size: tests a full fit
    ASSERT_EQ(PWP_EncodeRequestFrame(pwp_makeSetA1(), arr_Out.data(), arr_Out.size()), 24);
    EXPECT_EQ(arr_Out, PWP_TEST_SET_A_1);
}

TEST(RequestFrame, SizeQueryAndTooSmallBuffer) {
    EXPECT_EQ(PWP_EncodeRequestFrame(pwp_makeSetA1(), nullptr, 0), 24);  // size query

    std::array<uint8_t, 10> arr_Small{};
    EXPECT_EQ(PWP_EncodeRequestFrame(pwp_makeSetA1(), arr_Small.data(), arr_Small.size()),
              -P_ERANGE);
    EXPECT_EQ(arr_Small[0], 0);  // nothing was written

    EXPECT_EQ(PWP_EncodeRequestFrame(pwp_makeSetA1(), nullptr, 64), -P_EINVARG);
}

TEST(RequestFrame, DecodeReturnsFrameSizeAndFields) {
    PwpRequest str_Request;
    ASSERT_EQ(
        PWP_DecodeRequestFrame(PWP_TEST_SET_A_1.data(), PWP_TEST_SET_A_1.size(), &str_Request), 24);
    EXPECT_EQ(str_Request.enm_Opcode, PwpOpcode::OP_SET);
    EXPECT_EQ(str_Request.u32_RequestId, 7u);
    EXPECT_EQ(str_Request.vec_Args, (std::vector<std::string>{"a", "1"}));
}

TEST(RequestFrame, PartialFrameNeedsMoreBytes) {
    PwpRequest str_Request;
    EXPECT_EQ(PWP_DecodeRequestFrame(PWP_TEST_SET_A_1.data(), 11, &str_Request), 0);  // header
    EXPECT_EQ(PWP_DecodeRequestFrame(PWP_TEST_SET_A_1.data(), 20, &str_Request), 0);  // payload
    EXPECT_EQ(PWP_DecodeRequestFrame(nullptr, 0, &str_Request), 0);                   // empty
}

TEST(RequestFrame, DecodesOnlyTheFirstOfTwoFrames) {
    std::vector<uint8_t> vec_TwoFrames(PWP_TEST_SET_A_1.begin(), PWP_TEST_SET_A_1.end());
    vec_TwoFrames.insert(vec_TwoFrames.end(), PWP_TEST_SET_A_1.begin(), PWP_TEST_SET_A_1.end());

    PwpRequest str_Request;
    EXPECT_EQ(PWP_DecodeRequestFrame(vec_TwoFrames.data(), vec_TwoFrames.size(), &str_Request), 24);
}

TEST(RequestFrame, ValuesMayContainAnyBytes) {
    PwpRequest str_Request;
    str_Request.enm_Opcode = PwpOpcode::OP_SET;
    str_Request.vec_Args   = {"key", std::string("a\0b\n c", 6)};

    std::array<uint8_t, 64> arr_Out{};
    const int int_Length = PWP_EncodeRequestFrame(str_Request, arr_Out.data(), arr_Out.size());
    ASSERT_GT(int_Length, 0);

    PwpRequest str_Decoded;
    ASSERT_EQ(PWP_DecodeRequestFrame(arr_Out.data(), static_cast<size_t>(int_Length), &str_Decoded),
              int_Length);
    EXPECT_EQ(str_Decoded.vec_Args, str_Request.vec_Args);
}

TEST(RequestFrame, BrokenFramesAreRejected) {
    PwpRequest str_Request;

    /* Header problems come from PBWP and are passed through unchanged */
    std::array<uint8_t, 24> arr_BadMagic = PWP_TEST_SET_A_1;
    arr_BadMagic[0]                      = 0x50;  // 'P'
    EXPECT_EQ(PWP_DecodeRequestFrame(arr_BadMagic.data(), arr_BadMagic.size(), &str_Request),
              -P_EINVARG);

    std::array<uint8_t, 24> arr_BadVersion = PWP_TEST_SET_A_1;
    arr_BadVersion[1]                      = 0x02;
    EXPECT_EQ(PWP_DecodeRequestFrame(arr_BadVersion.data(), arr_BadVersion.size(), &str_Request),
              -P_EINVARG);

    std::array<uint8_t, 24> arr_TooLarge = PWP_TEST_SET_A_1;  // payload length 0x01000001
    arr_TooLarge[8]                      = 0x01;              // = 16 MiB + 1
    arr_TooLarge[11]                     = 0x01;
    EXPECT_EQ(PWP_DecodeRequestFrame(arr_TooLarge.data(), arr_TooLarge.size(), &str_Request),
              -P_ERANGE);

    /* Argument list problems are found by PWP */
    std::array<uint8_t, 24> arr_LyingLength = PWP_TEST_SET_A_1;  // key length 5, 1 follows
    arr_LyingLength[14]                     = 0x05;
    EXPECT_EQ(PWP_DecodeRequestFrame(arr_LyingLength.data(), arr_LyingLength.size(), &str_Request),
              -P_EINVARG);

    std::array<uint8_t, 24> arr_HugeCount = PWP_TEST_SET_A_1;  // claims 65535 arguments
    arr_HugeCount[12]                     = 0xFF;
    arr_HugeCount[13]                     = 0xFF;
    EXPECT_EQ(PWP_DecodeRequestFrame(arr_HugeCount.data(), arr_HugeCount.size(), &str_Request),
              -P_EINVARG);

    EXPECT_EQ(PWP_DecodeRequestFrame(nullptr, 24, &str_Request), -P_EINVARG);
    EXPECT_EQ(PWP_DecodeRequestFrame(PWP_TEST_SET_A_1.data(), 24, nullptr), -P_EINVARG);
}

TEST(RequestFrame, LeftoverBytesAreRejected) {
    /* SET a 1 claiming one argument: "1" and its length are left over after the key */
    std::array<uint8_t, 24> arr_Leftover = PWP_TEST_SET_A_1;
    arr_Leftover[12]                     = 0x01;

    PwpRequest str_Request;
    EXPECT_EQ(PWP_DecodeRequestFrame(arr_Leftover.data(), arr_Leftover.size(), &str_Request),
              -P_EINVARG);
}

TEST(RequestFrame, AnyFlagSetIsRejectedInVersion1) {
    PwpRequest str_Request;

    std::array<uint8_t, 24> arr_More = PWP_TEST_SET_A_1;
    arr_More[3]                      = middlewares::PBWP_FLAG_MORE;  // reserved, not supported yet
    EXPECT_EQ(PWP_DecodeRequestFrame(arr_More.data(), arr_More.size(), &str_Request), -P_EINVARG);

    std::array<uint8_t, 24> arr_Unknown = PWP_TEST_SET_A_1;
    arr_Unknown[3]                      = 0x80;  // a bit no version defines yet
    EXPECT_EQ(PWP_DecodeRequestFrame(arr_Unknown.data(), arr_Unknown.size(), &str_Request),
              -P_EINVARG);
}

// ================================================================================================
// BINARY: REPLY FRAMES
// ================================================================================================

TEST(ReplyFrame, OkMatchesSpecBytes) {
    constexpr std::array<uint8_t, 12> arr_Expected = {
        0xB1, 0x01, 0x00, 0x00,  // magic, version, RPL_OK, flags
        0x07, 0x00, 0x00, 0x00,  // request id 7
        0x00, 0x00, 0x00, 0x00,  // no payload
    };
    PwpReply str_Reply;
    str_Reply.enm_Type      = PwpReplyType::RPL_OK;
    str_Reply.u32_RequestId = 7;

    std::array<uint8_t, 12> arr_Out{};
    ASSERT_EQ(PWP_EncodeReplyFrame(str_Reply, arr_Out.data(), arr_Out.size()), 12);
    EXPECT_EQ(arr_Out, arr_Expected);
}

TEST(ReplyFrame, SizeQueryAndTooSmallBuffer) {
    PwpReply str_Reply;
    str_Reply.enm_Type   = PwpReplyType::RPL_VALUE;
    str_Reply.sz_Payload = "pumpkin";  // 12 + 7 = 19 bytes

    EXPECT_EQ(PWP_EncodeReplyFrame(str_Reply, nullptr, 0), 19);  // size query writes nothing

    std::array<uint8_t, 10> arr_Small{};
    EXPECT_EQ(PWP_EncodeReplyFrame(str_Reply, arr_Small.data(), arr_Small.size()), -P_ERANGE);
    EXPECT_EQ(arr_Small[0], 0);  // nothing was written

    EXPECT_EQ(PWP_EncodeReplyFrame(str_Reply, nullptr, 64), -P_EINVARG);
}

/* Every reply type with a payload must survive a size query: none may write through NULL */
TEST(ReplyFrame, SizeQueryWorksForEveryReplyType) {
    EXPECT_EQ(PWP_EncodeReplyFrame(PwpReply{PwpReplyType::RPL_VALUE, 0, 1, "abc", 0}, nullptr, 0),
              15);
    EXPECT_EQ(PWP_EncodeReplyFrame(PwpReply{PwpReplyType::RPL_ERROR, 3, 1, "abc", 0}, nullptr, 0),
              16);
    EXPECT_EQ(PWP_EncodeReplyFrame(PwpReply{PwpReplyType::RPL_INTEGER, 0, 1, "", 5}, nullptr, 0),
              20);
    EXPECT_EQ(PWP_EncodeReplyFrame(PwpReply{PwpReplyType::RPL_PONG, 0, 1, "", 0}, nullptr, 0), 12);
}

TEST(ReplyFrame, ErrorCarriesStatusInPayload) {
    constexpr std::array<uint8_t, 16> arr_Expected = {
        0xB1, 0x01, 0x04, 0x00,  // magic, version, RPL_ERROR, flags
        0x04, 0x00, 0x00, 0x00,  // request id 4
        0x04, 0x00, 0x00, 0x00,  // payload length 4: status + "bad"
        0x03,                    // status P_EINVARG
        'b',  'a',  'd',         // message
    };
    PwpReply str_Reply;
    str_Reply.enm_Type      = PwpReplyType::RPL_ERROR;
    str_Reply.u8_Status     = static_cast<uint8_t>(P_EINVARG);
    str_Reply.u32_RequestId = 4;
    str_Reply.sz_Payload    = "bad";

    std::array<uint8_t, 16> arr_Out{};
    ASSERT_EQ(PWP_EncodeReplyFrame(str_Reply, arr_Out.data(), arr_Out.size()), 16);
    EXPECT_EQ(arr_Out, arr_Expected);

    /* An error frame without even the status byte is broken */
    constexpr std::array<uint8_t, 12> arr_NoStatus = {0xB1, 0x01, 0x04, 0x00, 0x04, 0x00,
                                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    PwpReply                          str_Decoded;
    EXPECT_EQ(PWP_DecodeReplyFrame(arr_NoStatus.data(), arr_NoStatus.size(), &str_Decoded),
              -P_EINVARG);
}

TEST(ReplyFrame, IntegerKeepsAll64Bits) {
    PwpReply str_Reply;
    str_Reply.enm_Type    = PwpReplyType::RPL_INTEGER;
    str_Reply.s64_Integer = -2;  // FE FF FF FF FF FF FF FF: the high half must survive

    std::array<uint8_t, 20> arr_Out{};
    ASSERT_EQ(PWP_EncodeReplyFrame(str_Reply, arr_Out.data(), arr_Out.size()), 20);

    PwpReply str_Decoded;
    ASSERT_EQ(PWP_DecodeReplyFrame(arr_Out.data(), arr_Out.size(), &str_Decoded), 20);
    EXPECT_EQ(str_Decoded.s64_Integer, -2);
}

TEST(ReplyFrame, MalformedRepliesAreRejected) {
    PwpReply str_Decoded;

    /* RPL_OK must be empty */
    constexpr std::array<uint8_t, 13> arr_OkWithPayload = {0xB1, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
                                                           0x00, 0x01, 0x00, 0x00, 0x00, 0x2A};
    EXPECT_EQ(
        PWP_DecodeReplyFrame(arr_OkWithPayload.data(), arr_OkWithPayload.size(), &str_Decoded),
        -P_EINVARG);

    /* RPL_PONG must be empty, like RPL_OK */
    constexpr std::array<uint8_t, 13> arr_PongWithPayload = {
        0xB1, 0x01, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2A};
    EXPECT_EQ(
        PWP_DecodeReplyFrame(arr_PongWithPayload.data(), arr_PongWithPayload.size(), &str_Decoded),
        -P_EINVARG);

    /* RPL_INTEGER must carry exactly 8 bytes */
    constexpr std::array<uint8_t, 13> arr_ShortInteger = {0xB1, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00,
                                                          0x00, 0x01, 0x00, 0x00, 0x00, 0x2A};
    EXPECT_EQ(PWP_DecodeReplyFrame(arr_ShortInteger.data(), arr_ShortInteger.size(), &str_Decoded),
              -P_EINVARG);

    /* Unknown reply type */
    constexpr std::array<uint8_t, 12> arr_UnknownType = {0xB1, 0x01, 0x7F, 0x00, 0x01, 0x00,
                                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(PWP_DecodeReplyFrame(arr_UnknownType.data(), arr_UnknownType.size(), &str_Decoded),
              -P_EINVARG);

    /* Unknown reply type on the encode side */
    std::array<uint8_t, 32> arr_Out{};
    EXPECT_EQ(PWP_EncodeReplyFrame(PwpReply{static_cast<PwpReplyType>(0x7F), 0, 1, "", 0},
                                   arr_Out.data(), arr_Out.size()),
              -P_EINVARG);
}

TEST(ReplyFrame, EveryReplyTypeRoundTrips) {
    const std::vector<PwpReply> vec_Replies = {
        PwpReply{PwpReplyType::RPL_OK, 0, 1, "", 0},
        PwpReply{PwpReplyType::RPL_VALUE, 0, 2, "pumpkin", 0},
        PwpReply{PwpReplyType::RPL_NIL, 0, 3, "", 0},
        PwpReply{PwpReplyType::RPL_INTEGER, 0, 4, "", 1},
        PwpReply{PwpReplyType::RPL_ERROR, static_cast<uint8_t>(P_EINVARG), 5, "bad", 0},
        PwpReply{PwpReplyType::RPL_PONG, 0, 6, "", 0},
    };

    /* Encode all of them back to back into one buffer, advancing by each frame's size */
    std::array<uint8_t, 256> arr_Out{};
    size_t                   siz_Used = 0;
    for (const PwpReply& str_Reply : vec_Replies) {
        const int int_Length =
            PWP_EncodeReplyFrame(str_Reply, arr_Out.data() + siz_Used, arr_Out.size() - siz_Used);
        ASSERT_GT(int_Length, 0);
        siz_Used += static_cast<size_t>(int_Length);
    }

    /* Decode them one by one, the same way: advance a position through the buffer */
    size_t siz_Pos = 0;
    for (const PwpReply& str_Expected : vec_Replies) {
        PwpReply  str_Decoded;
        const int int_Length =
            PWP_DecodeReplyFrame(arr_Out.data() + siz_Pos, siz_Used - siz_Pos, &str_Decoded);
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
}  // namespace pumpkin::services
