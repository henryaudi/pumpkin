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
 * @brief Applies NET's command-line flags to a config: --bind <ipv4> and --port <0-65535>.
 *
 * @details Flags NET doesn't know are skipped, so other modules can have their own on the same
 *          command line. Temporary home for NET's flags until a configuration manager parses the
 *          command line for every module.
 *
 * @param argc       Argument count, as given to main().
 * @param argv       Arguments, as given to main(); argv[0] (the program name) is skipped.
 * @param ptr_Config Updated with the flags found. Unchanged on failure.
 *
 * @return P_OK; -P_EINVARG for a flag without a value, a non-numeric port or an invalid IPv4
 *         address; -P_ERANGE for a port above 65535.
 */
int NET_ParseArgs(int argc, char* argv[], NetServerConfig* ptr_Config);

}  // namespace pumpkin::services

#endif  // PUMPKIN_NET_SERVERCONFIG_HPP_
