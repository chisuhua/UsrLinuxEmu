#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace usr_linux_emu {

class WaitQueue {
 public:
  void wait();
  void wake_up();
  bool wait_for(int timeout_ms);

 private:
  std::mutex mtx_;
  std::condition_variable cv_;
  bool flag_ = false;
};

}  // namespace usr_linux_emu