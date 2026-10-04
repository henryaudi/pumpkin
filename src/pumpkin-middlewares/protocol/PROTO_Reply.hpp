#ifndef PUMPKIN_PROTO_REPLY_HPP_
#define PUMPKIN_PROTO_REPLY_HPP_

#include <cstdint>
#include <string>

namespace pumpkin::middlewares {

/**
 * @brief The kinds of reply a command can produce.
 */
enum class ReplyType { REPLY_OK, REPLY_PONG, REPLY_VALUE, REPLY_NIL, REPLY_INTEGER, REPLY_ERROR };

struct Reply {
    ReplyType    enm_Type = ReplyType::REPLY_OK;
    std::string  text;
    std::int64_t integer = 0;
};

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PROTO_REPLY_HPP_
