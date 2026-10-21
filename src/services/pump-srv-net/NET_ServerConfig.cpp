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

LOG_MODULE_REGISTER(NET, LOG_LEVEL_DEBUG)

namespace pumpkin::services {

namespace {
// ================================================================================================
// PRIVATE SCOPE
// ================================================================================================

constexpr char   NET_FLAG_BIND[]     = "--bind";
constexpr char   NET_FLAG_PORT[]     = "--port";
constexpr size_t NET_MAX_PORT_DIGITS = 5;
constexpr int    NET_MAX_PORT_VALUE  = 65535;

/**
 * @brief Parses a TCP port number: digits only, 0-65535.
 *
 * @param sz_Text  The text to parse, e.g. "7379".
 * @param ptr_Port Receives the port on success; unchanged on failure.
 *
 * @return P_OK; -P_EINVARG if empty or not only digits; -P_ERANGE if above 65535.
 */
int net_parsePort(const char* sz_Src, uint16_t* ptr_Port) {
    if (sz_Src == nullptr || sz_Src[0] == '\0') {
        return -P_EINVARG;
    }

    int    int_Port      = 0;
    size_t siz_NumDigits = 0;

    for (const char* p = sz_Src; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return -P_EINVARG;
        }
        if (++siz_NumDigits > NET_MAX_PORT_DIGITS) {
            return -P_EINVARG;
        }
        int_Port = int_Port * 10 + (*p - '0');
    }

    if (int_Port > NET_MAX_PORT_VALUE) {
        return -P_ERANGE;
    }

    *ptr_Port = static_cast<uint16_t>(int_Port);
    return P_OK;
}

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

}  // namespace pumpkin::services
