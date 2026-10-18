/**
 ******************************************************************************
 * @file    PWP_Reply.hpp
 * @author  Shangjie Zheng
 * @brief   Pumpkin Wire Protocol (PWP) reply types.
 *          This file defines:
 *           + ReplyType: the kinds of reply a command can produce
 *           + Reply: the result of a command
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

#ifndef PUMPKIN_PWP_REPLY_HPP_
#define PUMPKIN_PWP_REPLY_HPP_

#include <cstdint>
#include <string>

namespace pumpkin::middlewares {

/**
 * @brief The kinds of reply a command can produce.
 */
enum class ReplyType { REPLY_OK, REPLY_PONG, REPLY_VALUE, REPLY_NIL, REPLY_INTEGER, REPLY_ERROR };

struct Reply {
    ReplyType   enm_Type = ReplyType::REPLY_OK;
    std::string sz_Text;
    int64_t     s64_Integer = 0;
};

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PWP_REPLY_HPP_
