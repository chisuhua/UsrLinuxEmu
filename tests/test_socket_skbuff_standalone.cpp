/*
 * test_socket_skbuff_standalone.cpp — Stage 2.2.1 network compat layer
 *
 * Per ADR-038 D1/D2:
 * - D1: sk_buff opaque token + 6 ops (alloc/free/data/len/ref_up/ref_down)
 * - D2: L2 pass-through scope (socket/bind/sendto/recvfrom/close)
 *
 * Tests cover:
 * 1. sk_buff alloc/free/refcount
 * 2. socket/bind/sendto/recvfrom loopback (L2)
 * 3. error paths (null args, unbound socket, EAGAIN)
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include <kernel/net/sk_buff.h>
#include <kernel/net/socket.h>
}

TEST_CASE("sk_buff — alloc/free round-trip (D1 ops)",
          "[kernel][net][sk_buff][stage22]")
{
  auto* ops = sk_buff_ops_get();
  REQUIRE(ops != nullptr);
  REQUIRE(ops->alloc != nullptr);
  REQUIRE(ops->free != nullptr);

  struct sk_buff* skb = ops->alloc(64);
  REQUIRE(skb != nullptr);
  CHECK(ops->len(skb) == 64);
  CHECK(sk_buff_get_refcount(skb) == 1);

  ops->free(skb);
}

TEST_CASE("sk_buff — refcount via ref_up/ref_down",
          "[kernel][net][sk_buff][stage22][refcount]")
{
  auto* ops = sk_buff_ops_get();
  struct sk_buff* skb = ops->alloc(32);
  REQUIRE(skb != nullptr);
  CHECK(sk_buff_get_refcount(skb) == 1);

  ops->ref_up(skb);
  CHECK(sk_buff_get_refcount(skb) == 2);

  ops->ref_down(skb);
  CHECK(sk_buff_get_refcount(skb) == 1);

  ops->free(skb);
}

TEST_CASE("sk_buff — data pointer accessible for read/write",
          "[kernel][net][sk_buff][stage22][data]")
{
  auto* ops = sk_buff_ops_get();
  struct sk_buff* skb = ops->alloc(16);
  REQUIRE(skb != nullptr);

  char* data = static_cast<char*>(ops->data(skb));
  data[0] = 'h';
  data[1] = 'i';
  CHECK(static_cast<char*>(ops->data(skb))[0] == 'h');

  ops->free(skb);
}

TEST_CASE("sk_buff — zero-length alloc returns NULL",
          "[kernel][net][sk_buff][stage22][guard]")
{
  auto* ops = sk_buff_ops_get();
  CHECK(ops->alloc(0) == nullptr);
  ops->free(nullptr);  /* void return; must not crash */
  CHECK(ops->data(nullptr) == nullptr);
  CHECK(ops->len(nullptr) == 0);
}

TEST_CASE("socket — bind + sendto + recvfrom loopback (D2 L2)",
          "[kernel][net][socket][stage22][loopback]")
{
  int server = us_net_socket(0, 0, 0);
  int client = us_net_socket(0, 0, 0);
  REQUIRE(server > 0);
  REQUIRE(client > 0);

  CHECK(us_net_bind(server, 0x7f000001, 8080) == 0);
  CHECK(us_net_bind(client, 0x7f000001, 9090) == 0);

  const char* msg = "hello";
  int sent = us_net_sendto(client, msg, 6, 0x7f000001, 8080);
  CHECK(sent == 6);

  char buf[64] = {};
  unsigned long src_addr = 0;
  unsigned short src_port = 0;
  int recv = us_net_recvfrom(server, buf, sizeof(buf), &src_addr, &src_port);
  CHECK(recv == 6);
  CHECK(std::string(buf) == "hello");
  CHECK(src_addr == 0x7f000001);
  CHECK(src_port == 9090);

  us_net_close(server);
  us_net_close(client);
}

TEST_CASE("socket — recvfrom on empty queue returns -EAGAIN",
          "[kernel][net][socket][stage22][eagain]")
{
  int fd = us_net_socket(0, 0, 0);
  REQUIRE(fd > 0);
  CHECK(us_net_bind(fd, 0x7f000001, 8081) == 0);

  char buf[16];
  int rc = us_net_recvfrom(fd, buf, sizeof(buf), nullptr, nullptr);
  CHECK(rc == -11);  /* -EAGAIN */

  us_net_close(fd);
}

TEST_CASE("socket — sendto on unbound socket fails",
          "[kernel][net][socket][stage22][guard]")
{
  int fd = us_net_socket(0, 0, 0);
  REQUIRE(fd > 0);

  const char* msg = "x";
  int rc = us_net_sendto(fd, msg, 1, 0x7f000001, 8080);
  CHECK(rc != 0);

  us_net_close(fd);
}

TEST_CASE("socket — null arg guards",
          "[kernel][net][socket][stage22][null_guard]")
{
  CHECK(us_net_sendto(-1, nullptr, 0, 0, 0) != 0);
  CHECK(us_net_recvfrom(-1, nullptr, 0, nullptr, nullptr) != 0);
  CHECK(us_net_bind(-1, 0, 0) != 0);
  CHECK(us_net_close(-1) != 0);
}