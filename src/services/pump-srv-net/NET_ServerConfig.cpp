/**
 ******************************************************************************
 * @file    NET_ServerConfig.cpp
 * @author  Shangjie Zheng
 * @brief   TCP server configuration: implementation.
 *          This file provides functions to:
 *           + Parse and validate a port number and an IPv4 address
 *           + Apply --bind / --port from the command line to a NetServerConfig
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

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstring>

#include "common/pump-cmn-log/LOG_Logger.hpp"
#include "common/pump-cmn-stat/STAT_Status.hpp"

LOG_MODULE_REGISTER(NET, LOG_LEVEL_DBG)

namespace pumpkin::services {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

NetServerConfig net_str_serverConfig;

/**
 * @brief Checks that the text is a valid IPv4 address in dotted form, e.g. "127.0.0.1".
 *
 * @param sz_Ip The IPv4 address string to check.
 *
 * @return P_OK; -P_EINVARG otherwise (host names such as "localhost" are not accepted).
 */
int net_checkIpv4(const char* sz_Ip) {
    in_addr str_Address{};
    return (inet_pton(AF_INET, sz_Ip, &str_Address) == 1) ? P_OK : -P_EINVARG;
}
}  // namespace

// ================================================================================================
// GLOBAL SCOPE
// ================================================================================================
int NET_SetPort(uint16_t u16_Port) {
    net_str_serverConfig.u16_Port = u16_Port;
    return P_OK;
}

int NET_SetBindIp(const char* sz_Ip) {
    if (sz_Ip == nullptr || net_checkIpv4(sz_Ip) < 0) {
        return -P_EINVARG;
    }

    net_str_serverConfig.sz_BindIp = sz_Ip;
    return P_OK;
}

const NetServerConfig& NET_GetServerConfig() {
    return net_str_serverConfig;
}

}  // namespace pumpkin::services
