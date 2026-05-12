/*
 * test_doorbell_emu.cpp — TDD: DoorbellEmu 完整功能测试
 *
 * 测试 ADR-021 §6 定义的 DoorbellEmu 行为：
 * - CPU 通过 PCIe write 写入 doorbell (write())
 * - Puller 轮询检测 (poll())
 * - Puller 消费 doorbell (acknowledge())
 * - Doorbell 回调通知 Puller (setCallback())
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>
#include <atomic>

#include "gpu_driver/sim/hardware/doorbell_emu.h"

int test_doorbell_write_and_get_count() {
  DoorbellEmu db;

  // 初始计数为 0
  if (db.getRingCount(0) != 0) {
    std::cerr << "FAIL: initial count should be 0\n";
    return 1;
  }

  // write() 增加计数
  db.write(0);
  if (db.getRingCount(0) != 1) {
    std::cerr << "FAIL: count should be 1 after one write\n";
    return 1;
  }

  db.write(0);
  db.write(0);
  if (db.getRingCount(0) != 3) {
    std::cerr << "FAIL: count should be 3 after three writes\n";
    return 1;
  }

  // 不同 queue_id 独立计数
  db.write(1);
  if (db.getRingCount(0) != 3 || db.getRingCount(1) != 1) {
    std::cerr << "FAIL: queue counts should be independent\n";
    return 1;
  }

  std::cout << "PASS: test_doorbell_write_and_get_count\n";
  return 0;
}

int test_doorbell_poll_and_acknowledge() {
  DoorbellEmu db;

  // 初始 poll 应返回 false
  if (db.poll(0) != false) {
    std::cerr << "FAIL: poll should return false when no doorbell written\n";
    return 1;
  }

  // write() 后 poll 应返回 true
  db.write(0);
  if (db.poll(0) != true) {
    std::cerr << "FAIL: poll should return true after write\n";
    return 1;
  }

  // acknowledge() 后 poll 应返回 false
  db.acknowledge(0);
  if (db.poll(0) != false) {
    std::cerr << "FAIL: poll should return false after acknowledge\n";
    return 1;
  }

  // acknowledge 只清除对应 queue
  db.write(0);
  db.write(1);
  db.acknowledge(0);
  if (db.poll(0) != false || db.poll(1) != true) {
    std::cerr << "FAIL: acknowledge should only clear specified queue\n";
    return 1;
  }

  std::cout << "PASS: test_doorbell_poll_and_acknowledge\n";
  return 0;
}

int test_doorbell_callback() {
  DoorbellEmu db;
  std::atomic<int> callback_count(0);
  std::atomic<int> last_queue_id(-1);

  // 注册回调
  db.setCallback([&](u32 queue_id) {
    callback_count.fetch_add(1);
    last_queue_id.store(static_cast<int>(queue_id));
  });

  // write() 触发回调
  db.write(5);
  if (callback_count.load() != 1) {
    std::cerr << "FAIL: callback should be called once after write\n";
    return 1;
  }
  if (last_queue_id.load() != 5) {
    std::cerr << "FAIL: callback should receive correct queue_id\n";
    return 1;
  }

  // 再写一次
  db.write(3);
  if (callback_count.load() != 2 || last_queue_id.load() != 3) {
    std::cerr << "FAIL: second callback should update values\n";
    return 1;
  }

  std::cout << "PASS: test_doorbell_callback\n";
  return 0;
}

int test_doorbell_out_of_bounds() {
  DoorbellEmu db;

  // 超出范围的 queue_id 应该被忽略
  db.write(9999);  // >= MAX_QUEUES = 1024
  db.write(1024);   // 边界情况

  // 不应该崩溃，计数保持为 0
  if (db.getRingCount(9999) != 0 || db.getRingCount(1024) != 0) {
    std::cerr << "FAIL: out-of-bounds write should be ignored\n";
    return 1;
  }

  // poll 超出范围应返回 false
  if (db.poll(9999) != false || db.poll(1024) != false) {
    std::cerr << "FAIL: poll out-of-bounds should return false\n";
    return 1;
  }

  std::cout << "PASS: test_doorbell_out_of_bounds\n";
  return 0;
}

int main() {
  int result = 0;

  std::cout << "=== DoorbellEmu TDD Tests ===\n";

  result |= test_doorbell_write_and_get_count();
  result |= test_doorbell_poll_and_acknowledge();
  result |= test_doorbell_callback();
  result |= test_doorbell_out_of_bounds();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== SOME TESTS FAILED ===\n";
  }

  return result;
}
