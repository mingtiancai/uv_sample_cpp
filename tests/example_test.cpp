#include <gtest/gtest.h>

#include <uv.h>

#include <cstring>

// Placeholder test that also proves libuv is linkable from the test target.
// Replace / extend these as project logic is factored into testable libraries.

namespace {

TEST(SanityTest, BasicArithmetic) {
  EXPECT_EQ(2 + 2, 4);
}

TEST(LibuvTest, Ip4AddrParsesValidAddress) {
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));

  const int err = uv_ip4_addr("127.0.0.1", 9123, &addr);

  EXPECT_EQ(err, 0) << uv_strerror(err);
  EXPECT_EQ(addr.sin_family, AF_INET);
}

TEST(LibuvTest, Ip4AddrRejectsInvalidAddress) {
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));

  const int err = uv_ip4_addr("not-an-ip", 9123, &addr);

  EXPECT_NE(err, 0);
}

}  // namespace
