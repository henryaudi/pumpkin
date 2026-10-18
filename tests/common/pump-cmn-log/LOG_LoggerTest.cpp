/**
 ******************************************************************************
 * @file    LOG_LoggerTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for logging (LOG).
 *          This file tests:
 *           + LOG_Init: known, unknown and NULL level names
 *           + LOG_INF: the line format and runtime filtering
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

#include "common/pump-cmn-log/LOG_Logger.hpp"

#include <gtest/gtest.h>

#include <string>

#include "common/pump-cmn-stat/STAT_Status.hpp"

LOG_MODULE_REGISTER(TEST, LOG_LEVEL_TRC)

namespace {

TEST(LogInit, AcceptsKnownLevels) {
    const char* arr_Levels[] = {"none", "critical", "error", "warn", "info", "debug", "trace"};
    for (const char* sz_Level : arr_Levels) {
        EXPECT_EQ(LOG_Init(sz_Level), P_OK) << sz_Level;
    }
    LOG_Init("info");
}

TEST(LogInit, RejectsUnknownOrNullLevel) {
    EXPECT_EQ(LOG_Init("loud"), -P_EINVARG);
    EXPECT_EQ(LOG_Init(nullptr), -P_EINVARG);
}

TEST(LogWrite, LineHasLevelModuleAndThread) {
    LOG_Init("info");
    testing::internal::CaptureStderr();
    LOG_INF("hello %d", 7);
    const std::string sz_Output = testing::internal::GetCapturedStderr();

    EXPECT_NE(sz_Output.find("] [INFO    ] [TEST] ["), std::string::npos) << sz_Output;
    EXPECT_NE(sz_Output.find("hello 7"), std::string::npos) << sz_Output;
}

TEST(LogWrite, RuntimeLevelDropsMoreVerboseLines) {
    LOG_Init("warn");
    testing::internal::CaptureStderr();
    LOG_INF("hidden");
    LOG_WRN("shown");
    const std::string sz_Output = testing::internal::GetCapturedStderr();
    LOG_Init("info");

    EXPECT_EQ(sz_Output.find("hidden"), std::string::npos);
    EXPECT_NE(sz_Output.find("shown"), std::string::npos);
}

}  // namespace
