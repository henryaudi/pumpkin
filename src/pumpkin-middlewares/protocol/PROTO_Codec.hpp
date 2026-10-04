#ifndef PUMPKIN_PROTO_CODEC_HPP_
#define PUMPKIN_PROTO_CODEC_HPP_

#include <cstddef>
#include <optional>
#include <string>

#include "pumpkin-middlewares/protocol/PROTO_Command.hpp"
#include "pumpkin-middlewares/protocol/PROTO_Reply.hpp"

namespace pumpkin::middlewares {

/* Longest line accepted in bytes. Longer inputs would result in connection closure */
constexpr std::size_t PROTO_MAX_LINE_LENGTH = 64 * 1024;

}  // namespace pumpkin::middlewares

#endif  // PUMPKIN_PROTO_CODEC_HPP_
