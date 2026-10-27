/**
 ******************************************************************************
 * @file    NET_ServerConfigTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for NET's server settings and setters.
 *          This file tests:
 *           + Defaults
 *           + NET_SetPort / NET_SetBindIp, and that invalid values change nothing
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

#include "services/pump-srv-net/NET_ServerConfig.hpp"

#include <gtest/gtest.h>

#include "common/pump-cmn-stat/STAT_Status.hpp"

namespace pumpkin::services {
namespace {

/* The settings are process-wide: put NET's defaults back after every test */
class NetServerConfigTest : public ::testing::Test {
protected:
    void TearDown() override {
        NET_SetPort(NET_DEFAULT_PORT);
        NET_SetBindIp(NET_DEFAULT_BIND_IP);
    }
};

TEST_F(NetServerConfigTest, DefaultsAreLocalhost7379) {
    const NetServerConfig str_Default;
    EXPECT_EQ(str_Default.sz_BindIp, "127.0.0.1");
    EXPECT_EQ(str_Default.u16_Port, 7379);
    EXPECT_EQ(NET_GetServerConfig().sz_BindIp, NET_DEFAULT_BIND_IP);
    EXPECT_EQ(NET_GetServerConfig().u16_Port, NET_DEFAULT_PORT);
}

TEST_F(NetServerConfigTest, SettersChangeTheSettings) {
    EXPECT_EQ(NET_SetPort(7378), P_OK);
    EXPECT_EQ(NET_SetBindIp("0.0.0.0"), P_OK);
    EXPECT_EQ(NET_GetServerConfig().u16_Port, 7378);
    EXPECT_EQ(NET_GetServerConfig().sz_BindIp, "0.0.0.0");
}

TEST_F(NetServerConfigTest, InvalidBindIpChangesNothing) {
    EXPECT_EQ(NET_SetBindIp("localhost"), -P_EINVARG);  // host names are not accepted
    EXPECT_EQ(NET_SetBindIp("300.1.1.1"), -P_EINVARG);
    EXPECT_EQ(NET_SetBindIp(""), -P_EINVARG);
    EXPECT_EQ(NET_SetBindIp(nullptr), -P_EINVARG);
    EXPECT_EQ(NET_GetServerConfig().sz_BindIp, NET_DEFAULT_BIND_IP);
}

}  // namespace
}  // namespace pumpkin::services
