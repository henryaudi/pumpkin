/**
 ******************************************************************************
 * @file    CMS_ManagerTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the Config Management Service (CMS).
 *          This file tests:
 *           + No arguments keeps every module's defaults
 *           + --port / --bind reach NET, --loglevel reaches LOG
 *           + Unknown or bare arguments, missing and invalid values are errors
 *           + A bad option anywhere applies nothing (validate first, then apply)
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

#include "services/pump-srv-cms/CMS_Manager.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"
#include "services/pump-srv-net/NET_ServerConfig.hpp"

namespace pumpkin::services {
namespace {

/**
 * @brief Runs CMS_Init on arguments given as strings, as if typed after "pumpkin-server".
 */
int cms_run(std::vector<std::string> vec_Args) {
    vec_Args.insert(vec_Args.begin(), "pumpkin-server");  // argv[0]
    std::vector<char*> vec_Argv;
    for (std::string& sz_Arg : vec_Args) {
        vec_Argv.push_back(sz_Arg.data());
    }
    vec_Argv.push_back(nullptr);  // argv[argc] is NULL, as in a real main()
    return CMS_Init(static_cast<int>(vec_Args.size()), vec_Argv.data());
}

/* CMS changes process-wide settings: put every module's defaults back after each test */
class CmsManagerTest : public ::testing::Test {
protected:
    void TearDown() override {
        NET_SetPort(NET_DEFAULT_PORT);
        NET_SetBindIp(NET_DEFAULT_BIND_IP);
        LOG_Init("info");
    }
};

TEST_F(CmsManagerTest, NoArgumentsKeepsDefaults) {
    ASSERT_EQ(cms_run({}), P_OK);
    EXPECT_EQ(NET_GetServerConfig().u16_Port, NET_DEFAULT_PORT);
    EXPECT_EQ(NET_GetServerConfig().sz_BindIp, NET_DEFAULT_BIND_IP);
}

TEST_F(CmsManagerTest, NetOptionsReachNet) {
    ASSERT_EQ(cms_run({"--port", "7378", "--bind", "0.0.0.0"}), P_OK);
    EXPECT_EQ(NET_GetServerConfig().u16_Port, 7378);
    EXPECT_EQ(NET_GetServerConfig().sz_BindIp, "0.0.0.0");
}

TEST_F(CmsManagerTest, PortZeroIsAllowed) {
    ASSERT_EQ(cms_run({"--port", "0"}), P_OK);
    EXPECT_EQ(NET_GetServerConfig().u16_Port, 0);
}

TEST_F(CmsManagerTest, LogLevelReachesLog) {
    EXPECT_EQ(cms_run({"--loglevel", "debug"}), P_OK);
    EXPECT_EQ(cms_run({"--loglevel", "loud"}), -P_EINVARG);  // not a level name
}

TEST_F(CmsManagerTest, UnknownAndBareArgumentsAreErrors) {
    EXPECT_EQ(cms_run({"--prot", "7000"}), -P_EINVARG);  // typo
    EXPECT_EQ(cms_run({"8000"}), -P_EINVARG);            // old bare-port form
    EXPECT_EQ(NET_GetServerConfig().u16_Port, NET_DEFAULT_PORT);
}

TEST_F(CmsManagerTest, MissingValuesAreErrors) {
    EXPECT_EQ(cms_run({"--port"}), -P_EINVARG);
    EXPECT_EQ(cms_run({"--bind"}), -P_EINVARG);
    EXPECT_EQ(cms_run({"--loglevel"}), -P_EINVARG);
    EXPECT_EQ(cms_run({"--port", "--bind", "0.0.0.0"}), -P_EINVARG);  // next word is a flag
}

TEST_F(CmsManagerTest, InvalidValuesAreErrors) {
    EXPECT_EQ(cms_run({"--port", ""}), -P_EINVARG);
    EXPECT_EQ(cms_run({"--port", "abc"}), -P_EINVARG);
    EXPECT_EQ(cms_run({"--port", "-1"}), -P_EINVARG);
    EXPECT_EQ(cms_run({"--port", "65536"}), -P_ERANGE);
    EXPECT_EQ(cms_run({"--port", "123456"}), -P_ERANGE);
    EXPECT_EQ(cms_run({"--bind", "localhost"}), -P_EINVARG);
}

TEST_F(CmsManagerTest, InvalidOptionAppliesNothing) {
    EXPECT_EQ(cms_run({"--port", "8000", "--bind", "nope"}), -P_EINVARG);
    EXPECT_EQ(NET_GetServerConfig().u16_Port, NET_DEFAULT_PORT);  // --port was not applied
    EXPECT_EQ(NET_GetServerConfig().sz_BindIp, NET_DEFAULT_BIND_IP);
}

}  // namespace
}  // namespace pumpkin::services
