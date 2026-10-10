#include "common/pump-cmn-stat/STAT_Status.hpp"

#include <gtest/gtest.h>

#include <cerrno>
#include <string>

namespace pumpkin::common {
namespace {

TEST(StatusToString, DescribesKnownCodes) {
    EXPECT_EQ(std::string(STAT_ToString(P_OK)), "success");
    EXPECT_EQ(std::string(STAT_ToString(P_EINVARG)), "invalid argument");
    EXPECT_EQ(std::string(STAT_ToString(P_EADDRINUSE)), "address already in use");
}

TEST(StatusToString, UnknownCodeIsReported) {
    EXPECT_EQ(std::string(STAT_ToString(12345)), "unknown status");
}

TEST(StatusMapErrno, KnownErrnoGetsItsOwnCode) {
    EXPECT_EQ(STAT_MapErrno(EADDRINUSE), P_EADDRINUSE);
    EXPECT_EQ(STAT_MapErrno(EACCES), P_EACCES);
    EXPECT_EQ(STAT_MapErrno(ECONNRESET), P_ECONNRESET);
    EXPECT_EQ(STAT_MapErrno(EPIPE), P_EPIPE);
}

TEST(StatusMapErrno, OtherErrnoBecomesIoError) {
    EXPECT_EQ(STAT_MapErrno(EBADF), P_EIO);
    EXPECT_EQ(STAT_MapErrno(ENOTSOCK), P_EIO);
}

}  // namespace
}  // namespace pumpkin::common
