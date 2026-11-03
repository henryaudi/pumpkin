/**
 ******************************************************************************
 * @file    CDE_Dispatcher.hpp
 * @author  Shangjie Zheng
 * @brief   Command Dispatch Engine (CDE): public interface.
 *          This file declares functions to:
 *           + Execute a parsed Command against a KvStore
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

#ifndef PUMPKIN_CDE_DISPATCHER_HPP_
#define PUMPKIN_CDE_DISPATCHER_HPP_

#include <string>

#include "core/pump-core-eng/ENG_KvStore.hpp"
#include "middlewares/pump-mid-cde/CDE_Command.hpp"
#include "middlewares/pump-mid-cde/CDE_Reply.hpp"

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

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_CDE_DISPATCHER_HPP_
