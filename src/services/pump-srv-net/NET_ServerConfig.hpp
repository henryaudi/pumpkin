/**
 ******************************************************************************
 * @file    NET_ServerConfig.hpp
 * @author  Shangjie Zheng
 * @brief   TCP server configuration: public interface.
 *          This file declares:
 *           + NetServerConfig: the address the server listens on, with NET's defaults
 *           + NET_ParseArgs: reads NET's own command-line flags (--bind, --port)
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

#ifndef PUMPKIN_NET_SERVERCONFIG_HPP_
#define PUMPKIN_NET_SERVERCONFIG_HPP_

#include <cstdint>
#include <string>

namespace pumpkin::services {

inline constexpr const char NET_DEFAULT_BIND_IP[] = "127.0.0.1";
inline constexpr uint16_t   NET_DEFAULT_PORT      = 7379;

struct NetServerConfig {
    std::string sz_BindIp = NET_DEFAULT_BIND_IP;
    uint16_t    u16_Port  = NET_DEFAULT_PORT;
};

/**
 * @brief Sets the port the app's server will listen on.
 *
 * @param u16_Port The port number to set.
 *
 * @return P_OK on success;
 */
int NET_SetPort(uint16_t u16_Port);

/**
 * @brief Sets the IPv4 address the app's server will listen to.
 *
 * @param sz_Ip Dotted IPv4 address string, e.g. "127.0.0.1".
 *
 * @return P_OK on success; -P_EINVARG for NULL or an invalid address (the setting is left
 *         unchanged).
 */
int NET_SetBindIp(const char* sz_Ip);

/**
 * @brief Retrieves the current server configuration.
 *
 * @return A const reference to the NetServerConfig structure.
 */
const NetServerConfig& NET_GetServerConfig();

}  // namespace pumpkin::services

#endif  // PUMPKIN_NET_SERVERCONFIG_HPP_
