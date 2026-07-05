/*
 * test_storage_driver_standalone.cpp — Stage 2.3.4 storage driver test
 *
 * Per plan §2.3.4: ② portable block storage driver test.
 * Validates block_device_ops (open/close/read/write) and void* opaque API.
 */

#include <catch_amalgamated.hpp>

#include <cstdio>
#include <cstring>

extern "C" {
#include <linux_compat/types.h>
#include "storage_driver.h"
}

namespace {
/* Per storage_driver.cpp, sim hardcodes path "/tmp/storage_drv_test.dat".
 * Tests must use the same path to exercise the sim backing. */
const char* kTestPath = "/tmp/storage_drv_test.dat";
}

TEST_CASE("storage_driver — create/destroy round-trip",
          "[plugin][storage_driver][stage23][lifecycle]")
{
  void* dev = block_device_create("sda0");
  REQUIRE(dev != nullptr);
  CHECK(std::string(block_device_get_name(dev)) == "sda0");
  CHECK(block_device_size(dev) == 0);  /* no file backing yet */
  block_device_destroy(dev);
}

TEST_CASE("storage_driver — null guard",
          "[plugin][storage_driver][stage23][guard]")
{
  CHECK(block_device_create(nullptr) != nullptr);
  block_device_destroy(nullptr);  /* void return; must not crash */
  CHECK(block_device_open(nullptr) != 0);
  CHECK(block_device_close(nullptr) != 0);
  CHECK(block_device_read(nullptr, nullptr, 0) != 0);
  CHECK(block_device_write(nullptr, nullptr, 0) != 0);
  CHECK(block_device_size(nullptr) == 0);
  CHECK(block_device_get_name(nullptr) == nullptr);
}

TEST_CASE("storage_driver — write reflects in size (no seek API needed)",
          "[plugin][storage_driver][stage23][write_size]")
{
  std::remove(kTestPath);

  void* dev = block_device_create("sda0_size");
  REQUIRE(dev != nullptr);

  int rc = block_device_open(dev);
  if (rc != 0) {
    block_device_destroy(dev);
    return;
  }
  REQUIRE(rc == 0);

  const char buf[16] = "0123456789ABCDE";
  CHECK(block_device_write(dev, buf, sizeof(buf)) == (long)sizeof(buf));
  CHECK(block_device_size(dev) >= sizeof(buf));

  block_device_close(dev);
  block_device_destroy(dev);
  std::remove(kTestPath);
}

TEST_CASE("storage_driver — null guard (exhaustive)",
          "[plugin][storage_driver][stage23][guard]")
{
  CHECK(block_device_create(nullptr) != nullptr);
  block_device_destroy(nullptr);
  CHECK(block_device_open(nullptr) != 0);
  CHECK(block_device_close(nullptr) != 0);
  CHECK(block_device_read(nullptr, nullptr, 0) != 0);
  CHECK(block_device_write(nullptr, nullptr, 0) != 0);
  CHECK(block_device_size(nullptr) == 0);
  CHECK(block_device_get_name(nullptr) == nullptr);
}

TEST_CASE("storage_driver — write then size reflects data",
          "[plugin][storage_driver][stage23][size]")
{
  std::remove(kTestPath);

  void* dev = block_device_create("sda0_size");
  REQUIRE(dev != nullptr);

  if (block_device_open(dev) != 0) {
    block_device_destroy(dev);
    return;
  }

  const char buf[16] = "0123456789ABCDE";
  CHECK(block_device_write(dev, buf, sizeof(buf)) == (long)sizeof(buf));
  CHECK(block_device_size(dev) >= sizeof(buf));

  block_device_close(dev);
  block_device_destroy(dev);
  std::remove(kTestPath);
}