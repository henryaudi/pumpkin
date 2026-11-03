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

#include "applications/pump-app-eng/ENG_KvStore.hpp"
#include "middlewares/pump-mid-pbwp/PBWP_Message.hpp"

namespace pumpkin::services {

/**
 * @brief Validates and runs one request against a store.
 *
 * @details Always produces a reply. A bad request (wrong argument count, key too long, unknown
 *          opcode) comes back as RPL_ERROR with a P_E* status and a message. The request id is
 *          copied into the reply.
 *
 * @param str_Request The request: opcode, request id and arguments.
 * @param obj_Store   The store to read from or write to.
 *
 * @return The reply for the client.
 */
middlewares::PbwpReply CDE_ExecuteCommand(const middlewares::PbwpRequest& str_Req,
                                          applications::KvStore&          obj_Store);

}  // namespace pumpkin::services

#endif  // PUMPKIN_CDE_DISPATCHER_HPP_
