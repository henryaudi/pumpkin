/**
 ******************************************************************************
 * @file    CDE_DispatcherTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the Command Dispatch Engine (CDE).
 *          This file tests:
 *           + CDE_ProcessLine: text in, reply text out, with a real KvStore and no sockets
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

#include "middlewares/pump-mid-cde/CDE_Dispatcher.hpp"

#include <gtest/gtest.h>

#include <string>

namespace pumpkin::middlewares {
namespace {

TEST(ProcessLine, PingReturnsPong) {
    core::KvStore obj_Store;
    EXPECT_EQ(CDE_ProcessLine("PING", obj_Store), "PONG\n");
}

TEST(ProcessLine, SetThenGetReturnsValue) {
    core::KvStore obj_Store;
    EXPECT_EQ(CDE_ProcessLine("SET name pumpkin", obj_Store), "OK\n");
    EXPECT_EQ(CDE_ProcessLine("GET name", obj_Store), "VALUE pumpkin\n");
}

/* Bug 4: every pair is stored */
TEST(ProcessLine, SetStoresEveryPair) {
    core::KvStore obj_Store;
    EXPECT_EQ(CDE_ProcessLine("SET person/name/01 jack person/name/02 tim", obj_Store), "OK\n");
    EXPECT_EQ(CDE_ProcessLine("GET person/name/01", obj_Store), "VALUE jack\n");
    EXPECT_EQ(CDE_ProcessLine("GET person/name/02", obj_Store), "VALUE tim\n");
}

/* Bug 4: a key without a value stores nothing (not even the complete pairs) */
TEST(ProcessLine, SetWithMissingValueStoresNothing) {
    core::KvStore obj_Store;
    EXPECT_EQ(CDE_ProcessLine("SET a 1 b", obj_Store), "ERR SET takes key value pairs\n");
    EXPECT_EQ(CDE_ProcessLine("GET a", obj_Store), "NIL\n");
}

TEST(ProcessLine, GetMissingKeyReturnsNil) {
    core::KvStore obj_Store;
    EXPECT_EQ(CDE_ProcessLine("GET missing", obj_Store), "NIL\n");
}

TEST(ProcessLine, DelReturnsNumberOfKeysRemoved) {
    core::KvStore obj_Store;
    obj_Store.set("name", "pumpkin");
    EXPECT_EQ(CDE_ProcessLine("DEL name", obj_Store), "INT 1\n");
    EXPECT_EQ(CDE_ProcessLine("DEL name", obj_Store), "INT 0\n");
}

TEST(ProcessLine, InvalidLineReturnsError) {
    core::KvStore obj_Store;
    EXPECT_EQ(CDE_ProcessLine("NOPE", obj_Store), "ERR Unknown command: 'NOPE'\n");
}

TEST(ProcessLine, EmptyLineIsIgnored) {
    core::KvStore obj_Store;
    EXPECT_EQ(CDE_ProcessLine("", obj_Store), "");
}

/* Bug 6: a line with only whitespace is ignored like an empty one (no "ERR Empty command") */
TEST(ProcessLine, WhitespaceOnlyLineIsIgnored) {
    core::KvStore obj_Store;
    EXPECT_EQ(CDE_ProcessLine("   ", obj_Store), "");
    EXPECT_EQ(CDE_ProcessLine(" \t\r ", obj_Store), "");
}

}  // namespace
}  // namespace pumpkin::middlewares
