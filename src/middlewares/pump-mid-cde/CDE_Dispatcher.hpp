/**
 ******************************************************************************
 * @file    CDE_Dispatcher.hpp
 * @author  Shangjie Zheng
 * @brief   Command Dispatch Engine (CDE): public interface.
 *          This file declares functions to:
 *           + Execute a parsed Command against a KvStore
 *           + Process one PWP line end to end (parse, execute, encode)
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

#ifndef PUMPKIN_CDE_DISPATCHER_HPP_
#define PUMPKIN_CDE_DISPATCHER_HPP_

#include <string>

#include "core/pump-core-eng/ENG_KvStore.hpp"
#include "middlewares/pump-mid-pwp/PWP_Command.hpp"
#include "middlewares/pump-mid-pwp/PWP_Reply.hpp"

namespace pumpkin::middlewares {

/**
 * @brief Runs one command against a store.
 *
 * @param str_Command The command to run.
 * @param obj_Store   The store to read from or write to.
 *
 * @return The reply for the client.
 */
Reply CDE_ExecuteCommand(const Command& str_Command, core::KvStore& obj_Store);

/**
 * @brief Handles one line of the text protocol from start to finish.
 *
 * @details Parses the line, runs the command, and encodes the reply. Invalid lines produce an
 *          "ERR ..." reply. Empty or whitespace-only lines are ignored.
 *
 * @param sz_Line   One line from the client, without its line ending.
 * @param obj_Store The store to run the command against.
 *
 * @return The reply text including "\n", or an empty string for an blank line.
 */
std::string CDE_ProcessLine(const std::string& sz_Line, core::KvStore& obj_Store);

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_CDE_DISPATCHER_HPP_
