/**
 ******************************************************************************
 * @file    CDE_DispatcherTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the Command Dispatch Engine (CDE).
 *          This file tests:
 *           + CDE_ExecuteRequest: request in, reply out, with a real KvStore and no sockets
 *           + Request validation: argument counts, key size, unknown opcode
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

#include "services/pump-srv-cde/CDE_Dispatcher.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"

namespace pumpkin::services {
namespace {

/* Request id used by every test; CDE must copy it into the reply */
constexpr uint32_t CDE_TEST_REQUEST_ID = 7;

/**
 * @brief Builds a request for a test.
 *
 * @param enm_Opcode The opcode.
 * @param vec_Args   The arguments.
 *
 * @return The request, with id CDE_TEST_REQUEST_ID.
 */
PwpRequest cde_makeRequest(PwpOpcode enm_Opcode, std::vector<std::string> vec_Args) {
    PwpRequest str_Request;
    str_Request.enm_Opcode    = enm_Opcode;
    str_Request.u32_RequestId = CDE_TEST_REQUEST_ID;
    str_Request.vec_Args      = std::move(vec_Args);
    return str_Request;
}

TEST(ExecuteRequest, PingReturnsPong) {
    applications::KvStore obj_Store;
    const PwpReply        str_Reply =
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_PING, {}), obj_Store);
    EXPECT_EQ(str_Reply.enm_Type, PwpReplyType::RPL_PONG);
    EXPECT_EQ(str_Reply.u32_RequestId, CDE_TEST_REQUEST_ID);
}

TEST(ExecuteRequest, SetThenGetReturnsValue) {
    applications::KvStore obj_Store;
    EXPECT_EQ(CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_SET, {"name", "pumpkin"}), obj_Store)
                  .enm_Type,
              PwpReplyType::RPL_OK);

    const PwpReply str_Reply =
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_GET, {"name"}), obj_Store);
    EXPECT_EQ(str_Reply.enm_Type, PwpReplyType::RPL_VALUE);
    EXPECT_EQ(str_Reply.sz_Payload, "pumpkin");
}

/* Bug 4: every pair is stored */
TEST(ExecuteRequest, SetStoresEveryPair) {
    applications::KvStore obj_Store;
    CDE_ExecuteRequest(
        cde_makeRequest(PwpOpcode::OP_SET, {"person/name/01", "jack", "person/name/02", "tim"}),
        obj_Store);
    EXPECT_EQ(obj_Store.get("person/name/01"), "jack");
    EXPECT_EQ(obj_Store.get("person/name/02"), "tim");
}

/* Bug 4: a key without a value stores nothing (not even the complete pairs) */
TEST(ExecuteRequest, SetWithMissingValueStoresNothing) {
    applications::KvStore obj_Store;
    const PwpReply        str_Reply =
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_SET, {"a", "1", "b"}), obj_Store);
    EXPECT_EQ(str_Reply.enm_Type, PwpReplyType::RPL_ERROR);
    EXPECT_EQ(str_Reply.u8_Status, P_EINVARG);
    EXPECT_EQ(str_Reply.sz_Payload, "SET takes key value pairs");
    EXPECT_FALSE(obj_Store.get("a").has_value());
}

TEST(ExecuteRequest, GetMissingKeyReturnsNil) {
    applications::KvStore obj_Store;
    EXPECT_EQ(
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_GET, {"missing"}), obj_Store).enm_Type,
        PwpReplyType::RPL_NIL);
}

TEST(ExecuteRequest, DelReturnsNumberOfKeysRemoved) {
    applications::KvStore obj_Store;
    obj_Store.set("name", "pumpkin");
    EXPECT_EQ(
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_DEL, {"name"}), obj_Store).s64_Integer, 1);
    EXPECT_EQ(
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_DEL, {"name"}), obj_Store).s64_Integer, 0);
}

/* Moved here from PWP: CDE owns argument validation */
TEST(ExecuteRequest, WrongArgumentCountsAreRejected) {
    applications::KvStore obj_Store;
    EXPECT_EQ(
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_PING, {"extra"}), obj_Store).sz_Payload,
        "PING takes no arguments");
    EXPECT_EQ(CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_GET, {}), obj_Store).sz_Payload,
              "GET takes exactly one argument");
    EXPECT_EQ(
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_DEL, {"a", "b"}), obj_Store).sz_Payload,
        "DEL takes exactly one argument");
    EXPECT_EQ(CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_SET, {}), obj_Store).sz_Payload,
              "SET takes key value pairs");
}

TEST(ExecuteRequest, UnknownOpcodeIsRejected) {
    applications::KvStore obj_Store;
    const PwpReply        str_Reply =
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_INVALID, {}), obj_Store);
    EXPECT_EQ(str_Reply.enm_Type, PwpReplyType::RPL_ERROR);
    EXPECT_EQ(str_Reply.u8_Status, P_EINVARG);
    EXPECT_EQ(str_Reply.u32_RequestId, CDE_TEST_REQUEST_ID);
}

TEST(ExecuteRequest, KeyLongerThanLimitIsRejected) {
    applications::KvStore obj_Store;
    const std::string     sz_LongKey(PWP_MAX_KEY_SIZE + 1, 'k');
    const PwpReply        str_Reply =
        CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_GET, {sz_LongKey}), obj_Store);
    EXPECT_EQ(str_Reply.enm_Type, PwpReplyType::RPL_ERROR);
    EXPECT_EQ(str_Reply.u8_Status, P_ERANGE);
}

/* Issue #8: at the default level, values never appear in the log; keys and sizes do */
TEST(ExecuteRequest, ValuesAreNotLoggedAtInfo) {
    LOG_Init("info");
    applications::KvStore obj_Store;

    testing::internal::CaptureStderr();
    CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_SET, {"token", "s3cr3t"}), obj_Store);
    CDE_ExecuteRequest(cde_makeRequest(PwpOpcode::OP_GET, {"token"}), obj_Store);
    const std::string sz_Output = testing::internal::GetCapturedStderr();

    EXPECT_NE(sz_Output.find("SET 'token' (6 bytes)"), std::string::npos) << sz_Output;
    EXPECT_NE(sz_Output.find("GET 'token': found (6 bytes)"), std::string::npos) << sz_Output;
    EXPECT_EQ(sz_Output.find("s3cr3t"), std::string::npos) << sz_Output;
}

}  // namespace
}  // namespace pumpkin::services
