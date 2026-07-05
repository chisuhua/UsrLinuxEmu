/*
 * test_net_driver_standalone.cpp — Stage 2.2.4 network driver test
 *
 * Per ADR-038: tests cover 3-way separation behavior at the net_device
 * ② portable driver interface.
 */

#include <catch_amalgamated.hpp>

#include <cstring>

extern "C" {
#include <linux_compat/types.h>
#include <kernel/net/sk_buff.h>
#include <kernel/net/socket.h>
#include "net_driver.h"
}

TEST_CASE("net_driver — create/destroy round-trip",
          "[plugin][net_driver][stage22][lifecycle]")
{
  void* dev = net_device_create("net0");
  REQUIRE(dev != nullptr);
  CHECK(std::string(net_device_get_name(dev)) == "net0");
  CHECK(net_device_nic_tx_count(dev) == 0);
  net_device_destroy(dev);
}

TEST_CASE("net_driver — ndo_start_xmit via sk_buff ops (D1)",
          "[plugin][net_driver][stage22][sk_buff][xmit]")
{
  void* dev = net_device_create("net0");
  REQUIRE(dev != nullptr);
  REQUIRE(net_device_open(dev) == 0);

  const char payload[] = "hello-net";
  const unsigned long plen = std::strlen(payload);

  auto* ops = sk_buff_ops_get();
  struct sk_buff* skb = ops->alloc(plen);
  REQUIRE(skb != nullptr);
  std::memcpy(ops->data(skb), payload, plen);

  int rc = net_device_xmit(dev, skb);
  CHECK(rc == 0);
  CHECK(net_device_nic_tx_count(dev) == 1);

  char buf[64] = {};
  int recv_n = net_device_recv(dev, buf, sizeof(buf));
  CHECK(recv_n == (int)plen);
  CHECK(std::string(buf) == "hello-net");

  ops->free(skb);
  net_device_close(dev);
  net_device_destroy(dev);
}

TEST_CASE("net_driver — recv on empty queue returns -EAGAIN",
          "[plugin][net_driver][stage22][eagain]")
{
  void* dev = net_device_create("net0");
  REQUIRE(dev != nullptr);

  char buf[16];
  int rc = net_device_recv(dev, buf, sizeof(buf));
  CHECK(rc == -11);

  net_device_destroy(dev);
}

TEST_CASE("net_driver — xmit with null device returns -EINVAL",
          "[plugin][net_driver][stage22][guard]")
{
  auto* ops = sk_buff_ops_get();
  struct sk_buff* skb = ops->alloc(4);
  REQUIRE(skb != nullptr);
  int rc = net_device_xmit(nullptr, skb);
  CHECK(rc == -22);
  ops->free(skb);
}

TEST_CASE("net_driver — multiple xmit captures all packets",
          "[plugin][net_driver][stage22][multi_xmit]")
{
  void* dev = net_device_create("net0");
  REQUIRE(dev != nullptr);
  REQUIRE(net_device_open(dev) == 0);
  net_device_nic_reset(dev);

  auto* ops = sk_buff_ops_get();
  for (int i = 0; i < 3; ++i) {
    struct sk_buff* skb = ops->alloc(8);
    REQUIRE(skb != nullptr);
    std::memcpy(ops->data(skb), "pkt-X-_-", 8);
    static_cast<char*>(ops->data(skb))[5] = '0' + i;
    CHECK(net_device_xmit(dev, skb) == 0);
    ops->free(skb);
  }
  CHECK(net_device_nic_tx_count(dev) == 3);

  for (int i = 0; i < 3; ++i) {
    char buf[16] = {};
    int n = net_device_recv(dev, buf, sizeof(buf));
    CHECK(n == 8);
    CHECK(buf[5] == ('0' + i));
  }

  net_device_close(dev);
  net_device_destroy(dev);
}

TEST_CASE("net_driver — recv buffer truncation",
          "[plugin][net_driver][stage22][truncation]")
{
  void* dev = net_device_create("net0");
  REQUIRE(dev != nullptr);
  REQUIRE(net_device_open(dev) == 0);
  net_device_nic_reset(dev);

  auto* ops = sk_buff_ops_get();
  struct sk_buff* skb = ops->alloc(64);
  REQUIRE(skb != nullptr);
  std::memset(ops->data(skb), 0xAA, 64);
  CHECK(net_device_xmit(dev, skb) == 0);
  ops->free(skb);

  char buf[16] = {};
  int n = net_device_recv(dev, buf, sizeof(buf));
  CHECK(n == 16);

  net_device_close(dev);
  net_device_destroy(dev);
}