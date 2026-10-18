/**
 ******************************************************************************
 * @file    PWP_Command.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Wire Protocol (PWP) command types.
 *          This file defines:
 *           + CommandType: the commands Pumpkin understands
 *           + Command: a parsed client request
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

#ifndef PUMPKIN_PWP_COMMAND_HPP_
#define PUMPKIN_PWP_COMMAND_HPP_

#include <string>
#include <vector>

namespace pumpkin::middlewares {

/**
 * @brief The commands Pumpkin understands.
 */
enum class CommandType { CMD_PING, CMD_SET, CMD_GET, CMD_DEL };

/**
 * @brief A parsed client request.
 */
struct Command {
    CommandType              enm_Type = CommandType::CMD_PING;  // Default command type PING
    std::vector<std::string> vec_Args;  // Arguments associated with the command
};

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PWP_COMMAND_HPP_
