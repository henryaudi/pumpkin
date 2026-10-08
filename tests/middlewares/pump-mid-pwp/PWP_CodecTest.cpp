/**
 ******************************************************************************
 * @file    PWP_CodecTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the Pumpkin Wire Protocol (PWP) codec.
 *          This file tests:
 *           + PWP_ExtractLine: splitting received bytes into lines
 *           + PWP_ParseLine: valid and invalid commands
 *           + PWP_EncodeReply: the text of every reply type
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

#include "middlewares/pump-mid-pwp/PWP_Codec.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

namespace pumpkin::middlewares {
namespace {

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
    ASSERT_TRUE(str_Result.opt_Command.has_value());
    EXPECT_EQ(str_Result.opt_Command.value().enm_Type, CommandType::CMD_PING);
}

TEST(ParseLine, CommandNameIsCaseInsensitive) {
    const ParseResult str_Result = PWP_ParseLine("get name");
    ASSERT_TRUE(str_Result.opt_Command.has_value());
    EXPECT_EQ(str_Result.opt_Command.value().enm_Type, CommandType::CMD_GET);
    EXPECT_EQ(str_Result.opt_Command.value().vec_Args, std::vector<std::string>{"name"});
}

/* Bug 4: SET takes key value pairs */
TEST(ParseLine, SetTakesKeyValuePairs) {
    const ParseResult str_Result = PWP_ParseLine("SET person/name/01 jack person/name/02 tim");
    ASSERT_TRUE(str_Result.opt_Command.has_value());
    EXPECT_EQ(str_Result.opt_Command.value().enm_Type, CommandType::CMD_SET);
    const std::vector<std::string> vec_Expected = {"person/name/01", "jack", "person/name/02",
                                                   "tim"};
    EXPECT_EQ(str_Result.opt_Command.value().vec_Args, vec_Expected);
}

TEST(ParseLine, DelTakesOneKey) {
    const ParseResult str_Result = PWP_ParseLine("DEL name");
    ASSERT_TRUE(str_Result.opt_Command.has_value());
    EXPECT_EQ(str_Result.opt_Command.value().enm_Type, CommandType::CMD_DEL);
    EXPECT_EQ(str_Result.opt_Command.value().vec_Args, std::vector<std::string>{"name"});
}

TEST(ParseLine, EmptyLineGivesError) {
    EXPECT_EQ(PWP_ParseLine("").sz_Error, "Empty command");
    EXPECT_EQ(PWP_ParseLine("   ").sz_Error, "Empty command");
}

TEST(ParseLine, WrongArgumentsGiveError) {
    EXPECT_EQ(PWP_ParseLine("PING extra").sz_Error, "PING takes no arguments");
    EXPECT_EQ(PWP_ParseLine("GET").sz_Error, "GET takes exactly one argument.");
    EXPECT_EQ(PWP_ParseLine("GET a b").sz_Error, "GET takes exactly one argument.");
    EXPECT_EQ(PWP_ParseLine("SET").sz_Error, "SET takes key value pairs");
    EXPECT_EQ(PWP_ParseLine("SET a").sz_Error, "SET takes key value pairs");
    EXPECT_EQ(PWP_ParseLine("SET a 1 b").sz_Error, "SET takes key value pairs");
}

TEST(ParseLine, UnknownCommandGivesError) {
    const ParseResult str_Result = PWP_ParseLine("hello");
    EXPECT_FALSE(str_Result.opt_Command.has_value());
    EXPECT_EQ(str_Result.sz_Error, "Unknown command: 'HELLO'");
}

/* Bug 1: trailing whitespace */
TEST(ParseLine, TrailingWhitespaceIsIgnored) {
    EXPECT_TRUE(PWP_ParseLine("PING      ").opt_Command.has_value());

    const ParseResult str_Result = PWP_ParseLine("GET name  ");
    ASSERT_TRUE(str_Result.opt_Command.has_value());
    EXPECT_EQ(str_Result.opt_Command.value().vec_Args, std::vector<std::string>{"name"});
}

/* Bug 2: extra whitespace between words */
TEST(ParseLine, ExtraSpacesBetweenWordsAreIgnored) {
    const ParseResult str_Result = PWP_ParseLine("SET   name    pumpkin");
    ASSERT_TRUE(str_Result.opt_Command.has_value());
    const std::vector<std::string> vec_Expected = {"name", "pumpkin"};
    EXPECT_EQ(str_Result.opt_Command.value().vec_Args, vec_Expected);
}

/* Bug 5: tabs */
TEST(ParseLine, TabsSeparateWords) {
    const ParseResult str_Result = PWP_ParseLine("SET\tname\t\tpumpkin\t");
    ASSERT_TRUE(str_Result.opt_Command.has_value());
    const std::vector<std::string> vec_Expected = {"name", "pumpkin"};
    EXPECT_EQ(str_Result.opt_Command.value().vec_Args, vec_Expected);
}

/* Bug 7: an extra "r" before the line ending is ignored */
TEST(ParseLine, ExtraCarriageReturnIsIgnored) {
    std::string sz_Buffer = "PING\r\r\n";
    const std::optional<std::string> opt_Line = PWP_ExtractLine(sz_Buffer);
    ASSERT_TRUE(opt_Line.has_value());
    EXPECT_TRUE(PWP_ParseLine(opt_Line.value()).opt_Command.has_value());
}

/* Bug 10: "\r" separates words, so it can never end up inside a key or value */
TEST(ParseLine, CarriageReturnSeparatesWords) {
    const ParseResult str_Result = PWP_ParseLine("SET a\rb");
    ASSERT_TRUE(str_Result.opt_Command.has_value());
    const std::vector<std::string> vec_Expected = {"a", "b"};
    EXPECT_EQ(str_Result.opt_Command.value().vec_Args, vec_Expected);
}

TEST(EncodeReply, EncodesEveryReplyType) {
    EXPECT_EQ(PWP_EncodeReply(Reply{ReplyType::REPLY_OK, "", 0}), "OK\n");
    EXPECT_EQ(PWP_EncodeReply(Reply{ReplyType::REPLY_PONG, "", 0}), "PONG\n");
    EXPECT_EQ(PWP_EncodeReply(Reply{ReplyType::REPLY_VALUE, "pumpkin", 0}), "VALUE pumpkin\n");
    EXPECT_EQ(PWP_EncodeReply(Reply{ReplyType::REPLY_NIL, "", 0}), "NIL\n");
    EXPECT_EQ(PWP_EncodeReply(Reply{ReplyType::REPLY_INTEGER, "", 1}), "INT 1\n");
    EXPECT_EQ(PWP_EncodeReply(Reply{ReplyType::REPLY_ERROR, "bad", 0}), "ERR bad\n");
}

}  // namespace
}  // namespace pumpkin::middlewares
