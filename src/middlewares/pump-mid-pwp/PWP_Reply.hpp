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
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
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
    ReplyType    enm_Type = ReplyType::REPLY_OK;
    std::string  sz_Text;
    std::int64_t s64_Integer = 0;
};

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PWP_REPLY_HPP_
