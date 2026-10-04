#ifndef PUMPKIN_PROTO_COMMAND_HPP_
#define PUMPKIN_PROTO_COMMAND_HPP_

#include <string>

namespace pumpkin::middlewares {

/**
 * @brief The commands Pumpkin understands.
 */
enum class CommandType { CMD_PING, CMD_SET, CMD_GET, CMD_DEL };

/**
 * @brief A parsed client request.
 */
struct Command {
    CommandType enm_Type = CommandType::CMD_PING; /* Default */
    std::string sz_Key;
    std::string sz_Value;
};

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PROTO_COMMAND_HPP_
