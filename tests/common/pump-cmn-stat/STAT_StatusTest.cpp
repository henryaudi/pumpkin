/**
 ******************************************************************************
 * @file    STAT_StatusTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the status codes (STAT).
 *          This file tests:
 *           + STAT_ToString: text for known and unknown codes
 *           + STAT_MapErrno: errno values with and without a code of their own
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

#include "common/pump-cmn-stat/STAT_Status.hpp"

#include <gtest/gtest.h>

#include <cerrno>
#include <string>

namespace {

TEST(StatusToString, DescribesKnownCodes) {
    EXPECT_EQ(std::string(STAT_ToString(P_OK)), "success");
    EXPECT_EQ(std::string(STAT_ToString(P_EINVARG)), "invalid argument");
    EXPECT_EQ(std::string(STAT_ToString(P_EADDRINUSE)), "address already in use");
}

TEST(StatusToString, UnknownCodeIsReported) {
    EXPECT_EQ(std::string(STAT_ToString(12345)), "unknown status");
}

TEST(StatusMapErrno, KnownErrnoGetsItsOwnCode) {
    EXPECT_EQ(STAT_MapErrno(EADDRINUSE), P_EADDRINUSE);
    EXPECT_EQ(STAT_MapErrno(EACCES), P_EACCES);
    EXPECT_EQ(STAT_MapErrno(ECONNRESET), P_ECONNRESET);
    EXPECT_EQ(STAT_MapErrno(EPIPE), P_EPIPE);
}

TEST(StatusMapErrno, OtherErrnoBecomesIoError) {
    EXPECT_EQ(STAT_MapErrno(EBADF), P_EIO);
    EXPECT_EQ(STAT_MapErrno(ENOTSOCK), P_EIO);
}

}  // namespace
