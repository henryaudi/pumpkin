/**
 ******************************************************************************
 * @file    PWP_Protocol.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Wire Protocol (PWP): the protocol layer NET talks to.
 *          This file declares:
 *           + PwpConnState: per-connection protocol state (text or binary)
 *           + PWP_ProcessInput: requests in, replies out, for either protocol
 *           + PWP_ProcessLine: one text line, from parsing to the encoded reply
 *           + PWP_RequestToCommand / PWP_ReplyToPbwp: conversions shared by both protocols
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

#ifndef PUMPKIN_PWP_PROTOCOL_HPP_
#define PUMPKIN_PWP_PROTOCOL_HPP_

#include <cstddef>
#include <cstdint>
#include <string>

#include "applications/pump-app-eng/ENG_KvStore.hpp"
#include "services/pump-srv-cde/CDE_Command.hpp"
#include "services/pump-srv-cde/CDE_Reply.hpp"
#include ""

#endif  // PUMPKIN_PWP_PROTOCOL_HPP_
